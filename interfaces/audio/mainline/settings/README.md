# Mainline audio settings

Two apps, both in **Settings › Sound › Audio**, sharing
`MainlineAudioSettingsCommon` (`common/`: activity frame, page base class,
radio preference):

| App | Package | Lives in | Tile |
|-----|---------|----------|------|
| `MainlineAudioSettings` (this directory) | `vendor.mainline.audio.settings` | the HAL's APEX | Audio hardware |
| `MainlineAudioRouting` (`routing/`) | `vendor.mainline.audio.routing` | `/product/priv-app` | Default audio devices |

They are separate because Android never grants a vendor app (and an APK in
a vendor APEX is one) a privileged permission whose protection level lacks
`vendorPrivileged`. `MODIFY_AUDIO_ROUTING`, which the routing preferences
need, is `signature|privileged|role`. Requesting it from the APEX app would
not only fail: without an allowlist entry, `system_server` refuses to boot.
So the APEX app requests no privileged permission at all, and the routing
page is a small product app with its own allowlist. Each works without the
other.

On Android TV, TvSettings has no tiles: both apps are reached from
**Display & sound › Audio devices and hardware** instead (see
[Android TV](#android-tv)).

## Default audio devices (`routing/`)

Pick the device all audio goes to, or *Automatic* to leave the choice to
Android. The choice is the preferred device of every product strategy
(output) and of every user facing capture preset (input) of AudioService, so
it holds for all audio while the device is connected; when it is not,
Android falls back to its own choice and returns to the device once it is
back. A device that is not connected stays listed while it is chosen.
Devices are named by their product name or type; bus ports of the mainline
HAL show their address ("<card id>/<path>").

AudioService forgets routing preferences on reboot, so the app stores the
choice in device-protected storage and applies it again on
`LOCKED_BOOT_COMPLETED`.

## Audio hardware (the APEX app)

- **Sound cards:** what the HAL found and uses: per card the
  driver, the UCM verb and its enabled devices; per endpoint its role, PCM
  and UCM device, capabilities, jack state, the streams using it (and the
  ones a direct stream silences), the period the last low latency stream
  got and, for HDMI heads, the sink's passthrough formats, what decided high
  bit rate and how the last passthrough stream went. **Test this HDMI
  output** opens an idle HDMI output with each passthrough configuration
  without playing anything and lists what it accepts; **Retry high bit
  rate** forgets a refusal. **Low latency output** says whether
  `fast_latency_ms` got the primary output a FastMixer. **HDMI passthrough in
  Android** shows what Android itself makes of the formats (surround sound
  setting, formats it knows for the HDMI device, formats it offers apps for
  the current output), from public `AudioManager` APIs. **Copy report** /
  **Share report** hand out the HAL's dumpsys text plus `/proc/asound`
  (cards, PCMs, HDMI ELDs, HDA codecs) for bug reports.

- **Speakers:** per output the speaker layout (stereo, quad, 5.1,
  7.1 or automatic), speaker fill, swap left / right, mono, subwoofer and
  small speakers with a crossover, and per speaker a level, a delay and a
  test (pink noise for two seconds; the app plays silence to the output
  meanwhile, since the HAL only replaces what streams play). Changes apply
  right away and are stored by the HAL.
- **Mixer:** every writable mixer control of the HAL's cards
  (switches, levels, enumerations such as HDA's *Auto-Mute Mode* or
  *Independent HP*). A value the user sets is stored by the HAL and applied
  again at every start and whenever UCM switches devices on the card, since
  UCM sequences set the controls they know. *Forget my changes* drops the
  stored values of a card.

- **Advanced:** the HAL's `vendor.audio.mainline.*` settings,
  global and per card. Edits collect until **Apply**, which restarts the HAL
  with them on trial; a dialog then asks to keep them, and the HAL reverts on
  its own after 30 seconds or when it fails to start with them. **Reset to
  device defaults** drops everything set here. Passthrough formats are
  picked from the ones this HAL build can pack.

## Android TV

TvSettings' Display & sound screen has a hidden "default audio output
settings" entry, a slice whose URI and title are overlayable strings.
`MainlineAudioTvSettingsOverlay` (`tv/`, vendor) points it at
`TvSliceProvider` of the hardware app, which lists Default audio devices
(when the routing app is installed), Sound cards, Speakers, Mixer and
Advanced; each opens that page on its own, and Back returns to TvSettings.
TvSettings shows the entry only while the provider answers, so the overlay
does nothing without the app, and a TvSettings without the slot ignores it.

On a TV (`UI_MODE_TYPE_TELEVISION`) the pages use a dark theme and have no
Up button, which would take the first focus from the list; the remote's
Back key goes back. Sliders move with left / right, levels by 1 dB and
delays by 0.5 ms a press.

## How it fits together

| Piece | Where |
|-------|-------|
| Hardware app | `/apex/com.android.hardware.audio/priv-app/MainlineAudioSettings` |
| Routing app | `/product/priv-app/MainlineAudioRouting` |
| Routing app's allowlist | `/product/etc/permissions/privapp-permissions-vendor.mainline.audio.routing.xml` |
| Settings tiles | `com.android.settings.action.IA_SETTINGS`, category `ia.sound`, group `audio_category` |
| TvSettings entry | `content://vendor.mainline.audio.settings.slices/hardware`, from `/vendor/overlay/MainlineAudioTvSettingsOverlay` |
| Routing tile summary | `RoutingSummaryProvider`, `content://vendor.mainline.audio.routing.summary` |
| HAL interface | `vendor.mainline.audio.IControl/default` (`../aidl`), hardware app only |
| Hardware app's SELinux domain | from the device's vendor `seapp_contexts` |

Living in a vendor APEX makes the hardware app a vendor app: it is built
against `module_current` (`ServiceManager.waitForDeclaredService()` for
IControl), with no hidden APIs, no platform certificate and no privileged
permission, so a system image signed with other keys (a GSI) changes
nothing for it, and updating the APEX alone can not break boot. It is
privileged only so that the vendor `seapp_contexts` entry, which gives it a
domain that may use `IControl`, matches the preinstalled app alone.

The routing app is an ordinary system side app: product partition, system
SDK, `priv_app` domain, allowlist on `/product`.

## Limitations

- Only the system user's choice is restored at boot; a choice made in
  another user lasts until the next reboot.
- *Automatic* removes every preferred device, also one another app set.

## Testing

```
adb shell am start -n vendor.mainline.audio.settings/.AudioSettingsActivity
adb shell am start -n vendor.mainline.audio.routing/.RoutingActivity
adb shell am start -n vendor.mainline.audio.settings/.AudioSettingsActivity --es vendor.mainline.audio.settings.extra.PAGE speakers
adb shell cmd overlay list com.android.tv.settings
adb shell content call --uri content://vendor.mainline.audio.routing.summary --method getDynamicSummary
adb shell dumpsys audio | grep -i -A3 'preferred'
adb shell ps -AZ | grep vendor.mainline.audio
```
