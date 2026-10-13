/*
 * SPDX-FileCopyrightText: The LineageOS Project
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <string>
#include <vector>

namespace aidl::android::hardware::audio::core::mainline::alsa {

struct MixerInitOptions {
    // Percentage of the control range applied to playback volume controls.
    int playback_percent = 100;
    // Percentage of the control range applied to capture volume controls.
    int capture_percent = 80;
    // Turn every switch off before turning it on, instead of only turning it
    // on. A guest's driver skips a write that
    // does not change its cached value, so an emulated sound card (QEMU)
    // that applies the mixer to its host audio stream only on a write
    // would never see the values of a card that already has them.
    bool toggle_switches = false;
};

// Brings the simple mixer of a card into a usable state, in the spirit of
// "alsactl init": every playback switch is turned on, every playback volume
// is set to `playback_percent`, capture switches are turned on and capture
// volumes set to `capture_percent`. Controls that would be harmful when
// enabled blindly (loopbacks, beeps, mic boosts) are left alone.
//
// This is only used for cards that have no UCM profile; a UCM profile knows
// far better which controls matter. Returns false when the mixer could not be
// opened.
// With `toggle_switches` each switch is turned off first, so that turning it
// on is a change the card sees.
bool InitializeMixer(int card, const MixerInitOptions& options);

// Sets the playback volume of the named simple mixer elements of `card` to
// 0 dB, or to their maximum when that is lower. Android applies the volume
// digitally, so hardware volume left below unity (UCM boot sequences set 60%)
// only makes the output quieter. Missing elements and elements without a dB
// scale are skipped.
void SetPlaybackLevelUnity(int card, const std::vector<std::string>& elements);

}  // namespace aidl::android::hardware::audio::core::mainline::alsa
