/*
 * SPDX-FileCopyrightText: The LineageOS Project
 * SPDX-License-Identifier: Apache-2.0
 */

package vendor.mainline.audio;

/** A user adjustable ALSA mixer control of a sound card. */
@VintfStability
parcelable MixerControl {
    const int TYPE_BOOLEAN = 0;
    const int TYPE_INTEGER = 1;
    const int TYPE_ENUMERATED = 2;

    /** ALSA numid: identifies the control until the HAL restarts. */
    int numid;
    /** e.g. "Master Playback Volume", "Auto-Mute Mode". */
    String name;
    /** Distinguishes controls of the same name. */
    int index;
    /** One of TYPE_*. */
    int type;
    /** Range of a value: 0..1 for booleans, item indexes for enumerations. */
    long min;
    long max;
    /** Step of an integer control, 0 for any. */
    long step;
    /** Names of the items of an enumerated control. */
    String[] items;
    /** Current value, one per channel. */
    long[] values;
    /** dB range of an integer control in 1/100 dB, when hasDbRange. */
    boolean hasDbRange;
    int minDb;
    int maxDb;
    /** The user set the control: its value is applied again at every start. */
    boolean stored;
}
