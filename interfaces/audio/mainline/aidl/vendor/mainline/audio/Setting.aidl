/*
 * SPDX-FileCopyrightText: The LineageOS Project
 * SPDX-License-Identifier: Apache-2.0
 */

package vendor.mainline.audio;

/**
 * A HAL setting: one of the vendor.audio.mainline.* properties of the HAL's
 * README, which a stored value overrides. Settings apply when the HAL starts.
 */
@VintfStability
parcelable Setting {
    const int TYPE_BOOLEAN = 0;
    const int TYPE_INTEGER = 1;
    const int TYPE_STRING = 2;
    /** Comma separated strings. */
    const int TYPE_LIST = 3;
    /** Comma separated integers. */
    const int TYPE_INTEGER_LIST = 4;

    /** Property name without "vendor.audio.mainline.", e.g. "latency_ms", "card.PCH.rates". */
    String key;
    /** One of TYPE_*. */
    int type;
    /** Range of an integer, or of the items of an integer list. */
    int min;
    int max;
    /**
     * The value the HAL runs with: booleans "true" / "false", lists comma
     * separated, "" for unset.
     */
    String value;
    /** The value from the device's properties, without the stored one. */
    String defaultValue;
    /** The stored (confirmed) value, null when there is none. */
    @nullable String stored;
}
