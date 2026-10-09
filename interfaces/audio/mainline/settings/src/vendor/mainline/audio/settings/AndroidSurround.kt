/*
 * SPDX-FileCopyrightText: The LineageOS Project
 * SPDX-License-Identifier: Apache-2.0
 */

package vendor.mainline.audio.settings

import android.media.AudioAttributes
import android.media.AudioDeviceInfo
import android.media.AudioFormat
import android.media.AudioManager

/**
 * What Android itself makes of the HAL's passthrough formats, from public AudioManager APIs: the
 * HAL may offer a format that the audio policy never passes on to apps.
 */
object AndroidSurround {
    data class Info(
        /** One of AudioManager.ENCODED_SURROUND_OUTPUT_*. */
        val surroundMode: Int,
        /** Formats Android knows the connected HDMI device takes; null without one. */
        val hdmiFormats: List<String>?,
        /**
         * Per passthrough format name: whether Android tells apps they can send it as a bitstream
         * to the current media output.
         */
        val bitstream: Map<String, Boolean>,
    )

    /** Names as in hdmi.passthrough_formats, and the AudioFormat encodings they stand for. */
    private val ENCODINGS =
        linkedMapOf(
            "ac3" to AudioFormat.ENCODING_AC3,
            "eac3" to AudioFormat.ENCODING_E_AC3,
            "eac3-joc" to AudioFormat.ENCODING_E_AC3_JOC,
            "dts" to AudioFormat.ENCODING_DTS,
            "dtshd" to AudioFormat.ENCODING_DTS_HD,
            "dtshd-ma" to AudioFormat.ENCODING_DTS_HD_MA,
            "truehd" to AudioFormat.ENCODING_DOLBY_TRUEHD,
            "iec61937" to AudioFormat.ENCODING_IEC61937,
        )

    private val MEDIA =
        AudioAttributes.Builder()
            .setUsage(AudioAttributes.USAGE_MEDIA)
            .setContentType(AudioAttributes.CONTENT_TYPE_MOVIE)
            .build()

    /**
     * Reads Android's view of [formats] (passthrough format names). Calls into AudioService and the
     * audio policy: run it off the main thread.
     */
    fun read(audioManager: AudioManager, formats: List<String>): Info {
        val hdmi =
            audioManager.getDevices(AudioManager.GET_DEVICES_OUTPUTS).firstOrNull {
                it.type == AudioDeviceInfo.TYPE_HDMI
            }
        val hdmiFormats =
            hdmi?.let { device ->
                val encodings = device.encodings.toSet()
                ENCODINGS.filterValues { it in encodings }.keys.toList()
            }
        val bitstream =
            formats.associateWith { name ->
                val encoding = ENCODINGS[name] ?: return@associateWith false
                // Stereo at 48 kHz, which the HAL offers for every format a sink
                // announces at 48 kHz, as nearly all do.
                val format =
                    AudioFormat.Builder()
                        .setEncoding(encoding)
                        .setSampleRate(48000)
                        .setChannelMask(AudioFormat.CHANNEL_OUT_STEREO)
                        .build()
                (AudioManager.getDirectPlaybackSupport(format, MEDIA) and
                    AudioManager.DIRECT_PLAYBACK_BITSTREAM_SUPPORTED) != 0
            }
        return Info(audioManager.encodedSurroundMode, hdmiFormats, bitstream)
    }
}
