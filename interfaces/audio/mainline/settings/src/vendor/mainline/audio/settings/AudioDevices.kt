/*
 * SPDX-FileCopyrightText: The LineageOS Project
 * SPDX-License-Identifier: Apache-2.0
 */

package vendor.mainline.audio.settings

import android.content.Context
import android.media.AudioDeviceAttributes
import android.media.AudioDeviceInfo
import vendor.mainline.audio.EndpointStatus
import vendor.mainline.audio.Status

/**
 * Identifies a device the way the routing preferences of AudioService do: role, type and address.
 * [AudioDeviceAttributes.equals] also compares the name, which a stored preference does not have.
 */
data class DeviceId(val role: Int, val type: Int, val address: String) {
    constructor(
        attributes: AudioDeviceAttributes
    ) : this(attributes.role, attributes.type, attributes.address)

    fun toAttributes() = AudioDeviceAttributes(role, type, address)

    /** For SharedPreferences: "<role>|<type>|<address>". */
    fun serialize() = "$role|$type|$address"

    companion object {
        fun of(info: AudioDeviceInfo) =
            DeviceId(
                if (info.isSink) AudioDeviceAttributes.ROLE_OUTPUT
                else AudioDeviceAttributes.ROLE_INPUT,
                info.type,
                info.address,
            )

        fun parse(value: String?): DeviceId? {
            val parts = value?.split('|', limit = 3) ?: return null
            if (parts.size != 3) return null
            val role = parts[0].toIntOrNull() ?: return null
            val type = parts[1].toIntOrNull() ?: return null
            return DeviceId(role, type, parts[2])
        }
    }
}

/** A device the user can pick. */
data class DeviceOption(val id: DeviceId, val label: String, val connected: Boolean)

/** Device types that are no user choice: internal, telephony or tuner paths. */
private val HIDDEN_TYPES =
    setOf(
        AudioDeviceInfo.TYPE_REMOTE_SUBMIX,
        AudioDeviceInfo.TYPE_TELEPHONY,
        AudioDeviceInfo.TYPE_FM,
        AudioDeviceInfo.TYPE_FM_TUNER,
        AudioDeviceInfo.TYPE_TV_TUNER,
        AudioDeviceInfo.TYPE_BUILTIN_SPEAKER_SAFE,
    )

fun AudioDeviceInfo.isUserChoice() = type !in HIDDEN_TYPES

/**
 * Names devices. Devices of the mainline HAL get the name of their device port, e.g. "PCH: HDMI 1",
 * matched through [EndpointStatus.deviceType]; others get their product name or a type name.
 */
class DeviceLabels(private val context: Context, status: Status?) {
    private val endpoints = status?.endpoints.orEmpty()

    fun label(info: AudioDeviceInfo): String =
        halName(DeviceId.of(info))
            ?: info.productName?.toString()?.takeIf { it.isNotBlank() && isExternal(info.type) }
            ?: typeName(DeviceId.of(info))

    /** For a device that is not connected right now: no product name to go by. */
    fun label(id: DeviceId): String = halName(id) ?: typeName(id)

    private fun halName(id: DeviceId): String? {
        val input = id.role == AudioDeviceAttributes.ROLE_INPUT
        val deviceType = halDeviceType(id.type, input) ?: return null
        return endpoints
            .firstOrNull {
                it.input == input &&
                    it.deviceType == deviceType &&
                    (id.type != AudioDeviceInfo.TYPE_BUS || it.address == id.address)
            }
            ?.name
    }

