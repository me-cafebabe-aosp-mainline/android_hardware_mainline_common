/*
 * SPDX-FileCopyrightText: The LineageOS Project
 * SPDX-License-Identifier: Apache-2.0
 */

package vendor.mainline.audio.settings

import android.content.Context
import android.media.AudioManager
import android.system.Os
import android.text.format.DateUtils
import java.text.DecimalFormat
import vendor.mainline.audio.CardStatus
import vendor.mainline.audio.EndpointStatus
import vendor.mainline.audio.HdmiSinkStatus
import vendor.mainline.audio.PassthroughAttempt
import vendor.mainline.audio.PassthroughProbe
import vendor.mainline.audio.Status

/** Human readable text for the parcelables of [vendor.mainline.audio.Status]. */
class StatusText(private val context: Context) {
    private val kiloHertz = DecimalFormat("0.#")

    fun cardTitle(card: CardStatus) =
        context.getString(R.string.card_title, card.index, card.name.ifEmpty { card.id })

    fun cardSummary(card: CardStatus): String {
        val lines = mutableListOf(context.getString(R.string.card_driver, card.driver, card.id))
        if (card.mixerName.isNotEmpty()) {
            lines += context.getString(R.string.card_mixer, card.mixerName)
        }
        lines +=
            if (card.ucmVerb.isEmpty()) {
                context.getString(R.string.card_no_ucm)
            } else {
                context.getString(
                    R.string.card_ucm,
                    card.ucmVerb,
                    card.ucmEnabledDevices.joinToString().ifEmpty { none() },
                )
            }
        if (card.primary) lines += context.getString(R.string.card_primary)
        return lines.joinToString("\n")
    }

    fun endpointSummary(endpoint: EndpointStatus): String {
        val lines = mutableListOf<String>()
        val kind =
            context.getString(
                when {
                    endpoint.isDefault -> R.string.endpoint_default
                    endpoint.attached -> R.string.endpoint_attached
                    else -> R.string.endpoint_external
                }
            )
        lines += context.getString(R.string.endpoint_role, endpoint.role, kind)
        if (endpoint.nullDevice) {
            lines +=
                context.getString(
                    if (endpoint.input) R.string.endpoint_null_input
                    else R.string.endpoint_null_output
                )
            return lines.joinToString("\n")
        }
        lines +=
            if (endpoint.ucmDevice.isEmpty()) {
                context.getString(R.string.endpoint_path, endpoint.pcm)
            } else {
                context.getString(R.string.endpoint_path_ucm, endpoint.pcm, endpoint.ucmDevice)
            }
        lines += capabilities(endpoint)
        state(endpoint)?.let { lines += it }
        fast(endpoint)?.let { lines += it }
        endpoint.hdmi?.let { lines += hdmi(it) }
        return lines.joinToString("\n")
    }

    /** Whether fast_latency_ms gave the primary output a FastMixer. */
    fun fastSummary(status: Status): String =
        when {
            status.fastLatencyMs <= 0 -> context.getString(R.string.fast_off)
            status.fastActive ->
                context.getString(
                    R.string.fast_active,
                    status.fastLatencyMs,
                    status.fastBufferFrames,
                    status.normalMixerFrames,
                )
            else ->
                context.getString(
                    R.string.fast_inactive,
                    status.fastLatencyMs,
                    status.fastBufferFrames,
                    status.normalMixerFrames,
                )
        }

    /** What Android makes of the passthrough formats, one line each. */
    fun androidSummary(info: AndroidSurround.Info): String {
        val lines = mutableListOf<String>()
        lines += context.getString(R.string.android_surround_mode, surroundMode(info.surroundMode))
        lines +=
            info.hdmiFormats?.let {
                context.getString(
                    R.string.android_hdmi_formats,
                    it.joinToString().ifEmpty { none() },
                )
            } ?: context.getString(R.string.android_no_hdmi)
        val (offered, missing) = info.bitstream.entries.partition { it.value }
        lines +=
            context.getString(
                R.string.android_bitstream,
                offered.joinToString { it.key }.ifEmpty { none() },
            )
        if (missing.isNotEmpty()) {
            lines +=
                context.getString(R.string.android_no_bitstream, missing.joinToString { it.key })
        }
        return lines.joinToString("\n")
    }

    /** One line per configuration of an HDMI probe. */
    fun probeLines(results: List<PassthroughProbe>): String =
        results.joinToString("\n") { result ->
            val config =
                context.getString(
                    if (result.hbr) R.string.probe_config_hbr else R.string.probe_config,
                    result.channels,
                    kiloHertz.format(result.rate / 1000.0),
                )
            when {
                result.accepted -> context.getString(R.string.probe_accepted, config)
                result.error == 0 ->
                    context.getString(
                        R.string.probe_refused_no_error,
                        config,
                        step(result.failedStep.orEmpty()),
                    )
                else ->
                    context.getString(
                        R.string.probe_refused,
                        config,
                        step(result.failedStep.orEmpty()),
                        errorText(result.error),
                    )
            }
        }

    private fun fast(endpoint: EndpointStatus): String? {
        if (endpoint.fastBurstFrames <= 0) return null
        return context.getString(
            if (endpoint.fastPeriodFrames > endpoint.fastBurstFrames) R.string.fast_period_long
            else R.string.fast_period,
            endpoint.fastPeriodFrames,
            endpoint.fastBurstFrames,
        )
    }

