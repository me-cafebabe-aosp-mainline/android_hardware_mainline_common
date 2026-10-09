/*
 * SPDX-FileCopyrightText: The LineageOS Project
 * SPDX-License-Identifier: Apache-2.0
 */

#include "stream/FastPeriods.h"

namespace aidl::android::hardware::audio::core::mainline {

void FastPeriods::Set(const std::string& key, Entry entry) {
    std::lock_guard guard(lock_);
    entries_[key] = entry;
}

std::optional<FastPeriods::Entry> FastPeriods::Get(const std::string& key) const {
    std::lock_guard guard(lock_);
    const auto it = entries_.find(key);
    if (it == entries_.end()) return std::nullopt;
    return it->second;
}

}  // namespace aidl::android::hardware::audio::core::mainline