    private fun typeName(id: DeviceId): String {
        val input = id.role == AudioDeviceAttributes.ROLE_INPUT
        val name =
            when (id.type) {
                AudioDeviceInfo.TYPE_BUILTIN_SPEAKER -> context.getString(R.string.device_speaker)
                AudioDeviceInfo.TYPE_BUILTIN_EARPIECE -> context.getString(R.string.device_earpiece)
                AudioDeviceInfo.TYPE_WIRED_HEADPHONES ->
                    context.getString(R.string.device_headphones)
                AudioDeviceInfo.TYPE_WIRED_HEADSET ->
                    context.getString(
                        if (input) R.string.device_headset_mic else R.string.device_headset
                    )
                AudioDeviceInfo.TYPE_LINE_ANALOG ->
                    context.getString(
                        if (input) R.string.device_line_in else R.string.device_line_out
                    )
                AudioDeviceInfo.TYPE_HDMI -> context.getString(R.string.device_hdmi)
                AudioDeviceInfo.TYPE_HDMI_ARC,
                AudioDeviceInfo.TYPE_HDMI_EARC -> context.getString(R.string.device_hdmi_arc)
                AudioDeviceInfo.TYPE_LINE_DIGITAL -> context.getString(R.string.device_spdif)
                AudioDeviceInfo.TYPE_USB_DEVICE,
                AudioDeviceInfo.TYPE_USB_HEADSET,
                AudioDeviceInfo.TYPE_USB_ACCESSORY -> context.getString(R.string.device_usb)
                AudioDeviceInfo.TYPE_BLUETOOTH_A2DP,
                AudioDeviceInfo.TYPE_BLUETOOTH_SCO,
                AudioDeviceInfo.TYPE_BLE_HEADSET,
                AudioDeviceInfo.TYPE_BLE_SPEAKER,
                AudioDeviceInfo.TYPE_BLE_BROADCAST,
                AudioDeviceInfo.TYPE_HEARING_AID -> context.getString(R.string.device_bluetooth)
                AudioDeviceInfo.TYPE_BUILTIN_MIC -> context.getString(R.string.device_mic)
                else -> context.getString(R.string.device_other, id.type)
            }
        return if (id.type == AudioDeviceInfo.TYPE_BUS && id.address.isNotEmpty()) {
            context.getString(R.string.device_with_address, name, id.address)
        } else {
            name
        }
    }

    private fun isExternal(type: Int) =
        type !in
            setOf(
                AudioDeviceInfo.TYPE_BUILTIN_SPEAKER,
                AudioDeviceInfo.TYPE_BUILTIN_EARPIECE,
                AudioDeviceInfo.TYPE_BUILTIN_MIC,
                AudioDeviceInfo.TYPE_BUS,
            )

    private companion object {
        /**
         * The AIDL device description (as [EndpointStatus.deviceType] spells it) that AudioService
         * maps to an AudioDeviceInfo type, for the types the mainline HAL creates.
         */
        fun halDeviceType(type: Int, input: Boolean): String? =
            if (input) {
                when (type) {
                    AudioDeviceInfo.TYPE_BUILTIN_MIC -> "IN_MICROPHONE"
                    AudioDeviceInfo.TYPE_WIRED_HEADSET -> "IN_HEADSET analog"
                    AudioDeviceInfo.TYPE_BUS -> "IN_BUS"
                    else -> null
                }
            } else {
                when (type) {
                    AudioDeviceInfo.TYPE_BUILTIN_SPEAKER -> "OUT_SPEAKER"
                    AudioDeviceInfo.TYPE_BUILTIN_EARPIECE -> "OUT_SPEAKER_EARPIECE"
                    AudioDeviceInfo.TYPE_WIRED_HEADPHONES -> "OUT_HEADPHONE analog"
                    AudioDeviceInfo.TYPE_WIRED_HEADSET -> "OUT_HEADSET analog"
                    AudioDeviceInfo.TYPE_LINE_ANALOG -> "OUT_DEVICE analog"
                    AudioDeviceInfo.TYPE_HDMI -> "OUT_DEVICE hdmi"
                    AudioDeviceInfo.TYPE_LINE_DIGITAL -> "OUT_DEVICE spdif"
                    AudioDeviceInfo.TYPE_BUS -> "OUT_BUS"
                    else -> null
                }
            }
    }
}