    private fun capabilities(endpoint: EndpointStatus): String {
        val channels =
            if (endpoint.minChannels == endpoint.maxChannels) {
                endpoint.maxChannels.toString()
            } else {
                "${endpoint.minChannels}–${endpoint.maxChannels}"
            }
        return context.getString(
            R.string.endpoint_caps,
            endpoint.formats.joinToString(),
            rates(endpoint.sampleRates),
            channels,
        )
    }

    private fun state(endpoint: EndpointStatus): String? {
        val parts = mutableListOf<String>()
        when (endpoint.jack) {
            EndpointStatus.JACK_PLUGGED -> parts += context.getString(R.string.jack_plugged)
            EndpointStatus.JACK_UNPLUGGED -> parts += context.getString(R.string.jack_unplugged)
        }
        if (endpoint.activeStreams > 0) {
            parts +=
                context.resources.getQuantityString(
                    R.plurals.endpoint_streams,
                    endpoint.activeStreams,
                    endpoint.activeStreams,
                )
        }
        if (endpoint.pcmOwner.isNotEmpty()) {
            parts += context.getString(R.string.endpoint_owner, endpoint.pcmOwner)
        }
        val silenced = endpoint.silencedStreams.orEmpty()
        if (silenced.isNotEmpty()) {
            parts += context.getString(R.string.endpoint_silenced, silenced.joinToString())
        }
        return parts.takeIf { it.isNotEmpty() }?.joinToString(" · ")
    }

    private fun hdmi(sink: HdmiSinkStatus): String {
        if (!sink.passthroughPossible) return context.getString(R.string.hdmi_no_passthrough)
        val formats =
            sink.formats.joinToString { format ->
                context.getString(
                    R.string.hdmi_format,
                    format.name,
                    format.maxChannels,
                    rates(format.sampleRates),
                )
            }
        val lines = mutableListOf<String>()
        if (sink.monitorName.isNotEmpty()) {
            lines += context.getString(R.string.hdmi_sink, sink.monitorName)
        }
        lines +=
            context.getString(R.string.hdmi_passthrough, formats.ifEmpty { none() }, sink.source)
        lines += hbr(sink)
        sink.lastAttempt?.let { lines += attempt(it) }
        return lines.joinToString("\n")
    }

    private fun hbr(sink: HdmiSinkStatus): String {
        val support =
            context.getString(
                when (sink.hbr) {
                    "supported" -> R.string.hbr_supported
                    "unsupported" -> R.string.hbr_unsupported
                    else -> R.string.hbr_undetectable
                }
            )
        return when (sink.hbrSource) {
            "failure" -> context.getString(R.string.hdmi_hbr_failure, sink.hbrDetail.orEmpty())
            "setting" -> context.getString(R.string.hdmi_hbr_setting, support)
            "driver" ->
                context.getString(R.string.hdmi_hbr_driver, support, sink.hbrDetail.orEmpty())
            else -> context.getString(R.string.hdmi_hbr, support)
        }
    }

    private fun attempt(attempt: PassthroughAttempt): String {
        val time =
            DateUtils.formatDateTime(
                context,
                attempt.timeMillis,
                DateUtils.FORMAT_SHOW_TIME or DateUtils.FORMAT_SHOW_DATE,
            )
        val rate = kiloHertz.format(attempt.contentRate / 1000.0)
        return if (attempt.succeeded) {
            context.getString(
                R.string.attempt_ok,
                attempt.format,
                rate,
                attempt.pcmChannels,
                kiloHertz.format(attempt.pcmRate / 1000.0),
                time,
            )
        } else if (attempt.error == 0) {
            context.getString(
                R.string.attempt_failed_no_error,
                attempt.format,
                rate,
                step(attempt.failedStep.orEmpty()),
                time,
            )
        } else {
            context.getString(
                R.string.attempt_failed,
                attempt.format,
                rate,
                step(attempt.failedStep.orEmpty()),
                errorText(attempt.error),
                time,
            )
        }
    }

    /** The HAL's name of a failed step, readable. */
    private fun step(step: String): String =
        when (step) {
            "format" -> context.getString(R.string.step_format)
            "packer" -> context.getString(R.string.step_packer)
            "channel status" -> context.getString(R.string.step_channel_status)
            else -> step
        }

    /** "Invalid argument (-22)" for a negative errno. */
    private fun errorText(error: Int): String =
        context.getString(R.string.error_errno, Os.strerror(-error), error)

    private fun surroundMode(mode: Int): String =
        context.getString(
            when (mode) {
                AudioManager.ENCODED_SURROUND_OUTPUT_AUTO -> R.string.surround_auto
                AudioManager.ENCODED_SURROUND_OUTPUT_NEVER -> R.string.surround_never
                AudioManager.ENCODED_SURROUND_OUTPUT_ALWAYS -> R.string.surround_always
                AudioManager.ENCODED_SURROUND_OUTPUT_MANUAL -> R.string.surround_manual
                else -> R.string.surround_unknown
            }
        )

    /** "44.1, 48, 96 kHz" */
    private fun rates(rates: IntArray): String =
        if (rates.isEmpty()) {
            none()
        } else {
            context.getString(
                R.string.rates_khz,
                rates.sorted().joinToString { kiloHertz.format(it / 1000.0) },
            )
        }

    private fun none() = context.getString(R.string.none)
}
