/*
 * SPDX-FileCopyrightText: The LineageOS Project
 * SPDX-License-Identifier: Apache-2.0
 */

package vendor.mainline.sensors.settings

import android.hardware.Sensor
import android.hardware.SensorManager
import kotlin.math.abs
import vendor.mainline.sensors.VirtualSensor

/**
 * A device held still in a position. (x, y, z) is the axis that points up, in device coordinates: x
 * to the right and y to the top of the screen in its natural orientation, z out of the screen. At
 * rest an accelerometer reports +1 g along that axis.
 */
enum class DevicePosition(val titleRes: Int, private val up: FloatArray) {
    PORTRAIT(R.string.position_portrait, floatArrayOf(0f, 1f, 0f)),
    LANDSCAPE_LEFT(R.string.position_landscape_left, floatArrayOf(1f, 0f, 0f)),
    REVERSE_PORTRAIT(R.string.position_reverse_portrait, floatArrayOf(0f, -1f, 0f)),
    LANDSCAPE_RIGHT(R.string.position_landscape_right, floatArrayOf(-1f, 0f, 0f)),
    FACE_UP(R.string.position_face_up, floatArrayOf(0f, 0f, 1f)),
    FACE_DOWN(R.string.position_face_down, floatArrayOf(0f, 0f, -1f));

    /**
     * What [sensor] reports in this position: gravity for the accelerometer and gravity sensors,
     * nothing for linear acceleration and rotation, null for sensors a position does not set.
     */
    fun valuesFor(sensor: VirtualSensor): FloatArray? =
        when (sensor.type) {
            Sensor.TYPE_ACCELEROMETER,
            Sensor.TYPE_GRAVITY -> FloatArray(3) { up[it] * SensorManager.GRAVITY_EARTH }
            Sensor.TYPE_LINEAR_ACCELERATION,
            Sensor.TYPE_GYROSCOPE -> FloatArray(3)
            else -> null
        }

    companion object {
        /** The position [sensor] (an accelerometer or gravity sensor) reports, if any. */
        fun of(sensor: VirtualSensor): DevicePosition? =
            entries.firstOrNull { position ->
                val expected = position.valuesFor(sensor) ?: return@firstOrNull false
                expected.size == sensor.values.size &&
                    expected.indices.all { abs(expected[it] - sensor.values[it]) < 0.01f }
            }
    }
}
