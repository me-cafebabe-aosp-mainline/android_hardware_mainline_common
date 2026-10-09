/*
 * SPDX-FileCopyrightText: The LineageOS Project
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <cstdint>
#include <map>
#include <mutex>
#include <optional>
#include <string>

#include <android-base/thread_annotations.h>

namespace aidl::android::hardware::audio::core::mainline {

// What the last low latency (FAST) stream got when it opened each endpoint:
// its burst and the period the driver granted. A period longer than the burst
// makes the framework's FastMixer underrun. Written by stream workers, read by
// the control service. Thread safe.
class FastPeriods {
  public:
    struct Entry {
        uint32_t burst_frames = 0;
        uint32_t period_frames = 0;
    };

    // `key`: Endpoint::Key().
    void Set(const std::string& key, Entry entry);
    std::optional<Entry> Get(const std::string& key) const;

  private:
    mutable std::mutex lock_;
    std::map<std::string, Entry> entries_ GUARDED_BY(lock_);
};

}  // namespace aidl::android::hardware::audio::core::mainline
