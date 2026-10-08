/*
 * SPDX-FileCopyrightText: The LineageOS Project
 * SPDX-License-Identifier: Apache-2.0
 */

package vendor.mainline.audio;

import vendor.mainline.audio.MixerControl;
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

    /**
     * The user adjustable mixer controls of a card the HAL uses, by card index
     * (CardStatus.index). Fails with EX_ILLEGAL_ARGUMENT for another card.
     */
    MixerControl[] getMixerControls(int card);

    /**
     * Sets a mixer control (by MixerControl.numid) and stores the value, which
     * is applied again whenever the HAL starts and after UCM switched devices
     * on the card. `values` holds one value per channel, or a single value for
     * every channel; values are clamped to the control's range. Fails with
     * EX_ILLEGAL_ARGUMENT for an unknown card or control.
     */
    void setMixerControl(int card, int numid, in long[] values);

    /**
     * Forgets the stored mixer values of a card. The current values stay until
     * UCM or a restart of the HAL sets them.
     */
    void resetMixerControls(int card);
}
