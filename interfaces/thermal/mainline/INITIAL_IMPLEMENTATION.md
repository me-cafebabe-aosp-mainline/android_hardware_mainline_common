# Details about making the initial implementation of Android Thermal HAL for mainline kernel

We want a flexible and generic Android Thermal HAL,
for the usage on Android devices running mainline Linux kernel,
which follows proper Linux standards.

You (AI Coding Agent) act as a professional Android HAL engineer
and you've got to implement this in the directory containing this markdown file.

> See the repository root `docs/INITIAL_IMPLEMENTATION_GUIDELINES.md`
> (`hardware/mainline/common/docs/INITIAL_IMPLEMENTATION_GUIDELINES.md`) for
> requirements, references, and guidelines shared by every component. This
> file only lists what's specific to this HAL.

## Requirements

- The HAL shall be named `mainline`.
  - Init rc service name shall be `vendor.thermal-mainline`.
  - The APEX module name shall be `com.android.hardware.thermal.mainline`.
  - Filename for the executable shall be `android.hardware.thermal-service.mainline`.
  - `LOG_TAG` define on top of our source files shall begin with `MainlineThermal`.
  - Init rc and vintf fragment shall be renamed accordingly.
- Libraries name shall begin with `libthermal_`.
- Configuration files directory on the target device shall be either of `/{odm,vendor}/etc/thermal`.
- Android properties defined in this HAL shall have `vendor.thermal.` prefix.
- Selected by `TARGET_THERMAL_HAL=mainline` in `device/mainline/common/optional/thermal-hal_mainline/`,
  including SELinux policy, see `docs/WIRING_A_HAL.md`.
  Block installing it together with other thermal HALs there.

## References

The paths mentioned in this section are relative to AOSP source tree root.

### AIDL HAL interface definition

In `hardware/interfaces/thermal/aidl/`.
VTS module is in `vts` subdir there.

### HAL implementations

- **Example AIDL Thermal HAL**: `hardware/interfaces/thermal/aidl/default`. Check it out for standard example AIDL HAL implementation.

### Linux kernel

- Default: `kernel/virt/virtio`.
- Thermal core, ABI and netlink UAPI: `Documentation/ABI/testing/sysfs-class-thermal`,
  `drivers/thermal/thermal_sysfs.c`, `drivers/thermal/thermal_netlink.c`
  and `include/uapi/linux/thermal.h` in `kernel/mainline/android-mainline`.
- Zone and cooling device names used by Qualcomm boards: `arch/arm64/boot/dts/qcom/` in
  `kernel/mainline/msm8916-mainline`, `kernel/mainline/msm89x7-mainline`,
  `kernel/mainline/msm8953-mainline` and `kernel/mainline/sm7150-mainline`.
- Cooling device providers: `drivers/thermal/cpufreq_cooling.c`, `devfreq_cooling.c`,
  `cpuidle_cooling.c`, and `drivers/thermal/qcom/`.

Note that the kernel source root paths are actually symlinks.

## Guidelines

- Please firstly understand the AIDL interface, and then everything else.
- The VTS (`VtsHalThermalTargetTest`) is strict about threshold array sizes and
  ordering, callback behavior, and unsupported operations. Read it before designing.

## Design

The HAL shall basically work on as many as possible devices without any additional configuration.
It reads the Linux thermal framework (`/sys/class/thermal`) and reports what the
kernel already knows; it shall never throttle or control anything by itself.

### Single module

Thermal sysfs ABI is the same on every platform: ARM64 SoCs, x86 PCs and QEMU only
differ in zone and cooling device names, not in the interface. So there is no
frontend/backend split and no `dlopen()`. The HAL is one module that scans the
kernel's interfaces directly. Split the source into files by concern
(AIDL service, sysfs scanning, type/severity mapping rules, change detection,
callback registry), not by platform. Introduce a pluggable structure only when a
platform needs a genuinely different data source that the generic scan cannot cover.

Sources to read, all in the one module:
- `/sys/class/thermal/thermal_zone*`: temp, trips, hysteresis, mode.
- `/sys/class/thermal/cooling_device*`: type, cur_state, max_state.
- `/sys/class/power_supply/*/temp`: battery temperature.
- `/sys/class/hwmon`: temperature inputs not registered as thermal zones.

