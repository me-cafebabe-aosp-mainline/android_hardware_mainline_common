/*
 * SPDX-FileCopyrightText: The LineageOS Project
 * SPDX-License-Identifier: Apache-2.0
 */

package vendor.mainline.audio;

import vendor.mainline.audio.CardStatus;
import vendor.mainline.audio.EndpointStatus;

@VintfStability
parcelable Status {
    /** The sound cards the HAL uses, in card index order. */
    CardStatus[] cards;
    /** One entry per device port of IModule/default, USB templates excluded. */
    EndpointStatus[] endpoints;

    // Since version 2.

    /** The fast_latency_ms setting the HAL runs with; 0: no low latency output. */
    int fastLatencyMs;
    /**
     * Whether the primary output got the FAST flag. It only does when its buffer is below the
     * framework's normal mixer period, as only then the framework runs a FastMixer: at 48 kHz,
     * `fastBufferFrames` (the buffer fast_latency_ms gives) against `normalMixerFrames`.
     */
    boolean fastActive;
    int fastBufferFrames;
    int normalMixerFrames;
    /** Compressed formats this build can pack for passthrough, names as in hdmi.passthrough_formats. */
    String[] passthroughFormats = {};
}
