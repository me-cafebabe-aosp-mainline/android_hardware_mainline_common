/*
 * SPDX-FileCopyrightText: The LineageOS Project
 * SPDX-License-Identifier: Apache-2.0
 */

package vendor.mainline.sensors;

import vendor.mainline.sensors.VirtualSensor;

/**
 * Control interface of the virtual sensors backend of the mainline Sensors
 * HAL, served next to android.hardware.sensors.ISensors/default by the same
 * process while that backend is loaded. It is meant for the settings app
 * shipped in the HAL's APEX, not for the framework: the framework only talks
 * to ISensors and sees virtual sensors like any other.
 */
@VintfStability
interface IVirtualSensors {
    /** Every virtual sensor, in the order of the HAL's sensor list. */
    VirtualSensor[] getSensors();

    /**
     * Sets what a sensor reports from now on and stores it, so that it
     * survives a restart of the HAL. `values` must have as many entries as
     * VirtualSensor.values; each is clamped to the sensor's range. Fails with
     * EX_ILLEGAL_ARGUMENT for an unknown handle, a wrong number of values or
     * a value that is not a number.
     */
    void setValues(int handle, in float[] values);

    /**
     * Returns a sensor to its default values and forgets the stored ones.
     * Fails with EX_ILLEGAL_ARGUMENT for an unknown handle.
     */
    void resetValues(int handle);
}
