/*
 * Copyright (C) 2026 BlissLabs
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef HWDB_SENSOR_H
#define HWDB_SENSOR_H

#ifdef __cplusplus
extern "C" {
#endif

typedef struct hwdb_sensor_context hwdb_sensor_context;

/**
 * Initialize the hwdb sensor context.
 * 
 * @param custom_path Optional path to the 60-sensor.hwdb file.
 *                    If NULL, it will attempt to find the hwdb file in:
 *                    1. Android property `persist.vendor.sensor.hwdb_path` (if built on Android)
 *                    2. /etc/udev/hwdb.d/60-sensor.hwdb
 *                    3. /usr/lib/udev/hwdb.d/60-sensor.hwdb
 * @return hwdb_sensor_context pointer on success, NULL on failure.
 */
hwdb_sensor_context* hwdb_sensor_init(const char* custom_path);

/**
 * Free the hwdb sensor context.
 * 
 * @param ctx The context to free.
 */
void hwdb_sensor_free(hwdb_sensor_context* ctx);

/**
 * Get a property value from the hwdb for a specific modalias.
 * 
 * @param ctx The hwdb sensor context.
 * @param modalias The full modalias string to match against.
 * @param property_key The key of the property to retrieve (e.g. "ACCEL_MOUNT_MATRIX").
 * @return The property value string, or NULL if not found or no match.
 *         The returned string is valid as long as the context is not freed.
 */
const char* hwdb_sensor_get_property(hwdb_sensor_context* ctx, const char* modalias, const char* property_key);

/**
 * Build the full modalias string given a sensor's modalias.
 * It will append the DMI info from `/sys/class/dmi/id/modalias`.
 * 
 * @param sensor_modalias The modalias of the sensor, usually read from 
 *                        `/sys/bus/iio/devices/iio:deviceX/../modalias`
 * @return A newly allocated string containing the full modalias, or NULL on failure.
 *         The caller is responsible for freeing the string using `free()`.
 */
char* hwdb_sensor_build_modalias(const char* sensor_modalias);

#ifdef __cplusplus
}
#endif

#endif // HWDB_SENSOR_H
