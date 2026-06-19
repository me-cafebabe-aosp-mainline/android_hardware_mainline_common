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

#include "hwdb_sensor.h"
#include <iostream>
#include <cstdlib>

int main(int argc, char** argv) {
    if (argc < 3) {
        std::cerr << "Usage: " << argv[0] << " <hwdb_path> <modalias_pattern> [property_key]\n";
        std::cerr << "Example: " << argv[0] << " /media/work/sensors-work/systemd/hwdb.d/60-sensor.hwdb \"sensor:modalias:acpi:BOSC0200:*:dmi:*:svnAcer:*:rnAlien_AP:*\" ACCEL_MOUNT_MATRIX\n";
        return 1;
    }

    const char* hwdb_path = argv[1];
    const char* modalias = argv[2];
    const char* property_key = (argc > 3) ? argv[3] : "ACCEL_MOUNT_MATRIX";

    hwdb_sensor_context* ctx = hwdb_sensor_init(hwdb_path);
    if (!ctx) {
        std::cerr << "Failed to initialize hwdb parser with path: " << hwdb_path << "\n";
        return 1;
    }

    char* full_modalias = hwdb_sensor_build_modalias(modalias);
    if (!full_modalias) {
        std::cerr << "Failed to build full modalias string.\n";
        hwdb_sensor_free(ctx);
        return 1;
    }

    std::cout << "Built modalias string: " << full_modalias << "\n";

    const char* value = hwdb_sensor_get_property(ctx, full_modalias, property_key);
    if (value) {
        std::cout << "Match found!\n";
        std::cout << property_key << "=" << value << "\n";
    } else {
        std::cout << "No match found for modalias: " << full_modalias << "\n";
    }

    free(full_modalias);
    hwdb_sensor_free(ctx);
    return 0;
}
