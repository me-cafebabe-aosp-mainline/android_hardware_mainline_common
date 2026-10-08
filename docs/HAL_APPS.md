# Settings Apps and Control Interfaces of a HAL

Applies when a HAL here gets a user-facing app (a settings page, a
diagnostics screen, a test tool) and an interface for that app to talk to
the HAL. Written for AI agents and human contributors. Existing examples:
`interfaces/audio/mainline/` (`aidl/`, `control/`, `settings/`) and
`interfaces/sensors/mainline/` (`aidl/`, `settings/`).

Several rules below were learned the hard way: breaking them either stops
the device from booting or leaves an app that silently cannot reach its
HAL. Read the whole document before adding such an app.

## Where the app lives

Ship the app inside the HAL's APEX (`apps:` in the `apex` module), so that
it is updated together with the HAL and the interface between them.
Sources go into a `settings/` directory next to the HAL. Names are in
`docs/NAMING_CONVENTIONS.md`.

An APK inside a vendor APEX is a **vendor app**. PackageManager scans it as
part of the partition the APEX lives on (`/vendor/apex`), and that decides
everything below.

## Privileged permissions: none

A vendor app must not request any privileged permission.

- Android never grants a privileged permission to a privileged vendor app
  unless its protection level also has `vendorPrivileged`, which hardly any
  framework permission has (`AppIdPermissionPolicy.kt`,
  `shouldGrantPrivilegedOrOemPermission()`).
- Worse, a requested privileged permission without an allowlist entry
  makes `system_server` throw at `onSystemReady()` when privapp enforcement
  is on: **the device does not boot**. For a vendor app the allowlist is
  read from `/vendor/etc/permissions` only, never from the APEX's own
  `etc/permissions`, so an APEX updated on its own can lose it.

A feature that needs a privileged permission (for example
`MODIFY_AUDIO_ROUTING` for default audio devices) goes into a separate,
small system side app instead: a privileged app on the product partition,
with its allowlist on `/product` (`interfaces/audio/mainline/settings/routing/`).
Such an app runs as `priv_app` and can not use the HAL's vendor service, so
it must do without the control interface.

Normal permissions (e.g. `RECEIVE_BOOT_COMPLETED`) are fine.

## Build and signing

- Build against an SDK: `sdk_version: "module_current"` when the app needs
  `ServiceManager.isDeclared()` / `waitForDeclaredService()` (module-lib
  APIs) to find the HAL, `system_current` otherwise. No `platform_apis`, no
  hidden APIs.
- Do not use the platform certificate. A system image signed with other
  keys (a GSI) would otherwise change what the app may do.
- Every static library must be `apex_available` for the APEX. AndroidX,
  Material and kotlinx-coroutines are (`//apex_available:anyapex`);
  SettingsLib is not (it lists a fixed set of APEXes), so these apps use
  plain AndroidX Preference with a Material theme. Code shared between apps
  goes into an `android_library` that lists both `//apex_available:platform`
  and the APEX.

## SELinux

Without a rule of its own, the app runs as `priv_app` or `platform_app`,
which are `coredomain` and may not use vendor services. Give it a vendor
domain in the device's vendor policy (`device/mainline/common/sepolicy/vendor`):

- `seapp_contexts`: `user=_app isPrivApp=true name=<package> domain=<domain>
  type=app_data_file levelFrom=all`. Match by package name and privilege,
  never by `seinfo`, so the domain does not depend on the system image's
  signing keys. A rule naming the package outranks the platform's generic
  ones.
- Make the app `privileged: true` for that `isPrivApp=true` match alone, so
  that an installed app of the same name can not get the domain. It still
  requests no privileged permission.
- The domain: `type <domain>, domain;` and `app_domain(<domain>)`, plus
  `allow <domain> app_api_service:service_manager find;` for the framework
  APIs it uses, and `find` on the HAL's service type with
  `binder_call(<domain>, <hal domain>)`.

## The control interface

- An AIDL package `vendor.mainline.<domain>` with the interface the app
  needs (`IControl`, `IVirtualSensors`, ...). The framework never uses it.
