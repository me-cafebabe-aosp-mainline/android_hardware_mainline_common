/*
 * SPDX-FileCopyrightText: The LineageOS Project
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <optional>
#include <string>
#include <vector>

namespace aidl::android::hardware::audio::core::mainline::alsa {

// A user adjustable mixer control of a card (snd_ctl element of the MIXER
// interface), with its current value.
struct ControlInfo {
    enum class Type { kBoolean, kInteger, kEnumerated };

    unsigned int numid = 0;  // Only valid while the card exists.
    std::string name;        // e.g. "Master Playback Volume"
    unsigned int index = 0;  // Distinguishes controls of the same name.
    Type type = Type::kBoolean;
    unsigned int channels = 1;
    // kInteger: range and step; kBoolean: 0..1; kEnumerated: 0..items-1.
    long min = 0;
    long max = 1;
    long step = 0;
    std::vector<std::string> items;  // kEnumerated only.
    std::vector<long> values;        // One per channel.
    // dB range of a kInteger control, in 1/100 dB, when the driver has one.
    std::optional<std::pair<long, long>> db_range;

    std::string ToString() const;
};

// The writable, active MIXER controls of `card` of the types above. Others
// (byte arrays, IEC958 status, read-only meters) are left out. Empty when the
// card can not be opened.
std::vector<ControlInfo> ListControls(int card);

// The control called `name` / `index`, nullopt when there is none.
std::optional<ControlInfo> FindControl(int card, const std::string& name, unsigned int index);

// Writes `values` (one per channel, or a single one for every channel) to the
// control `numid`. Values are clamped to the control's range. Returns 0 or a
// negative errno; -EINVAL for a control ListControls() does not offer.
int WriteControl(int card, unsigned int numid, const std::vector<long>& values);

}  // namespace aidl::android::hardware::audio::core::mainline::alsa
