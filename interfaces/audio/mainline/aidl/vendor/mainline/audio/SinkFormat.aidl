/*
 * SPDX-FileCopyrightText: The LineageOS Project
 * SPDX-License-Identifier: Apache-2.0
 */

package vendor.mainline.audio;

/** A compressed format an HDMI sink decodes. */
@VintfStability
parcelable SinkFormat {
    /** Name as in hdmi.passthrough_formats, e.g. "ac3", "eac3-joc". */
    String name;
    int[] sampleRates;
    int maxChannels;
}
