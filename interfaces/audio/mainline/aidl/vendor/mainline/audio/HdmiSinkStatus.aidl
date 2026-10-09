/*
 * SPDX-FileCopyrightText: The LineageOS Project
 * SPDX-License-Identifier: Apache-2.0
 */

package vendor.mainline.audio;

import vendor.mainline.audio.PassthroughAttempt;
import vendor.mainline.audio.SinkFormat;

/** The sink behind an HDMI or DisplayPort head, for passthrough. */
@VintfStability
parcelable HdmiSinkStatus {
    /** The head has the controls passthrough needs. */
    boolean passthroughPossible;
    /** Which ALSA controls the HAL uses for the head. */
    String controls;
    /** "ELD", "property" (hdmi.passthrough_formats) or "none". */
    String source;
    /** From the ELD, may be empty. */
    String monitorName;
    /** Highest linear PCM channel count, 0 when not announced. */
    int maxPcmChannels;
    /** Compressed formats offered for passthrough. */
    SinkFormat[] formats;
    /** High bit rate support: "supported", "unsupported" or "undetectable". */
    String hbr;

    // Since version 2.

    /**
     * What decided `hbr`: "driver" (the card driver's quirk), "setting" (the card's hbr setting)
     * or "failure" (the head refused it since HDMI connected).
     */
    String hbrSource = "";
    /** The quirk's name for "driver", what was refused for "failure"; may be empty. */
    String hbrDetail = "";
    /** The last passthrough stream started on the head since the HAL started. */
    @nullable PassthroughAttempt lastAttempt;
}