- It must be **VINTF stable** (`stability: "vintf"`, `@VintfStability` on
  every type), even though both ends ship in one APEX. A binder of an
  unstable interface served by a vendor process has vendor stability, and
  an app process runs the system's libbinder: every call fails with
  "Cannot do a user transaction on a vendor stability binder ... in a
  system stability context" (`frameworks/native/libs/binder/BpBinder.cpp`).
- Versioning (`owner` set, `aidl_api/<name>/` holds the dumps; agents do
  not build, so the maintainer runs the `m` commands, `docs/WORKFLOW.md`):
  - A new interface starts unfrozen (`frozen: false`). The build checks it
    against `aidl_api/<name>/current/`: run `m <name>-update-api` after
    every change of the `.aidl` files.
  - Freeze it (`m <name>-freeze-api`) before it ships. `vendor.mainline.audio`
    and `vendor.mainline.sensors` are frozen at version 1.
  - To change a frozen interface: set `frozen: false`, edit, run
    `m <name>-update-api`. That makes a next version (V2) the latest, so in
    the same change bump the HAL's VINTF fragment to that version, widen
    the compatibility matrix entry to the range (`1-2`), and switch the
    `-V<n>-ndk` / `-V<n>-java` library names. Only add to a frozen version's
    successor (new methods, new fields at the end); never remove or change
    what an earlier version has. Freeze again before it ships.
- Declare the instance in the HAL's VINTF fragment inside the APEX, and add
  an optional entry to a framework compatibility matrix module installed on
  the product partition (`vintf_compatibility_matrix`, `product_specific`),
  so that VINTF checks know the HAL. Label the service in the device's
  vendor `service_contexts` and let the HAL `add_service` it.
- The HAL may change its behavior for the app, but never what it reports to
  the framework through its AIDL HAL interface behind the framework's back:
  anything that changes ports, sensors lists and the like takes effect
  through a restart of the HAL.
- The app finds the service with `ServiceManager.waitForDeclaredService()`
  off the main thread and with a timeout, and works, with less detail,
  while the HAL restarts or is absent.

## State the HAL keeps for the app

- Keep it in `/data/vendor/mainline_<domain>`. Create the directory from the
  APEX's rc file: `on early-boot` /
  `mkdir /data/vendor/mainline_<domain> 0770 <user> <group>`. init parses
  APEX rc files once `/data` is mounted (`perform_apex_config` in
  `post-fs-data`), and `early-boot` comes before `class_start hal`. Do not
  use `on property:apex.<APEX manifest name>.ready=true` for this: apexd
  only sets that property on a rebootless update
  (`LoadApexFromInit()` in `system/apex/apexd/apexd.cpp`), never at boot.
  Label the directory in the device's vendor `file_contexts` with a
  `data_file_type`.
- A missing directory (no SELinux label, no `/data` yet) must only cost
  persistence, never the HAL's function.
- Key state by identities that survive reboots (card ids, paths, sensor
  names), never by indexes, port ids or handles the kernel or the HAL
  assigns at run time.
- Write files atomically (write, `fsync`, rename).

## Changes that can break the device

A setting that can leave the device silent, without sensors or crashing
(card selection, latency, anything applied by restarting the HAL) must undo
itself: apply it as a trial that the HAL reverts on its own unless the user
confirms it in time, and after the HAL failed to start with it a few times.
Offer a property that starts the HAL without any stored setting, for the
case where even confirmed ones break it. `interfaces/audio/mainline/control/SettingsStore.*`
implements this.

## Settings integration

- Inject the app into Settings with the `com.android.settings.action.IA_SETTINGS`
  intent filter and the `com.android.settings.category` meta-data of the
  page it belongs to; `com.android.settings.group_key` places it inside a
  category of that page, `com.android.settings.order` within it. Settings
  injects tiles from system apps only, which APK in an APEX are.
- A dynamic summary comes from a `ContentProvider` named by
  `com.android.settings.summary_uri`. Settings does not catch every
  exception the provider throws, so it must not throw: a failing summary
  can crash Settings.
