/*
 * SPDX-FileCopyrightText: The LineageOS Project
 * SPDX-License-Identifier: Apache-2.0
 */

package vendor.mainline.audio;

/** A change for IControl.applySettings(). */
@VintfStability
parcelable SettingChange {
    /** Setting.key. */
    String key;
    /** The new value, null to go back to the device's property. */
    @nullable String value;
}
