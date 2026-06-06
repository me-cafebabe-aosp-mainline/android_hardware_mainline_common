# libhwdb-sensor

`libhwdb-sensor` is a lightweight, systemd-independent C/C++ library designed to parse the `60-sensor.hwdb` file. It allows hardware sensor parameters—such as mounting matrices and proximity thresholds—to be dynamically read directly from the hardware database.

This library is especially useful for embedded systems, Android HALs, and custom distributions where the full systemd stack is undesirable or unavailable, but standard hardware configuration compatibility is needed.

## Features

- **Standalone Parsing**: Parses standard systemd `.hwdb` formats natively.
- **Modalias Construction**: Automatically constructs full DMI-based `modalias` strings (e.g. for ACPI IIO devices) to match against the hardware database.
- **Fallback Support**: Includes integration with [smbios-parser](https://github.com/brunexgeek/smbios-parser) to fallback gracefully when standard `/sys/class/dmi/id/modalias` sysfs nodes are unavailable.
- **Flexible Pathing**: Automatically tries standard paths (`/etc/udev/hwdb.d/`, `/usr/lib/udev/hwdb.d/`), and explicitly supports an Android property override (`persist.vendor.sensor.hwdb_path`).
- **C API**: Provides a pure C API (`extern "C"`) to effortlessly integrate into existing C-based HALs and drivers.

## Usage

Include `hwdb_sensor.h` in your project.

```c
#include "hwdb_sensor.h"

// Initialize the context (pass NULL to auto-discover the hwdb path)
hwdb_sensor_context* ctx = hwdb_sensor_init(NULL);

if (ctx) {
    // 1. You can manually build a full DMI modalias for an IIO device:
    // (You would typically read raw_modalias from /sys/bus/iio/devices/.../modalias)
    const char* raw_modalias = "acpi:KIOX000A:KIOX000A:";
    char* full_modalias = hwdb_sensor_build_modalias(raw_modalias);

    // 2. Query properties:
    const char* matrix = hwdb_sensor_get_property(ctx, full_modalias, "ACCEL_MOUNT_MATRIX");
    if (matrix) {
        printf("Found matrix: %s\n", matrix);
    }

    free(full_modalias);
    hwdb_sensor_free(ctx);
}
```

## Building

### CMake (Standalone Testing)

You can build and run the test utility using standard CMake:

```bash
mkdir build && cd build
cmake ..
make
./test_hwdb
```

Alternatively, just run `./test.sh` from the root directory.

### Android AOSP Integration

The project includes an `Android.mk` to build natively within the Android tree. 
Make sure `smbios-parser` is cloned alongside `libhwdb-sensor` as a sibling directory, and add `libhwdb_sensor` to your `LOCAL_STATIC_LIBRARIES` in your module.
