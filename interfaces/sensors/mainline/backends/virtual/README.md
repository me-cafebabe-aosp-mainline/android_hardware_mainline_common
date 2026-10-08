# Virtual backend (`libsensors_virtual.so`)

Provides sensors whose values the user sets, through
`vendor.mainline.sensors.IVirtualSensors/default` (see [`../../aidl`](../../aidl)),
normally from the settings app of the HAL's APEX
([`../../settings`](../../settings/README.md)). Useful on devices without
the sensor hardware (virtual machines, boards, tablets without a proximity
sensor) and to try apps with specific sensor values.

The backend is only bundled when the build sets the soong config variable
`sensors_hal_mainline.include_virtual_backend`; it is then part of the
built-in default backend list (`iio,input,virtual,mock`). A device with its
own list (`load_custom_backends` or the `backends` setting) has to add
`virtual` to it.

Unlike the mock backend it is **not** fallback only: a virtual sensor is
exposed next to a real one of the same type, and most apps use the first
sensor of a type. Limit `virtual.sensors` to the types the device lacks, or
put `virtual` first in the backend list to make the virtual sensors the
defaults.

## Sensors

| Type                  | Config name   | Values           | Default                  |
|-----------------------|---------------|------------------|--------------------------|
| `ACCELEROMETER`       | `accel`       | x, y, z (m/s²)   | lying flat: 0, 0, 9.81   |
| `GYROSCOPE`           | `gyro`        | x, y, z (rad/s)  | 0, 0, 0                  |
| `MAGNETIC_FIELD`      | `magn`        | x, y, z (µT)     | 0, 22, -42               |
| `GRAVITY`             | `gravity`     | x, y, z (m/s²)   | 0, 0, 9.81               |
| `LINEAR_ACCELERATION` | `linear_accel`| x, y, z (m/s²)   | 0, 0, 0                  |
| `LIGHT`               | `light`       | lux              | 200                      |
| `PRESSURE`            | `pressure`    | hPa              | 1013.25                  |
| `PROXIMITY`           | `proximity`   | cm (0 = near)    | far (maximum range)      |
| `AMBIENT_TEMPERATURE` | `temperature` | °C (from -40)    | 22                       |
| `RELATIVE_HUMIDITY`   | `humidity`    | %                | 40                       |
| `HINGE_ANGLE`         | `hinge_angle` | degrees          | 180                      |

Ranges, resolution, power and delays are the type defaults of
`utils/common/SensorTypes.cpp`; 3-axis values range from `-maxRange` to
`maxRange`. Sensors are named "Virtual <type>", e.g. "Virtual Accelerometer".

Continuous sensors repeat the current values at the requested rate;
on-change sensors report once on activation and then whenever the values
change. New values are reported right away.

## Stored values

Values set through `IVirtualSensors.setValues()` are kept in
`/data/vendor/mainline_sensors/virtual` (one `<config name>\t<values>` line per
sensor), which the APEX's `sensors-mainline-virtual.rc` creates, and are
restored when the HAL starts. `resetValues()` drops them. Without the
directory the values only last until the HAL restarts.

## Configuration

| Setting key       | Property                         | Meaning                                                            |
|-------------------|----------------------------------|--------------------------------------------------------------------|
| `virtual.sensors` | `vendor.sensors.virtual.sensors` | Comma separated list of sensor types to create, or `none`. Default: `accel,gyro,magn,light,proximity,pressure,temperature,humidity`. |

Type names are the configuration names above or the AIDL enum names. Each
type is created once. The sensor list is read by the framework once, so a
change only takes effect after a reboot.

## Control interface

`vendor.mainline.sensors.IVirtualSensors/default` is registered by the backend
the first time it initializes, also without sensors, and is served by the HAL
process. Its handles are the backend's own. The APEX declares it (VINTF
fragment `sensors-mainline-virtual.xml`) whenever the backend is bundled, so a
backend list without `virtual` leaves a declared but missing service.

Every change and restored value is logged:

```
adb logcat -s MainlineSensorsVirtual
```
