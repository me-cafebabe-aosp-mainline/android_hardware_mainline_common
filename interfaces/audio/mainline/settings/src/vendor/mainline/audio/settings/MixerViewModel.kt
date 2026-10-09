/*
 * SPDX-FileCopyrightText: The LineageOS Project
 * SPDX-License-Identifier: Apache-2.0
 */

package vendor.mainline.audio.settings

import androidx.lifecycle.ViewModel
import androidx.lifecycle.viewModelScope
import kotlinx.coroutines.Job
import kotlinx.coroutines.channels.Channel
import kotlinx.coroutines.flow.Flow
import kotlinx.coroutines.flow.MutableStateFlow
import kotlinx.coroutines.flow.StateFlow
import kotlinx.coroutines.flow.asStateFlow
import kotlinx.coroutines.flow.receiveAsFlow
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

    private val _failures = Channel<Int>(Channel.BUFFERED)
    /** String resources of the changes the HAL did not take, for the page to show once each. */
    val failures: Flow<Int> = _failures.receiveAsFlow()

    fun refresh() {
        refreshJob?.cancel()
        refreshJob = viewModelScope.launch { _state.value = load() }
    }

    /** Sets a control to [value] on every channel. */
    fun set(card: Int, control: MixerControl, value: Long) {
        viewModelScope.launch {
            // Refused for a control the card no longer has, or one the driver did not take. The
            // refresh shows the value the control really has.
            val set =
                HalControl.call { it.setMixerControl(card, control.numid, longArrayOf(value)) }
            if (set == null) _failures.trySend(R.string.mixer_set_failed)
            refresh()
        }
    }

    /** Forgets what the user set on [card]; the values stay until the next restart. */
    fun forget(card: Int) {
        viewModelScope.launch {
            if (HalControl.call { it.resetMixerControls(card) } == null) {
                _failures.trySend(R.string.mixer_forget_failed)
            }
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
