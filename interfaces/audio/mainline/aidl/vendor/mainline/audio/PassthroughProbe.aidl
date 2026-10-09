/*
 * SPDX-FileCopyrightText: The LineageOS Project
 * SPDX-License-Identifier: Apache-2.0
 */

package vendor.mainline.audio;

/** Whether an HDMI head accepted one IEC 61937 PCM configuration, see IControl.probeHdmi(). */
@VintfStability
parcelable PassthroughProbe {
    int channels;
    int rate;
    /** 8 channels at 192 kHz: high bit rate. */
    boolean hbr;
    boolean accepted;
    /** "channel status", "open", "hw_params", "sw_params" or "prepare"; empty when accepted. */
    String failedStep;
    /** Negative errno of the failed step, 0 otherwise. */
    int error;
}
