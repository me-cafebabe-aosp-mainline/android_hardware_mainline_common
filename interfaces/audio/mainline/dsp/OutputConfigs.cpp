/*
 * SPDX-FileCopyrightText: The LineageOS Project
 * SPDX-License-Identifier: Apache-2.0
 */

#include "dsp/OutputConfigs.h"

namespace aidl::android::hardware::audio::core::mainline::dsp {

OutputConfig OutputConfigs::Get(const std::string& key) const {
    std::lock_guard guard(lock_);
    const auto it = configs_.find(key);
    return it != configs_.end() ? it->second : OutputConfig{};
}

void OutputConfigs::Set(const std::string& key, const OutputConfig& config) {
    {
        std::lock_guard guard(lock_);
        configs_[key] = config;
    }
    Bump();
}

void OutputConfigs::Remove(const std::string& key) {
    {
        std::lock_guard guard(lock_);
        configs_.erase(key);
    }
    Bump();
}

void OutputConfigs::StartTone(const std::string& key, unsigned int channel,
                              std::chrono::milliseconds duration) {
    {
        std::lock_guard guard(lock_);
        tone_ = Tone{.key = key,
                     .channel = channel,
                     .until = std::chrono::steady_clock::now() + duration};
    }
    Bump();
}

void OutputConfigs::StopTone() {
    {
        std::lock_guard guard(lock_);
        tone_.reset();
    }
    Bump();
}

std::optional<unsigned int> OutputConfigs::ToneFor(const std::string& key) const {
    std::lock_guard guard(lock_);
    if (!tone_.has_value() || tone_->key != key ||
        std::chrono::steady_clock::now() >= tone_->until) {
        return std::nullopt;
    }
    return tone_->channel;
}

}  // namespace aidl::android::hardware::audio::core::mainline::dsp
