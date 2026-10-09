/*
 * SPDX-FileCopyrightText: The LineageOS Project
 * SPDX-License-Identifier: Apache-2.0
 */

package vendor.mainline.audio;

/** The last time a passthrough stream started on an HDMI head, and how it went. */
@VintfStability
parcelable PassthroughAttempt {
    /** Name as in hdmi.passthrough_formats, or "iec61937" for data packed by the app. */
    String format;
    /** The stream's sample rate and channel count, as the app opened it. */
    int contentRate;
    int channels;
    /**
     * The PCM configuration the IEC 61937 data needed: 2 or 8 channels (8 is high bit rate), and
     * its rate. 0 when the HAL did not get that far.
     */
    int pcmChannels;
    int pcmRate;
    boolean succeeded;
    /**
     * Where it failed: "format" (no IEC 61937 stream for it, e.g. high bit rate needed and not
     * available), "packer", "channel status", or the PCM step "open", "hw_params", "sw_params",
     * "prepare". Empty on success.
     */
    String failedStep;
    /** Negative errno of a failed PCM step, 0 otherwise. */
    int error;
    /** When, in milliseconds since the epoch. */
    long timeMillis;
}
