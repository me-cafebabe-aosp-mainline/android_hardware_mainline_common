# Details about making the initial implementation of Android Power HAL for mainline kernel

We want a flexible and generic Android Power HAL,
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
  - Init rc service name shall be `vendor.power-mainline`.
  - The APEX module name shall be `com.android.hardware.power.mainline`.
  - Filename for the executable shall be `android.hardware.power-service.mainline`.
  - `LOG_TAG` define on top of our source files shall begin with `MainlinePower`.
  - Init rc and vintf fragment shall be renamed accordingly.
- Libraries name shall begin with `libpower_`.
- Configuration files directory on the target device shall be either of `/{odm,vendor}/etc/power`.
- Android properties defined in this HAL shall have `vendor.power.` prefix.
- Selected by `TARGET_POWER_HAL=mainline` in `device/mainline/common/optional/power-hal_mainline/`,
  including SELinux policy, see `docs/WIRING_A_HAL.md`.
  Block installing it together with other power HALs there.

## References

The paths mentioned in this section are relative to AOSP source tree root.

### AIDL HAL interface definition

In `hardware/interfaces/power/aidl/`.
VTS module is in `vts` subdir there.

### HAL implementations

- **Example AIDL Power HAL**: `hardware/interfaces/power/aidl/default`. Check it out for standard example AIDL HAL implementation.

### Linux kernel

- Default: `kernel/virt/virtio`.
- cpufreq, EPP, cpu_capacity, cpuidle: `Documentation/ABI/testing/sysfs-devices-system-cpu`
  and `Documentation/admin-guide/pm/` in `kernel/mainline/android-mainline`.
- devfreq: `Documentation/ABI/testing/sysfs-class-devfreq`.
- ACPI platform profile: `Documentation/ABI/testing/sysfs-class-platform-profile`.
- Utilization clamping: `Documentation/scheduler/sched-util-clamp.rst`,
  `kernel/sched/syscalls.c` (`sched_setattr` with `SCHED_FLAG_UTIL_CLAMP_*`).
- For Qualcomm boards (cpufreq, devfreq, big.LITTLE capacities): `kernel/mainline/msm8916-mainline`,
  `kernel/mainline/msm89x7-mainline`, `kernel/mainline/msm8953-mainline` and
  `kernel/mainline/sm7150-mainline`.

Note that the kernel source root paths are actually symlinks.

## Guidelines

- Please firstly understand the AIDL interface, and then everything else.
- The VTS (`VtsHalPowerTargetTest`) checks `getSupportInfo()` against `isModeSupported`/`isBoostSupported`,
  hint session behavior and the session channel. Read it before designing.
- Never report a mode, boost, hint or session feature as supported unless it will actually
  change system behavior on this device (same rule as `../../vibrator/mainline` for effects).

## Design

The HAL shall basically work on as many as possible devices without any additional configuration,
by acting only through generic kernel interfaces that are probed at runtime.
Everything the probe cannot find shall be reported as unsupported, never faked.

### Single module

The kernel interfaces used here (cpufreq, devfreq, `platform_profile`, utilization
clamping) are the same ABI on ARM64 SoCs, x86 PCs and QEMU. A device that lacks one
simply fails the probe. So there is no frontend/backend split and no `dlopen()`.
The HAL is one module. Split the source into files by concern, not by platform:
- AIDL service (`IPower`, `getSupportInfo()`, mode/boost state and timers)
- probing and restoring of the kernel knobs (cpufreq/EPP, devfreq, `platform_profile`)
- hint sessions (uclamp controller)
- headroom calculation
- policy tables (mode/boost to knob values)

Introduce a pluggable structure only when a platform needs a genuinely different
mechanism that these generic interfaces cannot cover.

`getSupportInfo()`, `isModeSupported()` and `isBoostSupported()` shall all be answered from the one
probe result. Overlapping modes (e.g. `LOW_POWER` plus `SUSTAINED_PERFORMANCE`) shall resolve
predictably. A knob shall record its original value before the first write, and all knobs shall be
restored when a mode or boost ends and on exit. Write nothing that can't be restored.

Print what was probed and what is supported to log on initialization,
no matter whether if it succeeds or fails.

### Knobs

- **cpufreq**: per-policy `scaling_min_freq`/`scaling_max_freq`, `energy_performance_preference`
  (amd-pstate, intel_pstate), cluster discovery from `related_cpus` and `cpu_capacity`.
  Never change the governor. Thermal cooling uses frequency QoS limits, so these compose.
- **devfreq**: `min_freq`/`max_freq` of GPU/bus devices under `/sys/class/devfreq`,
  classified by name or `related_cpus`.
