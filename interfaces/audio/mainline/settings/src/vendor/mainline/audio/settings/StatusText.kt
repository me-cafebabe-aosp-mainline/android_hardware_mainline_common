/*
 * SPDX-FileCopyrightText: The LineageOS Project
 * SPDX-License-Identifier: Apache-2.0
 */

package vendor.mainline.audio.settings

import android.content.Context
import java.text.DecimalFormat
import vendor.mainline.audio.CardStatus
import vendor.mainline.audio.EndpointStatus
import vendor.mainline.audio.HdmiSinkStatus

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
        endpoint.hdmi?.let { lines += hdmi(it) }
        return lines.joinToString("\n")
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
        lines += context.getString(R.string.hdmi_hbr, sink.hbr)
        return lines.joinToString("\n")
    }

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
