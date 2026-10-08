/*
 * SPDX-FileCopyrightText: The LineageOS Project
 * SPDX-License-Identifier: Apache-2.0
 */

package vendor.mainline.audio.settings

import androidx.lifecycle.ViewModel
import androidx.lifecycle.viewModelScope
import kotlinx.coroutines.Job
import kotlinx.coroutines.flow.MutableStateFlow
import kotlinx.coroutines.flow.StateFlow
import kotlinx.coroutines.flow.asStateFlow
import kotlinx.coroutines.launch
import vendor.mainline.audio.Status

/** What the HAL reports about the sound cards. */
class DiagnosticsViewModel : ViewModel() {
    sealed interface State {
        data object Loading : State

        /** The HAL did not answer. */
        data object Unavailable : State

        data class Loaded(val status: Status) : State
    }

    private val _state = MutableStateFlow<State>(State.Loading)
    val state: StateFlow<State> = _state.asStateFlow()

    private var refreshJob: Job? = null

    /** Reads the status again; streams and jacks change all the time. */
    fun refresh() {
        refreshJob?.cancel()
        refreshJob =
            viewModelScope.launch {
                _state.value = HalControl.status()?.let { State.Loaded(it) } ?: State.Unavailable
            }
    }

    /** The plain text report, null when the HAL did not answer. */
    suspend fun report(): String? = HalControl.call { it.report }
}
