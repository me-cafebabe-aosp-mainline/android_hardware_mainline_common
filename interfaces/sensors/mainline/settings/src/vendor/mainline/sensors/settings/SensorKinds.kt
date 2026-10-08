/*
 * SPDX-FileCopyrightText: The LineageOS Project
 * SPDX-License-Identifier: Apache-2.0
 */

package vendor.mainline.sensors.settings

import android.content.Context
import android.hardware.Sensor
import vendor.mainline.sensors.VirtualSensor

/**
 * How the values of a virtual sensor are presented. VirtualSensor.type is the HAL's SensorType,
 * whose values are the same as the framework's Sensor.TYPE_* constants.
 */
object SensorKinds {
    fun isProximity(sensor: VirtualSensor) = sensor.type == Sensor.TYPE_PROXIMITY

    /** Whether a device position sets the sensor, see [DevicePosition.valuesFor]. */
    fun isMotion(sensor: VirtualSensor) =
        sensor.type == Sensor.TYPE_ACCELEROMETER || sensor.type == Sensor.TYPE_GRAVITY

    /** The unit of every value of the sensor, e.g. "m/s²"; empty for an unknown type. */
    fun unit(context: Context, sensor: VirtualSensor): String =
        context.getString(
            when (sensor.type) {
                Sensor.TYPE_ACCELEROMETER,
                Sensor.TYPE_GRAVITY,
                Sensor.TYPE_LINEAR_ACCELERATION -> R.string.unit_acceleration
                Sensor.TYPE_GYROSCOPE -> R.string.unit_angular_speed
                Sensor.TYPE_MAGNETIC_FIELD -> R.string.unit_magnetic_field
                Sensor.TYPE_LIGHT -> R.string.unit_light
                Sensor.TYPE_PRESSURE -> R.string.unit_pressure
                Sensor.TYPE_PROXIMITY -> R.string.unit_distance
                Sensor.TYPE_AMBIENT_TEMPERATURE -> R.string.unit_temperature
                Sensor.TYPE_RELATIVE_HUMIDITY -> R.string.unit_humidity
                Sensor.TYPE_HINGE_ANGLE -> R.string.unit_angle
                else -> R.string.unit_none
            }
        )

    /** A title for each of the sensor's values. */
    fun valueTitles(context: Context, sensor: VirtualSensor): List<String> =
        if (sensor.values.size == 3) {
            listOf(R.string.axis_x, R.string.axis_y, R.string.axis_z).map(context::getString)
        } else {
            List(sensor.values.size) { context.getString(R.string.value_title) }
        }
}
