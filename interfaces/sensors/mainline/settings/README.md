# Mainline sensors settings

`MainlineSensorsSettings` (`vendor.mainline.sensors.settings`) is the settings
app of the virtual backend of the mainline Sensors HAL
([`../backends/virtual`](../backends/virtual/README.md)). It ships inside the
HAL's APEX together with that backend (soong config variable
`sensors_hal_mainline.include_virtual_backend`) and appears in
**Settings › System › Virtual sensors**.

## Page

- **Device position:** sets the accelerometer and gravity sensors to a
  device held still upright, on its side, upside down or lying flat, and the
  linear acceleration and gyroscope sensors to rest. Shown when there is an
  accelerometer or gravity sensor.
- **One section per virtual sensor:** whether the framework uses it and at
  which rate, a slider per value (a switch for proximity), **Exact values**
  to type them, and **Reset** to return to the defaults.

The HAL keeps what the user sets across reboots; which sensors exist comes
from the `virtual.sensors` setting and only changes after a reboot.

## How it fits together

| Piece | Where |
|-------|-------|
| App | `/apex/com.android.hardware.sensors/priv-app/MainlineSensorsSettings` |
| Settings tile | `com.android.settings.action.IA_SETTINGS`, category `ia.system`, group `interaction_category` |
| Tile summary | `SensorsSummaryProvider`, `content://vendor.mainline.sensors.settings.summary` |
| HAL interface | `vendor.mainline.sensors.IVirtualSensors/default` (`../aidl`) |
| SELinux domain | from the device's vendor `seapp_contexts` |

Living in a vendor APEX makes it a vendor app: it is built against
`module_current` (`ServiceManager.waitForDeclaredService()` for the HAL
interface), with no hidden APIs and no platform certificate, so a system
image signed with other keys does not change what it can do. It is
privileged so that the vendor `seapp_contexts` entry can match it by package
name and privilege; it requests no privileged permission and needs no
allowlist. Without that entry it would run as `priv_app`, which may not use
vendor services.

## Testing

```
adb shell am start -n vendor.mainline.sensors.settings/.SensorsSettingsActivity
adb shell content call --uri content://vendor.mainline.sensors.settings.summary --method getDynamicSummary
adb shell dumpsys sensorservice | grep -i virtual
adb shell ps -AZ | grep vendor.mainline.sensors.settings
```
