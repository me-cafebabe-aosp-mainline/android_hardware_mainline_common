/*
 * SPDX-FileCopyrightText: The LineageOS Project
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <atomic>
#include <chrono>
#include <cstdint>
#include <map>
#include <mutex>
#include <optional>
#include <string>

#include <android-base/thread_annotations.h>

#include "dsp/OutputConfig.h"

namespace aidl::android::hardware::audio::core::mainline::dsp {

// The OutputConfig of every output endpoint that has one, by Endpoint::Key(),
// and the test tone that plays on one of them. Shared by the module, its
// streams and the control interface. Thread safe; streams poll version()
// from their worker thread, which costs one atomic load per transfer.
class OutputConfigs {
  public:
    // Changes whenever a config or the test tone does.
    uint64_t version() const { return version_.load(std::memory_order_acquire); }

    // The config of `key`, the default one when it has none.
    OutputConfig Get(const std::string& key) const;
    void Set(const std::string& key, const OutputConfig& config);
    void Remove(const std::string& key);

    // Plays the test tone on `channel` of the output `key` for `duration`.
    void StartTone(const std::string& key, unsigned int channel,
                   std::chrono::milliseconds duration);
    void StopTone();
    // The channel the test tone plays on for `key`, nullopt when it does not.
    std::optional<unsigned int> ToneFor(const std::string& key) const;

  private:
    struct Tone {
        std::string key;
        unsigned int channel = 0;
        std::chrono::steady_clock::time_point until;
    };

    void Bump() { version_.fetch_add(1, std::memory_order_acq_rel); }

    mutable std::mutex lock_;
    std::map<std::string, OutputConfig> configs_ GUARDED_BY(lock_);
    std::optional<Tone> tone_ GUARDED_BY(lock_);
    std::atomic<uint64_t> version_ = 1;
};

}  // namespace aidl::android::hardware::audio::core::mainline::dsp
