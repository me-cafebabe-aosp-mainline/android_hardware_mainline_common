/*
 * SPDX-FileCopyrightText: The LineageOS Project
 * SPDX-License-Identifier: Apache-2.0
 */

package vendor.mainline.audio.settings

import android.app.Application
import android.media.AudioManager
import androidx.lifecycle.AndroidViewModel
import androidx.lifecycle.viewModelScope
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.Job
import kotlinx.coroutines.flow.MutableStateFlow
import kotlinx.coroutines.flow.StateFlow
import kotlinx.coroutines.flow.asStateFlow
import kotlinx.coroutines.launch
import kotlinx.coroutines.withContext
import vendor.mainline.audio.Status

/** What the HAL reports about the sound cards, and what Android makes of HDMI passthrough. */
class DiagnosticsViewModel(application: Application) : AndroidViewModel(application) {
    sealed interface State {
        data object Loading : State

        /** The HAL did not answer. */
        data object Unavailable : State

        /** [android] is null when the HAL reports no HDMI head. */
        data class Loaded(val status: Status, val android: AndroidSurround.Info?) : State
    }

    /** The answer to [probe], until the page showed it. */
    data class Probe(val name: String, val outcome: HalControl.ProbeOutcome)

    private val _state = MutableStateFlow<State>(State.Loading)
    val state: StateFlow<State> = _state.asStateFlow()

    private val _probe = MutableStateFlow<Probe?>(null)
    val probe: StateFlow<Probe?> = _probe.asStateFlow()

    private var refreshJob: Job? = null

    /** Reads the status again; streams and jacks change all the time. */
    fun refresh() {
        refreshJob?.cancel()
        refreshJob =
            viewModelScope.launch {
                val status = HalControl.status()
                _state.value =
                    if (status == null) State.Unavailable else State.Loaded(status, android(status))
            }
    }

    /** Tests the HDMI head [key] (EndpointStatus.key), named [name] on the page. */
    fun probe(key: String, name: String) {
        viewModelScope.launch { _probe.value = Probe(name, HalControl.probeHdmi(key)) }
    }

    fun probeShown() {
        _probe.value = null
    }

    /** Lets HDMI try high bit rate again, then shows the new state. */
    fun resetHbr() {
        viewModelScope.launch {
            HalControl.resetHbr()
            refresh()
        }
    }

    /** The plain text report, null when the HAL did not answer. */
    suspend fun report(): String? = HalControl.call { it.report }

    private suspend fun android(status: Status): AndroidSurround.Info? {
        val sinks = status.endpoints.mapNotNull { it.hdmi }
        if (sinks.isEmpty()) return null
        // The formats the HAL offers for the sinks, and what apps may pack themselves.
        val formats =
            (sinks.flatMap { sink -> sink.formats.orEmpty().map { it.name } } + IEC61937).distinct()
        val audioManager = getApplication<Application>().getSystemService(AudioManager::class.java)
        return withContext(Dispatchers.IO) { AndroidSurround.read(audioManager, formats) }
    }

    private companion object {
        const val IEC61937 = "iec61937"
    }
}
