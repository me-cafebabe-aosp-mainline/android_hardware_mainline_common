/*
 * SPDX-FileCopyrightText: The LineageOS Project
 * SPDX-License-Identifier: Apache-2.0
 */

package vendor.mainline.audio;

import vendor.mainline.audio.HdmiSinkStatus;

/** One playback or capture path and the device port it backs. */
@VintfStability
parcelable EndpointStatus {
    const int JACK_UNKNOWN = -1;
    const int JACK_UNPLUGGED = 0;
    const int JACK_PLUGGED = 1;

    /**
     * Identifies the path across restarts and card renumbering:
     * "<card id>/<UCM device or PCM device number>/<in|out>". Endpoints
     * backed by the same path (e.g. headphones and headset on one jack)
     * share the key.
     */
    String key;
    /** Name of the device port, e.g. "Speaker", "PCH: HDMI 1". */
    String name;
    /** e.g. "Speaker", "Headphones", "HDMI", "BusOut", "Mic". */
    String role;
    boolean input;
    /** Permanently attached, rather than a template the framework connects. */
    boolean attached;
    /** Android's default device of its direction. */
    boolean isDefault;
    /** AIDL device description, e.g. "OUT_SPEAKER", "OUT_DEVICE hdmi". */
    String deviceType;
    /** Device address, set for bus ports. */
    String address;
    /** No sound card: audio is discarded, or silence is captured. */
    boolean nullDevice;

    /** -1 for a null device. */
    int card;
    String cardId;
    /** alsa-lib PCM name, e.g. "hw:0,3". */
    String pcm;
    /** UCM device, empty without UCM. */
    String ucmDevice;
    int priority;

    /** What the path accepts, after the card filter properties. */
    String[] formats;
    int[] sampleRates;
    int minChannels;
    int maxChannels;

    /** Streams using the path's UCM device now, 0 without UCM. */
    int activeStreams;
    /** Stream that owns the PCM device, empty when free or not exclusive. */
    String pcmOwner;
    /** One of JACK_*. */
    int jack;

    /** Set for HDMI and DisplayPort heads. */
    @nullable HdmiSinkStatus hdmi;
}
