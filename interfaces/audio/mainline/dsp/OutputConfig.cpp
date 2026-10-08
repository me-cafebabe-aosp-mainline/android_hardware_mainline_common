/*
 * SPDX-FileCopyrightText: The LineageOS Project
 * SPDX-License-Identifier: Apache-2.0
 */

#include "dsp/OutputConfig.h"

#include <cmath>
#include <map>

#include <android-base/parsebool.h>
#include <android-base/parsedouble.h>
#include <android-base/parseint.h>
#include <android-base/strings.h>

namespace aidl::android::hardware::audio::core::mainline::dsp {

namespace {

std::string JoinFloats(const std::array<float, kMaxChannels>& values) {
    std::vector<std::string> parts;
    for (const float v : values) {
        // Two decimals are more than any setting needs, and keep the text stable.
        parts.push_back(std::to_string(std::lround(v * 100.0f) / 100.0f));
    }
    return ::android::base::Join(parts, ",");
}

bool ParseFloats(const std::string& text, std::array<float, kMaxChannels>* values) {
    const std::vector<std::string> parts = ::android::base::Split(text, ",");
    if (parts.size() != kMaxChannels) return false;
    for (size_t i = 0; i < kMaxChannels; ++i) {
        if (!::android::base::ParseFloat(parts[i], &(*values)[i])) return false;
    }
    return true;
}

bool ParseBool(const std::string& text, bool* value) {
    switch (::android::base::ParseBool(text)) {
        case ::android::base::ParseBoolResult::kTrue:
            *value = true;
            return true;
        case ::android::base::ParseBoolResult::kFalse:
            *value = false;
            return true;
        case ::android::base::ParseBoolResult::kError:
            return false;
    }
    return false;
}

}  // namespace

const std::vector<Role>& LayoutRoles(Layout layout) {
    static const std::vector<Role> kNone;
    static const std::vector<Role> kStereo = {Role::kFrontLeft, Role::kFrontRight};
    static const std::vector<Role> kQuad = {Role::kFrontLeft, Role::kFrontRight, Role::kRearLeft,
                                            Role::kRearRight};
    static const std::vector<Role> k51 = {Role::kFrontLeft, Role::kFrontRight, Role::kRearLeft,
                                          Role::kRearRight, Role::kCenter,     Role::kLfe};
    static const std::vector<Role> k71 = {Role::kFrontLeft, Role::kFrontRight, Role::kRearLeft,
                                          Role::kRearRight, Role::kCenter,     Role::kLfe,
                                          Role::kSideLeft,  Role::kSideRight};
    switch (layout) {
        case Layout::kAuto:
            return kNone;
        case Layout::kStereo:
            return kStereo;
        case Layout::kQuad:
            return kQuad;
        case Layout::k5Point1:
            return k51;
        case Layout::k7Point1:
            return k71;
    }
    return kNone;
}

const char* ToString(Layout layout) {
    switch (layout) {
        case Layout::kAuto:
            return "auto";
        case Layout::kStereo:
            return "stereo";
        case Layout::kQuad:
            return "quad";
        case Layout::k5Point1:
            return "5.1";
        case Layout::k7Point1:
            return "7.1";
    }
    return "auto";
}

std::optional<Layout> LayoutFromString(const std::string& name) {
    for (const Layout layout :
         {Layout::kAuto, Layout::kStereo, Layout::kQuad, Layout::k5Point1, Layout::k7Point1}) {
        if (name == ToString(layout)) return layout;
    }
    return std::nullopt;
}

bool OutputConfig::IsIdentity() const {
    if (layout != Layout::kAuto) return false;
    // Only these apply to kAuto.
    return !swap_left_right && !mono;
}

bool OutputConfig::IsValid() const {
    for (size_t i = 0; i < kMaxChannels; ++i) {
        if (!std::isfinite(levels_db[i]) || levels_db[i] < kMinLevelDb ||
            levels_db[i] > kMaxLevelDb || !std::isfinite(delays_ms[i]) || delays_ms[i] < 0 ||
            delays_ms[i] > kMaxDelayMs) {
            return false;
        }
    }
    return crossover_hz >= kMinCrossoverHz && crossover_hz <= kMaxCrossoverHz;
}

unsigned int OutputConfig::OutputChannels(unsigned int stream_channels) const {
    if (layout == Layout::kAuto) return stream_channels;
    return static_cast<unsigned int>(LayoutRoles(layout).size());
}

std::string OutputConfig::Serialize() const {
    // "key=value;..." with field names that stay stable across versions.
    return std::string("layout=") + ToString(layout) + ";fill=" + (fill ? "1" : "0") +
           ";swap=" + (swap_left_right ? "1" : "0") + ";mono=" + (mono ? "1" : "0") +
           ";sub=" + (subwoofer ? "1" : "0") + ";small=" + (small_speakers ? "1" : "0") +
           ";xover=" + std::to_string(crossover_hz) + ";levels=" + JoinFloats(levels_db) +
           ";delays=" + JoinFloats(delays_ms);
}

std::optional<OutputConfig> OutputConfig::Parse(const std::string& text) {
    std::map<std::string, std::string> fields;
    for (const std::string& part : ::android::base::Split(text, ";")) {
        const size_t eq = part.find('=');
        if (eq == std::string::npos) continue;
        fields[part.substr(0, eq)] = part.substr(eq + 1);
    }
    OutputConfig config;
    // Unknown or missing fields keep their defaults, so that a newer HAL reads
    // an older file.
    if (const auto it = fields.find("layout"); it != fields.end()) {
        const auto layout = LayoutFromString(it->second);
        if (!layout.has_value()) return std::nullopt;
        config.layout = *layout;
    }
    for (const auto& [name, field] :
         {std::pair{"fill", &config.fill}, std::pair{"swap", &config.swap_left_right},
          std::pair{"mono", &config.mono}, std::pair{"sub", &config.subwoofer},
          std::pair{"small", &config.small_speakers}}) {
        if (const auto it = fields.find(name);
            it != fields.end() && !ParseBool(it->second, field)) {
            return std::nullopt;
        }
    }
    if (const auto it = fields.find("xover");
        it != fields.end() && !::android::base::ParseInt(it->second, &config.crossover_hz)) {
        return std::nullopt;
    }
    if (const auto it = fields.find("levels");
        it != fields.end() && !ParseFloats(it->second, &config.levels_db)) {
        return std::nullopt;
    }
    if (const auto it = fields.find("delays");
        it != fields.end() && !ParseFloats(it->second, &config.delays_ms)) {
        return std::nullopt;
    }
    if (!config.IsValid()) return std::nullopt;
    return config;
}

}  // namespace aidl::android::hardware::audio::core::mainline::dsp
