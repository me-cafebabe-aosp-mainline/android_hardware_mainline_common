/*
 * SPDX-FileCopyrightText: The LineageOS Project
 * SPDX-License-Identifier: Apache-2.0
 */

package vendor.mainline.audio;

import vendor.mainline.audio.Status;

/**
 * Control interface of the mainline audio HAL, served next to
 * android.hardware.audio.core.IModule/default by the same process. It is
 * meant for the settings app shipped in the HAL's APEX, not for the
 * framework: the framework only talks to IModule.
 */
@VintfStability
interface IControl {
    /** What the HAL found at start-up, and what is in use right now. */
    Status getStatus();

    /**
     * A plain text report for bug reports: the HAL's dumpsys output plus the
     * kernel's view of the sound cards.
     */
    String getReport();
}
