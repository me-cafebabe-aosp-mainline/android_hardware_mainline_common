/*
 * SPDX-FileCopyrightText: The LineageOS Project
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <map>
#include <string>

namespace aidl::android::hardware::audio::core::mainline::control {

// Key / value state the control interface keeps across restarts, one file per
// kind of state in /data/vendor/mainline_audio (created by the APEX's init
// script once /data is available). Every write replaces the whole file
// atomically. Not thread safe: each owner serializes its own file.
class StateStore {
  public:
    using Entries = std::map<std::string, std::string>;

    // `name` is a file name inside the state directory, e.g. "mixer".
    explicit StateStore(std::string name);

    // The stored entries; empty when there is no file (yet) or it is unreadable.
    Entries Load() const;
    // Replaces the stored entries. Returns false (and logs) on failure.
    bool Save(const Entries& entries) const;

    // Keys and values must not contain these: they separate fields and lines.
    static bool IsStorable(const std::string& text);

    static constexpr const char* kDirectory = "/data/vendor/mainline_audio";

  private:
    const std::string path_;
};

}  // namespace aidl::android::hardware::audio::core::mainline::control
