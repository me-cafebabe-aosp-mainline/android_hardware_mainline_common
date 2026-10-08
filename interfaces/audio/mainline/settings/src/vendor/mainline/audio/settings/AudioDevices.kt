/*
 * SPDX-FileCopyrightText: The LineageOS Project
 * SPDX-License-Identifier: Apache-2.0
 */

package vendor.mainline.audio.settings

import android.media.AudioDeviceInfo
import vendor.mainline.audio.EndpointStatus

/**
 * The AIDL device description (as [EndpointStatus.deviceType] spells it) that AudioService maps to
 * an AudioDeviceInfo type, for the types the mainline HAL creates.
 */
internal fun halDeviceType(type: Int, input: Boolean): String? =
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

/** The connected AudioDeviceInfo behind an output endpoint of the HAL, null when none is. */
internal fun findOutputDevice(devices: List<AudioDeviceInfo>, endpoint: EndpointStatus) =
    devices.firstOrNull {
        it.isSink &&
            halDeviceType(it.type, input = false) == endpoint.deviceType &&
            (it.type != AudioDeviceInfo.TYPE_BUS || it.address == endpoint.address)
    }
