/*
 * SPDX-FileCopyrightText: The LineageOS Project
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <vector>

#include <alsa/asoundlib.h>

#include "dsp/Biquad.h"
#include "dsp/OutputConfig.h"

namespace aidl::android::hardware::audio::core::mainline::dsp {

// Turns a stream's frames into what an output's speakers get, following an
// OutputConfig: channel layout (upmix / downmix, speaker fill), bass
// management, left / right swap, mono, per speaker level and delay, and a
// test tone. Works in float; reads and writes the stream's sample format.
// One instance per open PCM device, used by the stream's worker thread only.
// No allocation after the first buffer of the largest size.
class OutputProcessor {
  public:
    // `input_roles` are the stream's channels in Android's interleaved order.
    // Returns nullptr for a format it can not process.
    static std::unique_ptr<OutputProcessor> Create(const OutputConfig& config,
                                                   std::vector<Role> input_roles,
                                                   snd_pcm_format_t format, unsigned int rate);

    const OutputConfig& config() const { return config_; }
    unsigned int output_channels() const { return static_cast<unsigned int>(output_roles_.size()); }

    // Plays pink noise on output channel `channel` instead of the stream,
    // or the stream again for nullopt.
    void SetTestTone(std::optional<unsigned int> channel);

    // Processes `frames` frames of `input`. The result, in ALSA channel order,
    // stays valid until the next call.
    const void* Process(const void* input, size_t frames);

  private:
    OutputProcessor(const OutputConfig& config, std::vector<Role> input_roles,
                    std::vector<Role> output_roles, snd_pcm_format_t format, unsigned int rate);

    void BuildMatrix();
    void Mix(size_t frames);
    void ManageBass(size_t frames);
    void SwapAndMono(size_t frames);
    void LevelsAndDelays(size_t frames);
    void TestTone(size_t frames);
    int IndexOf(Role role) const;
    float NextPinkNoise();

    const OutputConfig config_;
    const std::vector<Role> input_roles_;
    const std::vector<Role> output_roles_;
    const snd_pcm_format_t format_;
    const unsigned int rate_;

    // output x input gains.
    std::vector<std::vector<float>> matrix_;
    // Bass management.
    bool bass_to_sub_ = false;
    int lfe_ = -1;
    std::vector<int> mains_;
    std::vector<LinkwitzRiley4> high_passes_;
    LinkwitzRiley4 low_pass_;
    // Levels and delays per output channel.
    std::vector<float> gains_;
    std::vector<size_t> delays_;
    std::vector<std::vector<float>> delay_lines_;
    std::vector<size_t> delay_positions_;

    std::optional<unsigned int> tone_channel_;
    uint32_t noise_state_ = 22222;
    std::array<float, 7> pink_{};

    // Planar scratch buffers, grown on demand.
    std::vector<std::vector<float>> in_;
    std::vector<std::vector<float>> out_;
    std::vector<uint8_t> output_;
};

}  // namespace aidl::android::hardware::audio::core::mainline::dsp
