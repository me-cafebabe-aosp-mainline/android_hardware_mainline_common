/*
 * SPDX-FileCopyrightText: The LineageOS Project
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

namespace aidl::android::hardware::audio::core::mainline::alsa {

// AC97 front panel workaround (README.md, "AC97 front panels"): front jack
// pins of HD Audio codecs without presence detection, through the codec's
// sysfs files. Reconfiguring fails while the card is open: call it before the
// HAL opens the card.

enum class FrontPanelResult {
    kNoHdaCodec,   // No HD Audio codec with reconfiguration support on the card.
    kNoFrontJack,  // Codecs, but no front panel headphone or microphone pin.
    kUnchanged,    // The pins were already configured as asked.
    kApplied,      // The pins were changed and the codecs reconfigured.
    kFailed,       // Writing or reconfiguring failed; the codecs keep their pins.
};

const char* ToString(FrontPanelResult result);

// Marks (`ac97`) or restores the front headphone and mic pins of `card`,
// reconfigures the codecs that changed and waits for their PCM devices.
FrontPanelResult ConfigureFrontPanel(int card, bool ac97);

}  // namespace aidl::android::hardware::audio::core::mainline::alsa
