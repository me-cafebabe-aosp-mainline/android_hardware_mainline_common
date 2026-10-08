# Notes for AI agents working on the settings app

> Part of the mainline audio HAL: read `../AGENTS.md` first, then
> `README.md` here. Commit subjects use the HAL's prefix,
> `mainline/common: intf/audio/mainline: `.

## Layout

- `AudioSettingsActivity.kt`: the activity Settings opens; sub-pages are
  fragments on its back stack (`Preference.fragment`).
- `SettingsFragment.kt`: base of every page (insets, footer).
- `AudioSettingsFragment.kt`, `RoutingViewModel.kt`: default output / input.
- `AudioRouting.kt`: the only code that changes AudioService routing
  preferences, and their storage.
- `AudioDevices.kt`: device identity (`DeviceId`) and names (`DeviceLabels`).
- `DiagnosticsFragment.kt`, `DiagnosticsViewModel.kt`, `StatusText.kt`:
  Hardware › Sound cards, the report.
- `MixerFragment.kt`, `MixerViewModel.kt`: Hardware › Mixer.
- `HalControl.kt`: the only code that talks to `IControl`.
- `AudioSummaryProvider.kt`: the Settings tile summary.
- `BootReceiver.kt`: restores the routing preferences at boot.

## Invariants

- The app is a vendor app in the HAL's APEX. Use system / module APIs only
  (`sdk_version: "module_current"`), never hidden APIs, and never require the
  platform certificate: both would tie the vendor partition to one system
  image.
- A new privileged permission needs an entry in
  `privapp-permissions-vendor.mainline.audio.settings.xml`; a new HAL call
  may need the device's SELinux policy (`device/mainline/common/sepolicy/vendor`)
  to follow.
- Every Binder call to the HAL goes through `HalControl` and runs off the main
  thread. The HAL can be restarting or absent: everything must work, with
  less detail, when `HalControl` returns null.
- Match devices by `DeviceId` (role, type, address), never by
  `AudioDeviceInfo.getId()`, which changes when a device reconnects.
- The tile's `com.android.settings.summary_uri` meta-data and
  `AudioSummaryProvider.SUMMARY_URI` must name the same URI.
- Keep `README.md` in sync with behavior, permission or packaging changes.

## Framework interaction (AOSP source)

- `frameworks/base/media/java/android/media/AudioManager.java`: routing
  preference APIs (`setPreferredDeviceForStrategy()`,
  `setPreferredDeviceForCapturePreset()`).
- `frameworks/base/services/core/java/com/android/server/audio/`:
  `AudioDeviceBroker`, `AudioDeviceInventory` (how preferences are kept).
- `frameworks/base/packages/SettingsLib/Tile/`: the Settings tile protocol.
- `frameworks/base/services/permission/java/com/android/server/permission/access/permission/AppIdPermissionPolicy.kt`:
  which allowlist applies to an APK in an APEX.

## Workflow

- Do not build, install or run anything; the maintainer does.
- Format touched Kotlin with the prebuilt ktfmt:
  `java -jar prebuilts/build-tools/common/framework/ktfmt.jar --kotlinlang-style <files>`
