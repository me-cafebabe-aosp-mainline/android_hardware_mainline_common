/*
 * SPDX-FileCopyrightText: The LineageOS Project
 * SPDX-License-Identifier: Apache-2.0
 */

package vendor.mainline.audio;

/** A sound card, as the kernel reports it. */
@VintfStability
parcelable CardStatus {
    int index;
    /** e.g. "PCH" */
    String id;
    /** e.g. "HDA-Intel" */
    String driver;
    /** e.g. "HDA Intel PCH" */
    String name;
    String longName;
    String mixerName;
    /** The card that provides the built-in speaker and microphone. */
    boolean primary;
    /** Selected UCM verb, empty when the card has no UCM profile. */
    String ucmVerb;
    /** Every UCM device of the verb. */
    String[] ucmDevices;
    /** The UCM devices that are enabled right now. */
    String[] ucmEnabledDevices;
}
