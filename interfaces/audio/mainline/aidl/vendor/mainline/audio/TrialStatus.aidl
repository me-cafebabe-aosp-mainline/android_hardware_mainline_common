/*
 * SPDX-FileCopyrightText: The LineageOS Project
 * SPDX-License-Identifier: Apache-2.0
 */

package vendor.mainline.audio;

/** Whether the HAL runs with settings that wait for a confirmation. */
@VintfStability
parcelable TrialStatus {
    /** keepSettings() or revertSettings() is expected. */
    boolean active;
    /** Seconds until the HAL reverts on its own, while active. */
    int secondsLeft;
    /**
     * The last new settings were dropped, because they were not confirmed in
     * time or the HAL failed to start with them.
     */
    boolean revertedAfterFailure;
}