Print basic source info to log on initialization, no matter whether if it succeeds or fails.

### AIDL behavior

- Names reported in `Temperature`/`CoolingDevice` shall be unique and stable across boots.
- `TemperatureThreshold` arrays shall have one entry per `ThrottlingSeverity`,
  `NaN` where undefined, and be non-decreasing.
- Notify registered callbacks on severity change and cooling device state change,
  filtered by type as the interface requires.
- Do not crash when no thermal source is present (e.g. QEMU). Return empty lists.
- Return `EX_UNSUPPORTED_OPERATION` for `forecastSkinTemperature` unless a skin source exists.
- Change detection shall use the kernel's `thermal` generic netlink family
  (`THERMAL_GENL_EVENT_GROUP_NAME`) where available, falling back to polling.
  Zones may appear after boot (modules, hotplug), so the set shall be rescanned.

### Mapping kernel data to AIDL

Keep the rules in tables, separate from the scanning code, so they can be extended easily.

- **Temperature type**: from the zone `type` string (e.g. `cpuN-thermal`, `gpu-thermal`,
  `battery`, `x86_pkg_temp`, `acpitz`), then from the DT node name when readable
  from `/proc/device-tree`, then `UNKNOWN`. A per-zone override property is available.
- **Skin**: only reported when a zone is identified as skin by rule or property.
  Offer an opt-in property to use a chosen zone (or the maximum of chosen zones) as skin.
- **Severity thresholds**: map trip types onto `ThrottlingSeverity`
  (`passive` to LIGHT/MODERATE, `hot` to SEVERE, `critical` to SHUTDOWN, extra passive trips
  in order), `hysteresis` to the cold-side value. Document the mapping in `README.md`.
- **Cooling device type**: from the cooling device `type` string
  (`thermal-cpufreq-*` to CPU, `thermal-devfreq-*` to GPU or COMPONENT by the devfreq name,
  `Fan`/`pwm-fan` to FAN, `Processor`, `intel_powerclamp`, and so on).
- Temperatures shall be in degrees Celsius. Handle millidegree and invalid
  readings (`-ENODATA`, disabled zones) without reporting garbage.

If something can't be derived at runtime, either read it from a configuration file in
`/{odm,vendor}/etc/thermal`, or from `vendor.thermal.*` Android properties.
Devices using this HAL shall set these themselves.

### Directory structure

The root directory of the HAL implementation should be
`hardware/mainline/common/interfaces/thermal/mainline`
relative to AOSP root directory.

Android.bp build rules for each module shall stay in the directory
same as where the module is located at.

Sysfs/CPU helper code that the Power HAL (`../../power/mainline`) will also need
shall be in a shared library under `hardware/mainline/common/libraries/`,
instead of being copied.

### Testing

Keep sysfs access behind a thin interface rooted at a configurable path, so the
mapping rules can be unit tested against a fake sysfs tree. Document testing without
hardware (QEMU, `x86_pkg_temp` on a PC, the `thermal-testing` kernel module)
in `README.md`.

### Drivers and boards to focus compatibility on

Keep the HAL generic while focusing on these first:
- Zones and cooling devices of `arch/arm64/boot/dts/qcom/msm8916*`, `msm89{17,37,40}*`,
  `msm8953*` and `sm7150*` in the reference kernels (`tsens`, `cpufreq_cooling`,
  `devfreq_cooling`, `qcom-spmi-temp-alarm`, `qcom-spmi-adc-tm5`, `lmh`).
  Note that `msm8916` has a single CPU cluster, `msm8937` has two (`cpu4-thermal` and up),
  and GPU cooling is bound in the DT of some of them, so cluster and zone counts differ.
- x86 laptops/tablets: `x86_pkg_temp`, `acpitz`, `intel_powerclamp`, `iwlwifi`, NVMe via hwmon.
- `power_supply` batteries with a `temp` attribute.
- `thermal-generic-adc` and `pwm-fan` style board sensors and fans.
