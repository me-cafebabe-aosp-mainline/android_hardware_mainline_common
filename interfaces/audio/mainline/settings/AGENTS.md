# Notes for AI agents working on the settings apps

> Part of the mainline audio HAL: read `../AGENTS.md` first, then
> `README.md` here. Commit subjects use the HAL's prefix,
> `mainline/common: intf/audio/mainline: `.

## Layout

`common/` (`MainlineAudioSettingsCommon`, used by both apps):

- `SettingsActivity.kt`: the frame of the activities Settings opens; sub-pages
  are fragments on its back stack (`Preference.fragment`).
- `SettingsFragment.kt`: base of every page (insets, footer).
- `RadioPreference.kt`, layouts, the theme.

`routing/` (`MainlineAudioRouting`, product app):

- `RoutingActivity.kt`, `RoutingFragment.kt`, `RoutingViewModel.kt`: default
  output / input.
- `AudioRouting.kt`: the only code that changes AudioService routing
  preferences, and their storage.
- `AudioDevices.kt`: device identity (`DeviceId`) and names (`DeviceLabels`).
- `RoutingSummaryProvider.kt`: the tile summary.
- `BootReceiver.kt`: restores the routing preferences at boot.

This directory (`MainlineAudioSettings`, APEX app):

- `AudioSettingsActivity.kt`, `AudioSettingsFragment.kt`: the entry page.
- `AudioDevices.kt`: the HAL endpoint behind an `AudioDeviceInfo`.
- `DiagnosticsFragment.kt`, `DiagnosticsViewModel.kt`, `StatusText.kt`:
  Hardware › Sound cards, the report, the HDMI test and high bit rate retry.
- `AndroidSurround.kt`: what Android makes of the passthrough formats
  (public `AudioManager` / `AudioDeviceInfo` APIs only).
- `SpeakersFragment.kt`, `OutputFragment.kt`, `OutputViewModel.kt`:
  Hardware › Speakers; `Speakers` mirrors the HAL's speaker order per layout.
- `MixerFragment.kt`, `MixerViewModel.kt`: Hardware › Mixer.
- `HalSettingsFragment.kt`, `HalSettingsViewModel.kt`,
  `ConfirmSettingsDialogFragment.kt`: Hardware › Advanced, the trial dialog.
- `HalControl.kt`: the only code that talks to `IControl`.

## Invariants

- The APEX app is a vendor app. Use system / module APIs only
  (`sdk_version: "module_current"`), never hidden APIs, and never require the
  platform certificate: both would tie the vendor partition to one system
  image.
- The APEX app must never request a privileged permission. Android grants a
  vendor app only `vendorPrivileged` ones (few exist), and an unlisted
  privileged permission makes `system_server` refuse to boot. A feature that
  needs one belongs in the routing app (or another system side app), whose
  allowlist is `routing/privapp-permissions-vendor.mainline.audio.routing.xml`.
- Only the APEX app talks to the HAL: the routing app runs as `priv_app`,
  which may not use vendor services. A new HAL call may need the device's
  SELinux policy (`device/mainline/common/sepolicy/vendor`) to follow.
- `common/` stays free of AIDL and of anything one app alone needs, and on
  `sdk_version: "system_current"`, so that both apps can use it.
- Every Binder call to the HAL goes through `HalControl` and runs off the main
  thread. The HAL can be restarting or absent: everything must work, with
  less detail, when `HalControl` returns null.
- HAL settings only change through `applySettings()` (a trial the HAL reverts
  on its own) or `resetSettings()`. The confirmation dialog is not
  cancelable, and the app never keeps a trial without the user's Keep.
- Match devices by `DeviceId` (role, type, address), never by
  `AudioDeviceInfo.getId()`, which changes when a device reconnects.
- The routing tile's `com.android.settings.summary_uri` meta-data and
  `RoutingSummaryProvider.SUMMARY_URI` must name the same URI.
- Keep `README.md` in sync with behavior, permission or packaging changes.

## Framework interaction (AOSP source)

- `frameworks/base/media/java/android/media/AudioManager.java`: routing
  preference APIs (`setPreferredDeviceForStrategy()`,
  `setPreferredDeviceForCapturePreset()`).
- `frameworks/base/services/core/java/com/android/server/audio/`:
  `AudioDeviceBroker`, `AudioDeviceInventory` (how preferences are kept).
- `frameworks/base/packages/SettingsLib/Tile/`: the Settings tile protocol.
- `AudioManager.getDirectPlaybackSupport()`, `getEncodedSurroundMode()` and
  `AudioDeviceInfo.getEncodings()`: the public view of passthrough. Do not
  use `getReportedSurroundFormats()` / `getSurroundFormats()`: they are
  `@TestApi`, hidden from a vendor app.
- `IControl` errors reach Java as runtime exceptions (`EX_ILLEGAL_STATE` as
  `IllegalStateException`, `EX_ILLEGAL_ARGUMENT` as
  `IllegalArgumentException`, a service specific error as
  `ServiceSpecificException`, `Parcel.java`). `HalControl.call()` turns them
  into null like a dead HAL, so an uncaught one can not crash the app. A
  call that has to tell them apart catches them inside its block
  (`HalControl.probeHdmi()` maps `EX_ILLEGAL_STATE` to `Busy`). A page that
  changes something reports a null result through its ViewModel's
  `failures` (a toast), and shows the HAL's state again rather than the
  value it asked for.
- `frameworks/base/services/permission/java/com/android/server/permission/access/permission/AppIdPermissionPolicy.kt`:
  which allowlist applies to an APK in an APEX, and why a vendor app gets no
  privileged permission without `vendorPrivileged`
  (`shouldGrantPrivilegedOrOemPermission()`).

## Workflow

- Do not build, install or run anything; the maintainer does.
- Format touched Kotlin with the prebuilt ktfmt:
  `java -jar prebuilts/build-tools/common/framework/ktfmt.jar --kotlinlang-style <files>`