- **platform_profile**: `/sys/firmware/acpi/platform_profile` (`low-power`, `balanced`, `performance`).
- **uclamp**: `sched_setattr` utilization clamps for hint sessions, and `cpu.uclamp.min` of
  the Android cpu cgroups for launch/interaction boosts. Probe `CONFIG_UCLAMP_TASK` at runtime.

### Mapping AIDL to knobs

Keep the rules in tables, separate from the probing code, so they can be extended easily.
Frequency targets are policy, not hardware facts. Express them as fractions of the
discovered range, with defaults that work and `vendor.power.*` overrides.

- `LOW_POWER`: cap max frequency at a fraction, prefer EPP `power`, platform profile `low-power`.
- `SUSTAINED_PERFORMANCE`: avoid the top operating points that thermal throttling cannot sustain
  (cap at a fraction, default high), EPP `balance_performance`.
- `FIXED_PERFORMANCE`: raise minimums to a high fraction; supported only if a knob can do it.
- `LAUNCH`, `INTERACTION`, `DISPLAY_UPDATE_IMMINENT`, `EXPENSIVE_RENDERING`: timed minimum
  raise on the performance cluster(s), uclamp.min boost on `top-app`, devfreq minimum bump.
- `DEVICE_IDLE`, `DISPLAY_INACTIVE`, `INTERACTIVE`: only if it measurably changes something
  on this device; otherwise report unsupported.
- `DOUBLE_TAP_TO_WAKE`: no generic kernel interface exists. Unsupported unless a
  property names the input device and the knob. Do not toggle `power/wakeup` alone, as that wakes on any touch.
- Camera, game, audio, VR and automotive modes: unsupported at first.

### Hint sessions (ADPF)

This is the most valuable part for devices that have only schedutil and no vendor boost framework.

- Implement `createHintSession`, `createHintSessionWithConfig` and `IPowerHintSession`
  with a controller that sets per-thread `uclamp.min` from the target and actual work durations,
  with rate limiting, a stale-session timeout, and cleanup when threads exit.
- `getHintSessionPreferredRate` derives from the display refresh rate or a property default.
- Report `usesSessions` false when `sched_setattr` uclamp is unavailable.
- Implement the session channel (FMQ) only if the VTS requires it for the reported support level.
- `getCpuHeadroom`/`getGpuHeadroom`: derive from per-cluster utilization and frequency,
  honoring the calculation windows in `SupportInfo`. Report unsupported if no reliable source exists.
- Composition data and updates are accepted and may be ignored at first. Do not advertise them as used.

### Permissions

Sysfs knobs on mainline are root-owned. Decide with the user whether the service runs
as root with a minimal capability set, or whether init rc `chown`/`chmod` lines
give a restricted user write access. Document the decision in `AGENTS.md`.
Add the sepolicy rules for the files above in `device/mainline/common/sepolicy/vendor/`.

### Directory structure

The root directory of the HAL implementation should be
`hardware/mainline/common/interfaces/power/mainline`
relative to AOSP root directory.

Android.bp build rules for each module shall stay in the directory
same as where the module is located at.

CPU topology and sysfs helper code shared with the Thermal HAL
(`../../thermal/mainline`) shall be in a shared library under
`hardware/mainline/common/libraries/`.

### Testing

Keep sysfs and `sched_setattr` access behind a thin interface rooted at a configurable path,
so the probe and mapping rules can be unit tested against a fake sysfs tree and the session
controller against a fake clock. Document testing without hardware (QEMU, where most knobs
are absent and must report unsupported) in `README.md`.

### Drivers and boards to focus compatibility on

Keep the HAL generic while focusing on these first:
- cpufreq on `msm8916`, `msm89{17,37,40}`, `msm8953` and `sm7150` boards, including the
  big.LITTLE clusters of the latter. The `msm8916` has one cluster and `msm8937` has two,
  so the cluster discovery must not assume a cluster count.
- devfreq GPU nodes (freedreno, panfrost) and bus devfreq on the same boards.
- x86 laptops/tablets: `intel_pstate`, `amd-pstate`, `acpi-cpufreq`, ACPI `platform_profile`.
- Kernels with and without `CONFIG_UCLAMP_TASK` and `CONFIG_PM_DEVFREQ`. The `msm8916-mainline`
  and `msm89x7-mainline` defconfigs do not enable `UCLAMP_TASK` (and only enable
  `PM_DEVFREQ_EVENT`), so on these the HAL starts with only the cpufreq knobs,
  reports hint sessions as unsupported, and needs no configuration to do so.
