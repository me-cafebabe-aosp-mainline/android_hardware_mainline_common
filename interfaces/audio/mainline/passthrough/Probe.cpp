/*
 * SPDX-FileCopyrightText: The LineageOS Project
 * SPDX-License-Identifier: Apache-2.0
 */

#define LOG_TAG "MainlineAudio_Passthrough"

#include "passthrough/Probe.h"

#include <utility>

#include <android-base/logging.h>

#include "alsa/AlsaPcm.h"
#include "passthrough/Format.h"

namespace aidl::android::hardware::audio::core::mainline::passthrough {

std::vector<ProbeResult> ProbeHead(HdmiControl& control, const routing::Endpoint& head) {
    // The PCM configurations IEC 61937 data travels in: the content rate,
    // four times 48 kHz (E-AC-3, DTS-HD High Resolution), high bit rate.
    static constexpr std::pair<unsigned int, uint32_t> kConfigs[] = {
            {2, 48000},
            {2, 192000},
            {8, 192000},
    };
    std::vector<ProbeResult> results;
    for (const auto& [channels, rate] : kConfigs) {
        ProbeResult result{.channels = channels, .rate = rate, .hbr = channels == 8};
        // As app packed IEC 61937, whose stream is exactly this configuration.
        const auto stream = IecStreamFor(EncodedFormat::kIec61937, rate, channels, true /*hbr*/);
        auto guard = stream.has_value() ? control.SetChannelStatus(NonAudioChannelStatus(*stream))
                                        : nullptr;
        if (guard == nullptr) {
            result.failed_step = "channel status";
            results.push_back(std::move(result));
            continue;
        }
        alsa::PcmConfig config;
        config.format = SND_PCM_FORMAT_S16_LE;
        config.channels = channels;
        config.rate = rate;
        alsa::PcmOpenError error;
        auto pcm = alsa::Pcm::OpenStrict(head.pcm_name, SND_PCM_STREAM_PLAYBACK, config, &error);
        result.accepted = pcm != nullptr;
        if (!result.accepted) {
            result.failed_step = alsa::ToString(error.step);
            result.error = error.error;
        }
        // Closes the device before the guard restores the channel status.
        pcm.reset();
        LOG(INFO) << __func__ << ": " << head.pcm_name << " " << channels << "ch " << rate
                  << "Hz: " << (result.accepted ? "accepted" : error.ToString());
        results.push_back(std::move(result));
    }
    return results;
}

}  // namespace aidl::android::hardware::audio::core::mainline::passthrough
