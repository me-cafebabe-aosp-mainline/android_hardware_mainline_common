/*
 * SPDX-FileCopyrightText: The LineageOS Project
 * SPDX-License-Identifier: Apache-2.0
 */

package vendor.mainline.audio.settings

import androidx.lifecycle.ViewModel
import androidx.lifecycle.viewModelScope
import kotlinx.coroutines.Job
import kotlinx.coroutines.channels.Channel
import kotlinx.coroutines.delay
import kotlinx.coroutines.flow.Flow
import kotlinx.coroutines.flow.MutableStateFlow
import kotlinx.coroutines.flow.StateFlow
import kotlinx.coroutines.flow.asStateFlow
import kotlinx.coroutines.flow.receiveAsFlow
import kotlinx.coroutines.flow.update
import kotlinx.coroutines.launch
import vendor.mainline.audio.CardStatus
import vendor.mainline.audio.Setting
import vendor.mainline.audio.SettingChange
import vendor.mainline.audio.TrialStatus

/**
 * The HAL settings, the user's unapplied edits, and the trial that follows applying them: the HAL
 * restarts with new settings and reverts them on its own unless they are kept.
 */
class HalSettingsViewModel : ViewModel() {
    data class State(
        val settings: List<Setting>,
        val cards: List<CardStatus>,
        /** Unapplied edits by key; a null value returns the setting to the device's property. */
        val edits: Map<String, String?>,
        val trial: TrialStatus,
        /** The HAL restarts: no edits, no trial decision until it is back. */
        val restarting: Boolean,
        /** Passthrough formats this HAL build can pack, for hdmi.passthrough_formats. */
        val passthroughFormats: List<String> = emptyList(),
    ) {
        val hasStored: Boolean
            get() = settings.any { it.stored != null }
    }

    sealed interface Ui {
        data object Loading : Ui

        data object Unavailable : Ui

        data class Loaded(val state: State) : Ui
    }

    private val _ui = MutableStateFlow<Ui>(Ui.Loading)
    val ui: StateFlow<Ui> = _ui.asStateFlow()

    private val _failures = Channel<Int>(Channel.BUFFERED)
    /** String resources of the changes the HAL did not take, for the page to show once each. */
    val failures: Flow<Int> = _failures.receiveAsFlow()

    private var edits: Map<String, String?> = emptyMap()
    private var pollJob: Job? = null

    init {
        reload()
    }

    fun edit(key: String, value: String?) {
        edits = edits + (key to value)
        _ui.update { if (it is Ui.Loaded) Ui.Loaded(it.state.copy(edits = edits)) else it }
    }

    fun discardEdits() {
        edits = emptyMap()
        _ui.update { if (it is Ui.Loaded) Ui.Loaded(it.state.copy(edits = edits)) else it }
    }

    /** Applies the edits as a trial. The HAL restarts right after. */
    fun apply() {
        val changes =
            edits.map { (key, value) ->
                SettingChange().apply {
                    this.key = key
                    this.value = value
                }
            }
        if (changes.isEmpty()) return
        // Refused while a trial runs, or when the HAL can not store the settings (e.g. without
        // the SELinux label of its state directory). The edits stay for another try.
        restartingThen(R.string.settings_apply_failed) {
            HalControl.call { it.applySettings(changes.toTypedArray()) } != null
        }
    }

    fun keep() {
        viewModelScope.launch {
            // Refused when the trial ended already, or the settings can not be stored: the HAL
            // then goes back to the previous ones on its own.
            if (HalControl.call { it.keepSettings() } == null) {
                _failures.trySend(R.string.settings_keep_failed)
            }
            reload()
        }
    }

    fun revert() =
        restartingThen(R.string.settings_revert_failed) {
            HalControl.call { it.revertSettings() } != null
        }

    fun resetAll() =
        restartingThen(R.string.settings_reset_failed) {
            HalControl.call { it.resetSettings() } != null
        }

    /**
     * Reads the settings and the trial again, and keeps doing so while a trial counts down. After a
     * restart, [waitForHal] keeps trying while the HAL discovers the cards.
     */
    fun reload(waitForHal: Boolean = false) {
        pollJob?.cancel()
        pollJob = viewModelScope.launch { poll(waitForHal) }
    }

    private suspend fun poll(waitForHal: Boolean) {
        var attempts = if (waitForHal) RESTART_ATTEMPTS else 1
        while (true) {
            val state = load(restarting = false)
            if (state == null && --attempts > 0) continue
            _ui.value = state?.let { Ui.Loaded(it) } ?: Ui.Unavailable
            if (state == null || !state.trial.active) return
            delay(1_000)
        }
    }

    /**
     * Runs [request], which restarts the HAL on success, then waits for the HAL to come back. On
     * failure, shows [failure] and the state as the HAL has it.
     */
    private fun restartingThen(failure: Int, request: suspend () -> Boolean) {
        pollJob?.cancel()
        pollJob =
            viewModelScope.launch {
                _ui.update {
                    if (it is Ui.Loaded) Ui.Loaded(it.state.copy(restarting = true)) else it
                }
                if (!request()) {
                    _failures.trySend(failure)
                    reload()
                    return@launch
                }
                edits = emptyMap()
                // The HAL answers until it exits a moment later; wait for the new process.
                delay(RESTART_SETTLE_MS)
                poll(waitForHal = true)
            }
    }

    private suspend fun load(restarting: Boolean): State? {
        // Each call waits a while for the HAL to (re)appear, see HalControl.
        val settings = HalControl.call { it.settings } ?: return null
        val status = HalControl.status() ?: return null
        val trial = HalControl.call { it.trialStatus } ?: return null
        return State(
            settings.toList(),
            status.cards.toList(),
            edits,
            trial,
            restarting,
            status.passthroughFormats.orEmpty().toList(),
        )
    }

    private companion object {
        const val RESTART_SETTLE_MS = 1_500L

        /** Times [load] waits for a restarting HAL, which may wait for cards up to a minute. */
        const val RESTART_ATTEMPTS = 30
    }
}
