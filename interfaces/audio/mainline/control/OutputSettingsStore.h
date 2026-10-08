/*
 * SPDX-FileCopyrightText: The LineageOS Project
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <memory>
#include <mutex>
#include <string>

#include <android-base/thread_annotations.h>

#include "control/StateStore.h"
#include "dsp/OutputConfigs.h"

namespace aidl::android::hardware::audio::core::mainline::control {

// Stores the output processing set through IControl, by Endpoint::Key(), and
// keeps the module's OutputConfigs in step with it. Thread safe.
class OutputSettingsStore {
  public:
    // Loads the stored configs into `configs`.
    explicit OutputSettingsStore(std::shared_ptr<dsp::OutputConfigs> configs);

    void Set(const std::string& key, const dsp::OutputConfig& config);
    void Reset(const std::string& key);

  private:
    const std::shared_ptr<dsp::OutputConfigs> configs_;
    const StateStore store_;
    std::mutex lock_;
    StateStore::Entries entries_ GUARDED_BY(lock_);
};

}  // namespace aidl::android::hardware::audio::core::mainline::control
