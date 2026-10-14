# APEX Rebootless Update

Applies to every HAL (or other service) shipped as an APEX in this
repository. Written for both AI agents and human contributors.

A new implementation that ships as an APEX **must** support rebootless
update. `adb install` can then replace the APEX and restart its service
without rebooting the device, which makes testing a change much faster.

## Requirements

1. Set `supportsRebootlessUpdate` in the APEX manifest:

   ```json
   {
       "name": "com.android.hardware.vibrator",
       "supportsRebootlessUpdate": true,
       "version": 1
   }
   ```

2. Start every service of the APEX from its init rc once the APEX is
   active. After a rebootless update, init unloads the old APEX, loads
   the new one and sets `apex.<APEX name>.ready=true`. The old service is
   killed, and nothing else starts the new one, because the service's
   class is not started again.

   ```
   on property:apex.com.android.hardware.vibrator.ready=true
       start vendor.vibrator-mainline
   ```

   - `<APEX name>` is the manifest `name`, not the module name (see
     `docs/NAMING_CONVENTIONS.md`).
   - Add one `start` line per service of the APEX, including the ones
     that serve a different HAL (e.g. the audio effect HAL).
   - apexd sets the property only on a rebootless update, never at boot.
     Anything the service needs at boot must still be done by a normal
     trigger such as `on early-boot`.

3. Keep the service restartable. It gets `SIGTERM`/`SIGKILL` while
   clients are connected, so it must not leave state behind that blocks
   the next instance, and its clients must cope with a dying HAL.

## Testing

Plain `adb install` of an APEX is always a staged install that needs a
reboot. Ask for a non-staged one:

```
adb install --non-staged <APEX>.apex
```

Check that:

- `/apex/apex-info-list.xml` lists the new file as active
  (`isFactory="false"`, under `/data/apex/active/`).
- logcat shows init processing `apex.<APEX name>.ready=true`, and the
  service running with a new pid.

Without the manifest flag the update is refused. `--force-non-staged`
forces it anyway; use it only to debug, never to ship.

## Device requirements

apexd pins updated APEXes on the block device behind `/data`, which a
`tmpfs` does not have. A device that boots with `/data` on `tmpfs`, like
the live ISO, must disable pinning, or the install fails with
`Failed to create APEX storage`:

```
apexd.config.use_fiemap=false
```

`device/mainline/generic` sets it in `configs/properties/vendor.prop`.
apexd reads it once at startup, so it has to be set before apexd starts.
Do not restart apexd to apply it: apexd is a critical service, and
killing it reboots the device.
