/*
 * SPDX-FileCopyrightText: The LineageOS Project
 * SPDX-License-Identifier: Apache-2.0
 */

package vendor.mainline.audio;

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
}
