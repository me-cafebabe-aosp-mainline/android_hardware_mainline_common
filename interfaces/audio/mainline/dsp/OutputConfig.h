/*
 * SPDX-FileCopyrightText: The LineageOS Project
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <array>
#include <optional>
#include <string>
#include <vector>

namespace aidl::android::hardware::audio::core::mainline::dsp {

// Speaker positions the output processing knows about.
enum class Role {
    kFrontLeft,
    kFrontRight,
    kRearLeft,
    kRearRight,
    kCenter,
    kLfe,
    kSideLeft,
    kSideRight,
    kOther,  // Index masks and positions without a speaker of their own.
};

// The speaker setup of an output. kAuto plays the stream's channels as they
// are, like without processing.
enum class Layout { kAuto, kStereo, kQuad, k5Point1, k7Point1 };

constexpr size_t kMaxChannels = 8;

// The speakers of `layout` in the order ALSA (and the hardware) expects them:
// FL FR RL RR FC LFE SL SR. Empty for kAuto.
const std::vector<Role>& LayoutRoles(Layout layout);
const char* ToString(Layout layout);
std::optional<Layout> LayoutFromString(const std::string& name);

// How an output endpoint processes what it plays. Kept per endpoint by the
// control interface; the defaults leave the stream untouched.
struct OutputConfig {
    Layout layout = Layout::kAuto;
    // Fill speakers the stream has no channel for (stereo on 5.1: rears from
    // the fronts, center from both, subwoofer from the bass).
    bool fill = false;
    bool swap_left_right = false;
    // Left and right of each speaker pair get their average.
    bool mono = false;
    // The layout's LFE channel drives a subwoofer. Without one, the stream's
    // LFE goes to the front speakers instead.
    bool subwoofer = true;
    // The other speakers can not play bass: below the crossover it goes to
    // the subwoofer. Needs a layout with LFE and `subwoofer`.
    bool small_speakers = false;
    int crossover_hz = 80;
    // Per channel of LayoutRoles(layout); ignored for kAuto.
    std::array<float, kMaxChannels> levels_db{};
    std::array<float, kMaxChannels> delays_ms{};

    static constexpr float kMinLevelDb = -30.0f;
    static constexpr float kMaxLevelDb = 10.0f;
    static constexpr float kMaxDelayMs = 20.0f;
    static constexpr int kMinCrossoverHz = 40;
    static constexpr int kMaxCrossoverHz = 250;

    // Whether processing would leave every stream as it is.
    bool IsIdentity() const;
    // Whether every value is within its range.
    bool IsValid() const;
    // Output channels for a stream of `stream_channels`.
    unsigned int OutputChannels(unsigned int stream_channels) const;

    std::string Serialize() const;
    static std::optional<OutputConfig> Parse(const std::string& text);
    bool operator==(const OutputConfig&) const = default;
};

}  // namespace aidl::android::hardware::audio::core::mainline::dsp
