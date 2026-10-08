# Notes for AI agents working on the settings app

> Part of the mainline Sensors HAL: read `../AGENTS.md` first, then
> `README.md` here and `../backends/virtual/README.md`. Commit subjects use
> the HAL's prefix, `mainline/common: intf/sensors/mainline: `.

## Layout

- `SensorsSettingsActivity.kt`: the activity Settings opens.
- `SettingsFragment.kt`: base of every page (insets, footer).
- `VirtualSensorsFragment.kt`, `VirtualSensorsViewModel.kt`: the page.
- `SensorKinds.kt`: titles and units of a sensor type's values.
- `DevicePosition.kt`: what the motion sensors report in a position.
- `HalControl.kt`: the only code that talks to `IVirtualSensors`.
- `SensorsSummaryProvider.kt`: the Settings tile summary.

## Invariants

- The app is a vendor app in the HAL's APEX. Use system / module APIs only
  (`sdk_version: "module_current"`), never hidden APIs, and never require the
  platform certificate: both would tie the vendor partition to one system
  image.
- A privileged permission would need a vendor allowlist file that the APEX
  requires (see the audio settings app); a new HAL call may need the device's
  SELinux policy (`device/mainline/common/sepolicy/vendor`) to follow.
- Every Binder call to the HAL goes through `HalControl` and runs off the main
  thread. The HAL can be restarting or the backend absent: the page must work,
  showing why, when `HalControl` returns null.
- Sensor handles are the backend's own and stable while the HAL runs; the
  page re-renders when the list changes. Listeners read the latest values
  (`current`), never the ones captured when the preference was made.
- `VirtualSensor.type` is the HAL's `SensorType`, equal to the framework's
  `Sensor.TYPE_*`.
- The tile's `com.android.settings.summary_uri` meta-data and
  `SensorsSummaryProvider.METHOD_GET_DYNAMIC_SUMMARY` must agree.
- Keep `README.md` in sync with behavior, permission or packaging changes.

## Framework interaction (AOSP source)

- `frameworks/base/core/java/android/hardware/Sensor.java`: type constants
  and units.
- `frameworks/base/packages/SettingsLib/Tile/`: the Settings tile protocol.
- `packages/apps/Settings/res/xml/system_dashboard_fragment.xml`: the group
  keys of Settings › System.

## Workflow

- Do not build, install or run anything; the maintainer does.
- Format touched Kotlin with the prebuilt ktfmt:
  `java -jar prebuilts/build-tools/common/framework/ktfmt.jar --kotlinlang-style <files>`
