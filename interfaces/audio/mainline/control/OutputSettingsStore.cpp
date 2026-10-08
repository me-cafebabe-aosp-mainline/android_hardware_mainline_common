/*
 * SPDX-FileCopyrightText: The LineageOS Project
 * SPDX-License-Identifier: Apache-2.0
 */

#define LOG_TAG "MainlineAudio_Control"

#include "control/OutputSettingsStore.h"

#include <utility>

#include <android-base/logging.h>

namespace aidl::android::hardware::audio::core::mainline::control {

OutputSettingsStore::OutputSettingsStore(std::shared_ptr<dsp::OutputConfigs> configs)
    : configs_(std::move(configs)), store_("output") {
    std::lock_guard guard(lock_);
    entries_ = store_.Load();
    for (auto it = entries_.begin(); it != entries_.end();) {
        if (const auto config = dsp::OutputConfig::Parse(it->second); config.has_value()) {
            configs_->Set(it->first, *config);
            ++it;
        } else {
            LOG(WARNING) << "dropping the unreadable output settings of " << it->first;
            it = entries_.erase(it);
        }
    }
}

void OutputSettingsStore::Set(const std::string& key, const dsp::OutputConfig& config) {
    std::lock_guard guard(lock_);
    configs_->Set(key, config);
    if (config == dsp::OutputConfig{}) {
        entries_.erase(key);
    } else if (StateStore::IsStorable(key)) {
        entries_[key] = config.Serialize();
    }
    store_.Save(entries_);
    LOG(INFO) << "output " << key << ": " << config.Serialize();
}

void OutputSettingsStore::Reset(const std::string& key) {
    std::lock_guard guard(lock_);
    configs_->Remove(key);
    entries_.erase(key);
    store_.Save(entries_);
    LOG(INFO) << "output " << key << ": defaults";
}

}  // namespace aidl::android::hardware::audio::core::mainline::control
