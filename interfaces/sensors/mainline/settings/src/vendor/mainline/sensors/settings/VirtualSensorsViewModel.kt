/*
 * SPDX-FileCopyrightText: The LineageOS Project
 * SPDX-License-Identifier: Apache-2.0
 */

package vendor.mainline.sensors.settings

import androidx.lifecycle.ViewModel
import androidx.lifecycle.viewModelScope
import kotlinx.coroutines.Job
import kotlinx.coroutines.flow.MutableStateFlow
import kotlinx.coroutines.flow.StateFlow
import kotlinx.coroutines.flow.asStateFlow
import kotlinx.coroutines.launch
import vendor.mainline.sensors.VirtualSensor

/** The virtual sensors and their values. */
class VirtualSensorsViewModel : ViewModel() {
    sealed interface State {
        data object Loading : State

        data object Unavailable : State

        data class Loaded(val sensors: List<VirtualSensor>) : State
    }

    private val _state = MutableStateFlow<State>(State.Loading)
    val state: StateFlow<State> = _state.asStateFlow()

    private var refreshJob: Job? = null

    fun refresh() {
        refreshJob?.cancel()
        refreshJob = viewModelScope.launch { _state.value = load() }
    }

    /** Sets what the sensor reports; the HAL clamps the values to its range. */
    fun set(handle: Int, values: FloatArray) {
        viewModelScope.launch {
            HalControl.call { it.setValues(handle, values) }
            refresh()
        }
    }

    /** Returns the sensor to its defaults. */
    fun reset(handle: Int) {
        viewModelScope.launch {
            HalControl.call { it.resetValues(handle) }
            refresh()
        }
    }

    /** Sets every motion sensor to what it reports in [position]. */
    fun setPosition(position: DevicePosition) {
        val sensors = (state.value as? State.Loaded)?.sensors ?: return
        viewModelScope.launch {
            HalControl.call { service ->
                for (sensor in sensors) {
                    position.valuesFor(sensor)?.let { service.setValues(sensor.handle, it) }
                }
            }
            refresh()
        }
    }

    private suspend fun load(): State =
        HalControl.sensors()?.let { State.Loaded(it) } ?: State.Unavailable
}
