/*
 * SPDX-FileCopyrightText: The LineageOS Project
 * SPDX-License-Identifier: Apache-2.0
 */

#include "passthrough/Quirks.h"

#include <dirent.h>

#include <algorithm>
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <utility>

#include <android-base/file.h>
#include <android-base/logging.h>
#include <android-base/strings.h>

namespace aidl::android::hardware::audio::core::mainline::passthrough {

namespace {

// HDA: the ELD is a PCM control of the HDMI PCM device, the IEC958 controls
// are mixer controls numbered by HDMI PCM (sound/hda/common/codec.c,
// snd_hda_create_dig_out_ctls). The non-audio bit of the channel status
// selects the non-PCM stream format at prepare time, and high bit rate is
// refused there with -EINVAL when the pin (or, on Tegra 20 / 30, the display
// side) can not do it, which the runtime fallback handles. The HDA codec
// layer adds "HDA:<codec ids>" to the card components, whatever the
// controller driver.
bool IsHda(const alsa::CardInfo& card) {
    return card.driver == "HDA-Intel" || card.driver == "tegra-hda" ||
           card.components.find("HDA:") != std::string::npos;
}

// What the HDA HDMI pins of a card can do, from the codec proc files.
struct HdaPins {
    int hdmi = 0;  // Pins with HDMI capability.
    int hbr = 0;   // Of those, the ones with high bit rate capability.
};

// sound/pci/hda/hda_proc.c (sound/hda/common/proc.c in newer kernels) prints
// a "Pincap 0x...:" line per pin, with " HDMI" for an HDMI capable pin,
// preceded by " HBR" when it can also do high bit rate (AC_PINCAP_HBR, which
// hdmi_pin_hbr_setup() checks at prepare time). Realtek codecs reuse the
// HDMI bit and get " R/L" instead, so they never count.
HdaPins ReadHdaPins(int card) {
    HdaPins pins;
    const std::string dir = "/proc/asound/card" + std::to_string(card);
    std::unique_ptr<DIR, decltype(&closedir)> handle(opendir(dir.c_str()), &closedir);
    if (handle == nullptr) return pins;
    while (const dirent* entry = readdir(handle.get())) {
        const std::string name = entry->d_name;
        if (!::android::base::StartsWith(name, "codec#")) continue;
        std::string contents;
        if (!::android::base::ReadFileToString(dir + "/" + name, &contents)) continue;
        for (const std::string& line : ::android::base::Split(contents, "\n")) {
            const std::string trimmed = ::android::base::Trim(line);
            if (!::android::base::StartsWith(trimmed, "Pincap 0x")) continue;
            const size_t colon = trimmed.find(':');
            if (colon == std::string::npos) continue;
            const std::vector<std::string> caps =
                    ::android::base::Split(::android::base::Trim(trimmed.substr(colon + 1)), " ");
            const auto has = [&caps](const char* cap) {
                return std::find(caps.begin(), caps.end(), cap) != caps.end();
            };
            if (!has("HDMI")) continue;
            ++pins.hdmi;
            if (has("HBR")) ++pins.hbr;
        }
    }
    return pins;
}

// What the HDMI pins of an HDA card say about high bit rate, for the
// diagnostics only. Mainline refuses high bit rate at prepare time on a pin
// without AC_PINCAP_HBR, but the bit is not the truth: Tegra 210 sends high
// bit rate with NVIDIA's downstream kernel although its pin does not report
// it, so a fixed kernel may accept it. The runtime fallback decides; this
// tells why it may refuse.
std::string HdaPinDetail(const alsa::CardInfo& card) {
    static std::mutex lock;
    static std::map<std::pair<int, std::string>, HdaPins> cache;
    HdaPins pins;
    {
        std::lock_guard guard(lock);
        const auto key = std::make_pair(card.index, card.id);
        auto it = cache.find(key);
        if (it == cache.end()) {
            it = cache.emplace(key, ReadHdaPins(card.index)).first;
            LOG(INFO) << __func__ << ": card " << card.index << " [" << card.id
                      << "]: " << it->second.hdmi << " HDMI pin(s), " << it->second.hbr
                      << " reporting HBR";
        }
        pins = it->second;
    }
    if (pins.hdmi == 0) return "pin capabilities unknown";
    if (pins.hbr == 0) return "no HDMI pin reports HBR, the kernel may refuse it";
    if (pins.hbr < pins.hdmi) return "HBR reported on some HDMI pins";
    return "HDMI pins report HBR";
}

}  // namespace

const char* ToString(ControlLocator locator) {
    switch (locator) {
        case ControlLocator::kPcmDevice:
            return "pcm-device";
        case ControlLocator::kHdaOrdinal:
            return "hda-ordinal";
        case ControlLocator::kSoleControl:
            return "sole-control";
    }
    return "?";
}

Quirk QuirkFor(const alsa::CardInfo& card) {
    if (IsHda(card)) {
        return Quirk{.name = "hda",
                     .locators = {ControlLocator::kPcmDevice, ControlLocator::kHdaOrdinal},
                     .hbr = HbrSupport::kSupported,
                     .hbr_detail = HdaPinDetail(card)};
    }
    // Anything else, typically ASoC hdmi-codec (e.g. Amlogic dw-hdmi through
    // an I2S bridge that knows nothing about high bit rate): no HDA style
    // numbering, and an HBR attempt may seem to work while sending garbage.
    return Quirk{.name = "default",
                 .locators = {ControlLocator::kPcmDevice, ControlLocator::kSoleControl},
                 .hbr = HbrSupport::kUndetectable};
}

}  // namespace aidl::android::hardware::audio::core::mainline::passthrough
