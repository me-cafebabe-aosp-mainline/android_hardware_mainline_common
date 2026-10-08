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
import vendor.mainline.audio.CardStatus
import vendor.mainline.audio.MixerControl

/** The mixer controls of every card the HAL uses. */
class MixerViewModel : ViewModel() {
    data class Card(val status: CardStatus, val controls: List<MixerControl>)

    sealed interface State {
        data object Loading : State

        data object Unavailable : State

        data class Loaded(val cards: List<Card>) : State
    }

    private val _state = MutableStateFlow<State>(State.Loading)
    val state: StateFlow<State> = _state.asStateFlow()

    private var refreshJob: Job? = null

    fun refresh() {
        refreshJob?.cancel()
        refreshJob = viewModelScope.launch { _state.value = load() }
    }

    /** Sets a control to [value] on every channel. */
    fun set(card: Int, control: MixerControl, value: Long) {
        viewModelScope.launch {
            HalControl.call { it.setMixerControl(card, control.numid, longArrayOf(value)) }
            refresh()
        }
    }

    /** Forgets what the user set on [card]; the values stay until the next restart. */
    fun forget(card: Int) {
        viewModelScope.launch {
            HalControl.call { it.resetMixerControls(card) }
            refresh()
        }
    }

    private suspend fun load(): State {
        val status = HalControl.status() ?: return State.Unavailable
        val cards =
            status.cards.map { card ->
                Card(card, HalControl.call { it.getMixerControls(card.index) }?.toList().orEmpty())
            }
        return State.Loaded(cards)
    }
}
