/*
 * SPDX-FileCopyrightText: The LineageOS Project
 * SPDX-License-Identifier: Apache-2.0
 */

#define LOG_TAG "MainlineAudio_HdaFrontPanel"

#include "alsa/HdaFrontPanel.h"

#include <dirent.h>
#include <fcntl.h>
#include <unistd.h>

#include <cerrno>
#include <chrono>
#include <cinttypes>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <map>
#include <string>
#include <thread>
#include <vector>

#include <android-base/file.h>
#include <android-base/logging.h>
#include <android-base/stringprintf.h>
#include <android-base/strings.h>
#include <android-base/unique_fd.h>

namespace aidl::android::hardware::audio::core::mainline::alsa {

namespace {

using ::android::base::StringPrintf;
using PinConfigs = std::map<int, uint32_t>;  // Pin widget nid -> default configuration.

// Codec addresses on an HD Audio link.
constexpr int kMaxCodecs = 16;

// Fields of a pin's default configuration (include/sound/hda_verbs.h).
constexpr int kConnectivityShift = 30;
constexpr uint32_t kConnectivityJack = 0x0;  // AC_JACK_PORT_COMPLEX
constexpr int kLocationShift = 24;
constexpr uint32_t kLocationMask = 0x3f;
constexpr uint32_t kLocationExternalFront = 0x02;  // AC_JACK_LOC_EXTERNAL | AC_JACK_LOC_FRONT
constexpr int kDeviceShift = 20;
constexpr uint32_t kDeviceMask = 0xf;
constexpr uint32_t kDeviceHpOut = 0x2;         // AC_JACK_HP_OUT
constexpr uint32_t kDeviceMicIn = 0xa;         // AC_JACK_MIC_IN
constexpr uint32_t kMiscNoPresence = 1u << 8;  // AC_DEFCFG_MISC_NO_PRESENCE

// ueventd recreates the PCM device nodes after a reconfiguration.
constexpr auto kPcmWait = std::chrono::seconds(3);
constexpr auto kPcmPoll = std::chrono::milliseconds(20);

std::string CodecPath(int card, int codec) {
    return StringPrintf("/sys/class/sound/hwC%dD%d", card, codec);
}

// "0x12 0x90a60140" lines; a missing file reads as empty.
PinConfigs ReadPinConfigs(const std::string& path) {
    PinConfigs pins;
    std::string text;
    if (!::android::base::ReadFileToString(path, &text)) return pins;
    for (const std::string& line : ::android::base::Split(text, "\n")) {
        const std::vector<std::string> fields =
                ::android::base::Split(::android::base::Trim(line), " ");
        if (fields.size() != 2) continue;
        char* end = nullptr;
        const unsigned long nid = std::strtoul(fields[0].c_str(), &end, 0);
        if (*end != '\0' || nid == 0) continue;
        const unsigned long config = std::strtoul(fields[1].c_str(), &end, 0);
        if (*end != '\0') continue;
        pins[static_cast<int>(nid)] = static_cast<uint32_t>(config);
    }
    return pins;
}

// 0 or the errno of the kernel's store.
int WriteAttribute(const std::string& path, const std::string& text) {
    const ::android::base::unique_fd fd(
            TEMP_FAILURE_RETRY(open(path.c_str(), O_WRONLY | O_CLOEXEC)));
    if (fd.get() < 0) return errno;
    if (TEMP_FAILURE_RETRY(write(fd.get(), text.data(), text.size())) < 0) return errno;
    return 0;
}

bool IsFrontPanelJack(uint32_t config) {
    if ((config >> kConnectivityShift) != kConnectivityJack) return false;
    if (((config >> kLocationShift) & kLocationMask) != kLocationExternalFront) return false;
    const uint32_t device = (config >> kDeviceShift) & kDeviceMask;
    return device == kDeviceHpOut || device == kDeviceMicIn;
}

struct PinChange {
    int nid;
    uint32_t from;
    uint32_t to;
};

struct Codec {
    int address;
    std::string path;
    std::vector<PinChange> changes;
};

// /proc/asound lists the new PCM devices at once; their nodes come later.
void WaitForPcmDevices(int card) {
    const std::string proc = StringPrintf("/proc/asound/card%d", card);
    std::vector<std::string> nodes;
    if (std::unique_ptr<DIR, decltype(&closedir)> dir(opendir(proc.c_str()), closedir); dir) {
        while (const dirent* entry = readdir(dir.get())) {
            int device = -1;
            char direction = 0;
            if (std::sscanf(entry->d_name, "pcm%d%c", &device, &direction) == 2 &&
                (direction == 'p' || direction == 'c')) {
                nodes.push_back(StringPrintf("/dev/snd/pcmC%dD%d%c", card, device, direction));
            }
        }
    }
    const auto deadline = std::chrono::steady_clock::now() + kPcmWait;
    for (const std::string& node : nodes) {
        while (access(node.c_str(), R_OK | W_OK) != 0) {
            if (std::chrono::steady_clock::now() > deadline) {
                LOG(WARNING) << __func__ << ": " << node << " is still not accessible";
                return;
            }
            std::this_thread::sleep_for(kPcmPoll);
        }
    }
}

}  // namespace

const char* ToString(FrontPanelResult result) {
    switch (result) {
        case FrontPanelResult::kNoHdaCodec:
            return "no HD Audio codec";
        case FrontPanelResult::kNoFrontJack:
            return "no front panel jack";
        case FrontPanelResult::kUnchanged:
            return "unchanged";
        case FrontPanelResult::kApplied:
            return "applied";
        case FrontPanelResult::kFailed:
            return "failed";
    }
    return "?";
}

FrontPanelResult ConfigureFrontPanel(int card, bool ac97) {
    std::vector<Codec> codecs;
    bool any_front_jack = false;
    for (int address = 0; address < kMaxCodecs; ++address) {
        const std::string path = CodecPath(card, address);
        if (access((path + "/user_pin_configs").c_str(), F_OK) != 0) continue;
        // Driver fixups override the firmware, user_pin_configs both.
        PinConfigs base = ReadPinConfigs(path + "/init_pin_configs");
        for (const auto& [nid, config] : ReadPinConfigs(path + "/driver_pin_configs")) {
            base[nid] = config;
        }
        const PinConfigs user = ReadPinConfigs(path + "/user_pin_configs");
        Codec codec{.address = address, .path = path, .changes = {}};
        for (const auto& [nid, config] : base) {
            if (!IsFrontPanelJack(config)) continue;
            any_front_jack = true;
            const auto it = user.find(nid);
            const uint32_t current = it != user.end() ? it->second : config;
            const uint32_t wanted = ac97 ? config | kMiscNoPresence : config;
            if (current != wanted) codec.changes.push_back({nid, current, wanted});
        }
        codecs.push_back(std::move(codec));
    }
    if (codecs.empty()) return FrontPanelResult::kNoHdaCodec;
    if (!any_front_jack) return FrontPanelResult::kNoFrontJack;

    bool changed = false;
    bool failed = false;
    for (const Codec& codec : codecs) {
        if (codec.changes.empty()) continue;
        changed = true;
        const std::string pins = codec.path + "/user_pin_configs";
        size_t written = 0;
        int err = 0;
        for (; written < codec.changes.size(); ++written) {
            const PinChange& change = codec.changes[written];
            err = WriteAttribute(pins,
                                 StringPrintf("0x%02x 0x%08" PRIx32 "\n", change.nid, change.to));
            if (err != 0) break;
        }
        if (err == 0) {
            LOG(INFO) << __func__ << ": card " << card << " codec " << codec.address << ": "
                      << (ac97 ? "disabling" : "restoring")
                      << " front panel jack detection, reconfiguring";
            err = WriteAttribute(codec.path + "/reconfig", "1\n");
        }
        if (err == 0) continue;
        // The codec still runs with the old pins: undo, so that the next
        // start tries again.
        LOG(ERROR) << __func__ << ": card " << card << " codec " << codec.address << ": "
                   << std::strerror(err) << (err == EINVAL ? " (is the card in use?)" : "");
        for (size_t i = 0; i < written; ++i) {
            const PinChange& change = codec.changes[i];
            WriteAttribute(pins, StringPrintf("0x%02x 0x%08" PRIx32 "\n", change.nid, change.from));
        }
        failed = true;
    }
    if (!changed) return FrontPanelResult::kUnchanged;
    WaitForPcmDevices(card);
    return failed ? FrontPanelResult::kFailed : FrontPanelResult::kApplied;
}

}  // namespace aidl::android::hardware::audio::core::mainline::alsa
