# Mainline audio settings

`MainlineAudioSettings` (`vendor.mainline.audio.settings`) is the settings
app of the mainline audio HAL. It ships inside the HAL's APEX and appears in
**Settings › Sound › Audio › Audio devices**.

## Pages

- **Default output / default input:** pick the device all audio goes to, or
  *Automatic* to leave the choice to Android. The choice is the preferred
  device of every product strategy (output) and of every user facing capture
  preset (input) of AudioService, so it holds for all audio while the device
  is connected; when it is not, Android falls back to its own choice and
  returns to the device once it is back. A device that is not connected stays
  listed while it is chosen.

- **Hardware › Sound cards:** what the HAL found and uses: per card the
  driver, the UCM verb and its enabled devices; per endpoint its role, PCM
  and UCM device, capabilities, jack state, the streams using it and, for
  HDMI heads, the sink's passthrough formats. **Copy report** / **Share
  report** hand out the HAL's dumpsys text plus `/proc/asound` for bug
  reports. Shown only on devices that declare `IControl`.

- **Hardware › Mixer:** every writable mixer control of the HAL's cards
  (switches, levels, enumerations such as HDA's *Auto-Mute Mode* or
  *Independent HP*). A value the user sets is stored by the HAL and applied
  again at every start and whenever UCM switches devices on the card, since
  UCM sequences set the controls they know. *Forget my changes* drops the
  stored values of a card.

- **Hardware › Advanced:** the HAL's `vendor.audio.mainline.*` settings,
  global and per card. Edits collect until **Apply**, which restarts the HAL
  with them on trial; a dialog then asks to keep them, and the HAL reverts on
  its own after 30 seconds or when it fails to start with them. **Reset to
  device defaults** drops everything set here.

Devices of the mainline HAL are named after their device port (e.g.
"PCH: HDMI 1"), from `vendor.mainline.audio.IControl`; others by their
product name or type.

## How it fits together

| Piece | Where |
|-------|-------|
| App | `/apex/com.android.hardware.audio/priv-app/MainlineAudioSettings` |
| Privileged permission allowlist | `/vendor/etc/permissions/privapp-permissions-vendor.mainline.audio.settings.xml` |
| Settings tile | `com.android.settings.action.IA_SETTINGS`, category `ia.sound`, group `audio_category` |
| Tile summary | `AudioSummaryProvider`, `content://vendor.mainline.audio.settings.summary` |
| HAL interface | `vendor.mainline.audio.IControl/default` (`../aidl`) |
| SELinux domain | from the device's vendor `seapp_contexts` |

Living in a vendor APEX makes it a vendor app:

- It is built against `module_current`: system APIs for the routing
  preferences and `ServiceManager.waitForDeclaredService()` for IControl. No
  hidden APIs and no platform certificate, so a system image signed with
  other keys (a GSI) does not change what it can do.
- PackageManager takes the allowlist of a vendor app from the vendor
  partition, also for an APK in an APEX, so the allowlist is a separate
  vendor file the APEX requires. A missing entry would keep
  `MODIFY_AUDIO_ROUTING` from being granted.
- SELinux puts an app into `priv_app` unless a `seapp_contexts` entry says
  otherwise; the device's vendor policy gives it a domain that may use
  `IControl`.

AudioService forgets routing preferences on reboot, so the app stores the
choice in device-protected storage and applies it again on
`LOCKED_BOOT_COMPLETED`.

## Limitations

- Only the system user's choice is restored at boot; a choice made in
  another user lasts until the next reboot.
- *Automatic* removes every preferred device, also one another app set.

## Testing

```
adb shell am start -n vendor.mainline.audio.settings/.AudioSettingsActivity
adb shell content call --uri content://vendor.mainline.audio.settings.summary --method getDynamicSummary
adb shell dumpsys audio | grep -i -A3 'preferred'
adb shell ps -AZ | grep vendor.mainline.audio.settings
```
