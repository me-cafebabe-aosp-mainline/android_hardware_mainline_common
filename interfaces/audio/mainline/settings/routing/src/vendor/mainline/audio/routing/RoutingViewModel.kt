/*
 * SPDX-FileCopyrightText: The LineageOS Project
 * SPDX-License-Identifier: Apache-2.0
 */

package vendor.mainline.audio.routing

import android.app.Application
import android.media.AudioDeviceCallback
import android.media.AudioDeviceInfo
import android.media.AudioManager
import android.os.Handler
import android.os.Looper
import androidx.lifecycle.AndroidViewModel
import androidx.lifecycle.viewModelScope
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.Job
import kotlinx.coroutines.flow.MutableStateFlow
import kotlinx.coroutines.flow.StateFlow
import kotlinx.coroutines.flow.asStateFlow
import kotlinx.coroutines.launch
import kotlinx.coroutines.withContext

/** The device lists and the current default output and input. */
class RoutingViewModel(application: Application) : AndroidViewModel(application) {
    data class Selection(val options: List<DeviceOption>, val choice: AudioRouting.Choice)

    data class State(val output: Selection, val input: Selection)

    private val routing = AudioRouting(application)
    private val audioManager = application.getSystemService(AudioManager::class.java)

    private val _state = MutableStateFlow<State?>(null)
    val state: StateFlow<State?> = _state.asStateFlow()

    private var refreshJob: Job? = null

    private val deviceCallback =
        object : AudioDeviceCallback() {
            override fun onAudioDevicesAdded(addedDevices: Array<out AudioDeviceInfo>) = refresh()

            override fun onAudioDevicesRemoved(removedDevices: Array<out AudioDeviceInfo>) =
                refresh()
        }

    init {
        // Also delivers the initial device list, which triggers the first refresh.
        audioManager.registerAudioDeviceCallback(deviceCallback, Handler(Looper.getMainLooper()))
        refresh()
    }

    fun selectOutput(device: DeviceId?) = change { routing.setOutput(device) }

    fun selectInput(device: DeviceId?) = change { routing.setInput(device) }

    override fun onCleared() {
        audioManager.unregisterAudioDeviceCallback(deviceCallback)
    }

    private fun change(block: () -> Unit) {
        viewModelScope.launch {
            withContext(Dispatchers.IO) { block() }
            RoutingSummaryProvider.notifyChanged(getApplication())
            refresh()
        }
    }

    private fun refresh() {
        refreshJob?.cancel()
        refreshJob = viewModelScope.launch { _state.value = readState() }
    }

    private suspend fun readState(): State {
        val labels = DeviceLabels(getApplication())
        return withContext(Dispatchers.IO) {
            State(
                output =
                    selection(
                        routing.outputs(),
                        routing.outputChoice(),
                        routing.savedOutput(),
                        labels,
                    ),
                input =
                    selection(routing.inputs(), routing.inputChoice(), routing.savedInput(), labels),
            )
        }
    }

    private fun selection(
        devices: List<AudioDeviceInfo>,
        choice: AudioRouting.Choice,
        saved: DeviceId?,
        labels: DeviceLabels,
    ): Selection {
        val options =
            devices
                .map { DeviceOption(DeviceId.of(it), labels.label(it), connected = true) }
                .distinctBy { it.id }
                .toMutableList()
        // Keep the chosen device listed while it is unplugged: the preference stays in force and
        // applies again once it is back.
        val chosen = (choice as? AudioRouting.Choice.Device)?.id ?: saved
        if (chosen != null && options.none { it.id == chosen }) {
            options += DeviceOption(chosen, labels.label(chosen), connected = false)
        }
        return Selection(options, choice)
    }
}
