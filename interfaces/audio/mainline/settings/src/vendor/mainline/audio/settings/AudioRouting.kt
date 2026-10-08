/*
 * SPDX-FileCopyrightText: The LineageOS Project
 * SPDX-License-Identifier: Apache-2.0
 */

package vendor.mainline.audio.settings

import android.content.Context
import android.media.AudioDeviceInfo
import android.media.AudioManager
import android.media.MediaRecorder
import android.util.Log

/**
 * The default output and input: the only code that changes AudioService's routing preferences.
 *
 * The default output is the preferred device of every product strategy that AudioService exposes
 * (internal ones excluded), the default input the preferred device of every user facing capture
 * preset. AudioService forgets these on reboot, so the choice is also stored here, in
 * device-protected storage, and [reapply] restores it at boot. All methods block on Binder.
 */
class AudioRouting(context: Context) {
    sealed interface Choice {
        /** No preference: the audio policy decides. */
        data object Automatic : Choice

        data class Device(val id: DeviceId) : Choice

        /** Preferences that differ between strategies or presets, set by someone else. */
        data object Mixed : Choice
    }

    private val audioManager = context.getSystemService(AudioManager::class.java)
    private val prefs =
        context
            .createDeviceProtectedStorageContext()
            .getSharedPreferences(PREFS_NAME, Context.MODE_PRIVATE)

    fun outputs(): List<AudioDeviceInfo> =
        audioManager.getDevices(AudioManager.GET_DEVICES_OUTPUTS).filter { it.isUserChoice() }

    fun inputs(): List<AudioDeviceInfo> =
        audioManager.getDevices(AudioManager.GET_DEVICES_INPUTS).filter { it.isUserChoice() }

    fun outputChoice(): Choice =
        choiceOf(
            AudioManager.getAudioProductStrategies().map {
                audioManager.getPreferredDeviceForStrategy(it)?.let(::DeviceId)
            }
        )

    fun inputChoice(): Choice =
        choiceOf(
            CAPTURE_PRESETS.map {
                audioManager.getPreferredDevicesForCapturePreset(it).firstOrNull()?.let(::DeviceId)
            }
        )

    /** The stored choice, which may be a device that is not connected now. */
    fun savedOutput(): DeviceId? = DeviceId.parse(prefs.getString(KEY_OUTPUT, null))

    fun savedInput(): DeviceId? = DeviceId.parse(prefs.getString(KEY_INPUT, null))

    /** Makes [device] the default output, or returns the choice to the policy when null. */
    fun setOutput(device: DeviceId?) {
        applyOutput(device)
        save(KEY_OUTPUT, device)
    }

    /** Makes [device] the default input, or returns the choice to the policy when null. */
    fun setInput(device: DeviceId?) {
        applyInput(device)
        save(KEY_INPUT, device)
    }

    /** Applies the stored choices again. Leaves AudioService alone where nothing is stored. */
    fun reapply() {
        savedOutput()?.let(::applyOutput)
        savedInput()?.let(::applyInput)
    }

    private fun applyOutput(device: DeviceId?) {
        val attributes = device?.toAttributes()
        for (strategy in AudioManager.getAudioProductStrategies()) {
            val ok =
                if (attributes != null) {
                    audioManager.setPreferredDeviceForStrategy(strategy, attributes)
                } else {
                    audioManager.removePreferredDeviceForStrategy(strategy) ||
                        audioManager.getPreferredDeviceForStrategy(strategy) == null
                }
            if (!ok) Log.w(TAG, "preferred output $device not applied to $strategy")
        }
    }

    private fun applyInput(device: DeviceId?) {
        val attributes = device?.toAttributes()
        for (preset in CAPTURE_PRESETS) {
            val ok =
                if (attributes != null) {
                    audioManager.setPreferredDeviceForCapturePreset(preset, attributes)
                } else {
                    audioManager.clearPreferredDevicesForCapturePreset(preset) ||
                        audioManager.getPreferredDevicesForCapturePreset(preset).isEmpty()
                }
            if (!ok) Log.w(TAG, "preferred input $device not applied to preset $preset")
        }
    }

    private fun save(key: String, device: DeviceId?) {
        val editor = prefs.edit()
        if (device != null) editor.putString(key, device.serialize()) else editor.remove(key)
        editor.apply()
    }

    private companion object {
        const val TAG = "MainlineAudioSettings"
        const val PREFS_NAME = "routing"
        const val KEY_OUTPUT = "output"
        const val KEY_INPUT = "input"

        /** Capture presets an app picks for recording, calls and recognition. */
        val CAPTURE_PRESETS =
            listOf(
                MediaRecorder.AudioSource.MIC,
                MediaRecorder.AudioSource.CAMCORDER,
                MediaRecorder.AudioSource.VOICE_RECOGNITION,
                MediaRecorder.AudioSource.VOICE_COMMUNICATION,
                MediaRecorder.AudioSource.UNPROCESSED,
                MediaRecorder.AudioSource.VOICE_PERFORMANCE,
            )

        fun choiceOf(preferred: List<DeviceId?>): Choice {
            val distinct = preferred.toSet()
            return when {
                distinct.isEmpty() || distinct == setOf(null) -> Choice.Automatic
                distinct.size == 1 -> Choice.Device(distinct.single()!!)
                else -> Choice.Mixed
            }
        }
    }
}
