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

    const char* value = hwdb_sensor_get_property(ctx, modalias, property_key);
    if (value) {
        std::cout << "Match found!\n";
        std::cout << property_key << "=" << value << "\n";
    } else {
        std::cout << "No match found for modalias: " << modalias << "\n";
    }

    hwdb_sensor_free(ctx);
    return 0;
}
