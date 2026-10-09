/*
 * SPDX-FileCopyrightText: The LineageOS Project
 * SPDX-License-Identifier: Apache-2.0
 */

package vendor.mainline.audio;

import vendor.mainline.audio.MixerControl;
import vendor.mainline.audio.OutputSettings;
import vendor.mainline.audio.PassthroughProbe;
import vendor.mainline.audio.Setting;
import vendor.mainline.audio.SettingChange;
import vendor.mainline.audio.Status;
import vendor.mainline.audio.TrialStatus;

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

    /** The global HAL settings, and the per-card ones of the cards in use. */
    Setting[] getSettings();

    /**
     * Applies `changes` on top of the stored settings as a trial: the HAL
     * restarts with them shortly after this returns, which also restarts
     * audioserver. After the restart, keepSettings() confirms them; without
     * it the HAL reverts on its own (see TrialStatus). Fails with
     * EX_ILLEGAL_ARGUMENT for an unknown key or invalid value, and with
     * EX_ILLEGAL_STATE while a trial runs or when nothing can be stored.
     */
    void applySettings(in SettingChange[] changes);

    TrialStatus getTrialStatus();

    /** Confirms the settings on trial. EX_ILLEGAL_STATE without a trial. */
    void keepSettings();

    /**
     * Drops the settings on trial; the HAL restarts with the previous ones.
     * EX_ILLEGAL_STATE without a trial.
     */
    void revertSettings();

    /**
     * Drops every stored setting; the HAL restarts with the device's
     * properties. EX_ILLEGAL_STATE when that can not be stored.
     */
    void resetSettings();

    /**
     * The processing of the output endpoint `key` (EndpointStatus.key). Fails
     * with EX_ILLEGAL_ARGUMENT for a key that is no output of the HAL.
     */
    OutputSettings getOutputSettings(String key);

    /**
     * Sets and stores the processing of an output; it applies right away.
     * Fails with EX_ILLEGAL_ARGUMENT for an unknown output, a value out of
     * range, or a layout with more channels than the output has.
     */
    void setOutputSettings(String key, in OutputSettings settings);

    /** Returns an output to the defaults: no processing. */
    void resetOutputSettings(String key);

    /**
     * Plays pink noise on one speaker of an output instead of what its streams
     * play, for `durationMs` (100 to 5000), or stops with a negative channel.
     * `channel` counts the speakers of the output's layout as in
     * OutputSettings.levelsDb, so the output needs a layout other than
     * LAYOUT_AUTO. Only plays while a stream plays to the output:
     * the caller keeps one running, e.g. silence. Fails with
     * EX_ILLEGAL_ARGUMENT for an unknown output or channel.
     */
    void playTestTone(String key, int channel, int durationMs);

    // Since version 2.

    /**
     * Forgets the high bit rate refusals of the HDMI heads (HdmiSinkStatus.hbrSource "failure"),
     * so that the next passthrough stream tries high bit rate again. The formats that need it are
     * offered again from the next HDMI connection on, as IModule's ports only change then.
     */
    void resetHbr();
    /**
     * Opens the PCM device of the HDMI head `key` (EndpointStatus.key) with each IEC 61937
     * configuration (2 channels at 48 and 192 kHz, 8 channels at 192 kHz) and the matching
     * non-audio channel status, and closes it again: nothing is played. Tells what the hardware
     * accepts, independently of the sink. Fails with EX_ILLEGAL_ARGUMENT for a key that is no
     * HDMI output or a head without passthrough controls, and with EX_ILLEGAL_STATE while a
     * stream holds the device.
     */
    PassthroughProbe[] probeHdmi(String key);
}
