/*
 * SPDX-FileCopyrightText: The LineageOS Project
 * SPDX-License-Identifier: Apache-2.0
 */

package vendor.mainline.audio.settings

import android.app.Application
import android.media.AudioAttributes
import android.media.AudioFormat
import android.media.AudioManager
import android.media.AudioTrack
import androidx.lifecycle.AndroidViewModel
import androidx.lifecycle.viewModelScope
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.Job
import kotlinx.coroutines.delay
import kotlinx.coroutines.flow.MutableStateFlow
import kotlinx.coroutines.flow.StateFlow
import kotlinx.coroutines.flow.asStateFlow
import kotlinx.coroutines.launch
import kotlinx.coroutines.withContext
import vendor.mainline.audio.EndpointStatus
import vendor.mainline.audio.OutputSettings

/** The processing of one output, applied as the user changes it. */
class OutputViewModel(application: Application) : AndroidViewModel(application) {
    sealed interface State {
        data object Loading : State

        data object Unavailable : State

        data class Loaded(val endpoint: EndpointStatus, val settings: OutputSettings) : State
    }

    private val _state = MutableStateFlow<State>(State.Loading)
    val state: StateFlow<State> = _state.asStateFlow()

    private var key: String? = null
    private var applyJob: Job? = null
    private var toneJob: Job? = null

    fun load(key: String) {
        if (this.key == key) return
        this.key = key
        viewModelScope.launch { _state.value = read(key) ?: State.Unavailable }
    }

    /** Changes the settings and applies them right away. */
    fun update(change: OutputSettings.() -> Unit) {
        val loaded = _state.value as? State.Loaded ?: return
        val settings = copy(loaded.settings).apply(change)
        normalize(settings)
        _state.value = loaded.copy(settings = settings)
        val key = key ?: return
        // The latest change wins; a slider sends many.
        applyJob?.cancel()
        applyJob =
            viewModelScope.launch {
                HalControl.call { it.setOutputSettings(key, settings) }
                AudioSummaryProvider.notifyChanged(getApplication())
            }
    }

    fun reset() {
        val key = key ?: return
        viewModelScope.launch {
            HalControl.call { it.resetOutputSettings(key) }
            _state.value = read(key) ?: State.Unavailable
        }
    }

    /**
     * Plays the test tone on [channel] for [TONE_MS]. The HAL only replaces what a stream plays, so
     * this also plays silence to the output for as long. The HAL ends the tone on its own.
     */
    fun playTestTone(channel: Int) {
        val loaded = _state.value as? State.Loaded ?: return
        val key = key ?: return
        toneJob?.cancel()
        toneJob =
            viewModelScope.launch {
                val track = withContext(Dispatchers.IO) { silentTrack(loaded.endpoint) }
                try {
                    track?.play()
                    HalControl.call { it.playTestTone(key, channel, TONE_MS) }
                    delay(TONE_MS.toLong())
                } finally {
                    track?.stop()
                    track?.release()
                }
            }
    }

    private suspend fun read(key: String): State.Loaded? {
        val endpoint =
            HalControl.status()?.endpoints?.firstOrNull { it.key == key && !it.input }
                ?: return null
        val settings = HalControl.call { it.getOutputSettings(key) } ?: return null
        normalize(settings)
        return State.Loaded(endpoint, settings)
    }

    /** A silent stream to the output's device, so that the test tone has a stream to replace. */
    private fun silentTrack(endpoint: EndpointStatus): AudioTrack? {
        val audioManager = getApplication<Application>().getSystemService(AudioManager::class.java)
        val device =
            findOutputDevice(
                audioManager.getDevices(AudioManager.GET_DEVICES_OUTPUTS).toList(),
                endpoint,
            ) ?: return null
        val format =
            AudioFormat.Builder()
                .setEncoding(AudioFormat.ENCODING_PCM_16BIT)
                .setSampleRate(SILENCE_RATE)
                .setChannelMask(AudioFormat.CHANNEL_OUT_STEREO)
                .build()
        val bytes = SILENCE_RATE * 4 * (TONE_MS + SILENCE_EXTRA_MS) / 1000
        val track =
            AudioTrack.Builder()
                .setAudioAttributes(
                    AudioAttributes.Builder()
                        .setUsage(AudioAttributes.USAGE_MEDIA)
                        .setContentType(AudioAttributes.CONTENT_TYPE_UNKNOWN)
                        .build()
                )
                .setAudioFormat(format)
                .setTransferMode(AudioTrack.MODE_STATIC)
                .setBufferSizeInBytes(bytes)
                .build()
        track.write(ByteArray(bytes), 0, bytes)
        track.setPreferredDevice(device)
        return track
    }

    private companion object {
        const val TONE_MS = 2_000
        const val SILENCE_EXTRA_MS = 500
        const val SILENCE_RATE = 48_000

        /** A copy, so that the state never shares arrays with an edit in progress. */
        fun copy(settings: OutputSettings) =
            OutputSettings().apply {
                layout = settings.layout
                fill = settings.fill
                swapLeftRight = settings.swapLeftRight
                mono = settings.mono
                subwoofer = settings.subwoofer
                smallSpeakers = settings.smallSpeakers
                crossoverHz = settings.crossoverHz
                levelsDb = settings.levelsDb?.copyOf() ?: FloatArray(0)
                delaysMs = settings.delaysMs?.copyOf() ?: FloatArray(0)
            }

        /** One level and one delay per speaker of the layout. */
        fun normalize(settings: OutputSettings) {
            val speakers = Speakers.of(settings.layout).size
            settings.levelsDb = FloatArray(speakers) { settings.levelsDb?.getOrNull(it) ?: 0f }
            settings.delaysMs = FloatArray(speakers) { settings.delaysMs?.getOrNull(it) ?: 0f }
        }
    }
}

/** The speakers of each OutputSettings layout, in the order of its levels and delays. */
enum class Speakers(val titleRes: Int) {
    FRONT_LEFT(R.string.speaker_front_left),
    FRONT_RIGHT(R.string.speaker_front_right),
    REAR_LEFT(R.string.speaker_rear_left),
    REAR_RIGHT(R.string.speaker_rear_right),
    CENTER(R.string.speaker_center),
    SUBWOOFER(R.string.speaker_subwoofer),
    SIDE_LEFT(R.string.speaker_side_left),
    SIDE_RIGHT(R.string.speaker_side_right);

    companion object {
        /** Speaker count of each layout; the HAL lists them in this enum's order. */
        fun of(layout: Int): List<Speakers> =
            entries.take(
                when (layout) {
                    OutputSettings.LAYOUT_STEREO -> 2
                    OutputSettings.LAYOUT_QUAD -> 4
                    OutputSettings.LAYOUT_5_1 -> 6
                    OutputSettings.LAYOUT_7_1 -> 8
                    else -> 0
                }
            )

        fun hasSubwoofer(layout: Int) =
            layout == OutputSettings.LAYOUT_5_1 || layout == OutputSettings.LAYOUT_7_1
    }
}
