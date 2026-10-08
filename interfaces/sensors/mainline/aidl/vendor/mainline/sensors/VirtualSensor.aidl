/*
 * SPDX-FileCopyrightText: The LineageOS Project
 * SPDX-License-Identifier: Apache-2.0
 */

package vendor.mainline.sensors;

/**
 * A sensor of the virtual backend and the values it reports.
 */
@VintfStability
parcelable VirtualSensor {
    /**
     * Identifies the sensor in IVirtualSensors calls. Stable while the HAL
     * runs; it is not the handle the framework sees.
     */
    int handle;

    /** android.hardware.sensors.SensorType, e.g. 1 for ACCELEROMETER. */
    int type;

    /** The name the framework shows, e.g. "Virtual Accelerometer". */
    String name;

    /**
     * Range of every value, in the unit Android defines for the type. A
     * proximity sensor reports "near" as minValue and "far" as maxValue.
     */
    float minValue;
    float maxValue;

    /** Smallest step the framework expects between two values. */
    float resolution;

    /** What the sensor reports: x, y, z for 3-axis types, else one value. */
    float[] values;

    /** Whether values were set by the user rather than being the defaults. */
    boolean stored;

    /** Whether the framework is listening to the sensor right now. */
    boolean active;

    /** The sampling period the framework asked for, while active. */
    long samplingPeriodNs;
}
