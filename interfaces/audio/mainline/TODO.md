# Mainline audio HAL: status and TODO

What was verified where, known issues, and what is left. Keep it current:
update it in the commit that changes the state, and remove entries once they
are done.

So far this covers the PCM arbiter, the FAST primary output and HDMI
passthrough (the `PLAN_HDMI_PASSTHROUGH_AND_FAST` series). The other parts of
the HAL (settings app, control interface, output processing) are not tracked
here yet.

## Status

| Area | Verified on |
|------|-------------|
| HDMI 5.1 linear PCM next to the primary output (arbiter, multichannel output) | NVIDIA Tegra (`tegra-hda`), Android TV, Samsung TV |
| ELD parsing, passthrough profiles (FFmpeg build) | same setup: the HAL logs all 8 expected profiles |
| Passthrough formats reaching the policy (dynamic `hdmi passthrough` port, probe answer) | same setup: `dumpsys media.audio_policy` shows all 8 formats on the HDMI device and the mix port |
| TV Settings → Surround sound showing the formats | same setup, once HDMI is full volume (after toggling CEC Volume Control, see known issue 2) |
| Bitstreaming AC-3, E-AC-3 (incl. JOC), DTS | not verified |
| Bitstreaming DTS-HD, DTS-HD MA, TrueHD, 7.1 IEC 61937 (high bit rate) | **fails** on the same setup with a mainline kernel, see known issue 1 |
| HDA pin capabilities in the HBR diagnostics (`HdaPinDetail()`) | not built |
| UI sounds during a direct / passthrough stream (arbiter pre-emption) | not verified |
| FAST primary output (`fast_latency_ms`): FastMixer, fast tracks | not verified |
| VTS (`VtsHalAudioCoreModuleTargetTest`), in particular the dynamic `hdmi passthrough` port and the probe answer in `setAudioPortConfig()` | not run |
| FFmpeg packer: flush / seek (the packer is rebuilt), positions for A/V sync | not verified |
| `IControl` version 2 (HDMI / FAST diagnostics, `probeHdmi()`, `resetHbr()`) and the settings app pages using it | not built (frozen) |
| `mixer.toggle` (switches off and on in the mixer init, for QEMU cards whose host audio stream does not follow the guest's unchanged mixer values) | not verified |
| TvSettings entry (Display & sound slot, `TvSliceProvider`, `MainlineAudioTvSettingsOverlay`) and the pages with a remote | not built |
| AC97 front panel (`card.<selector>.ac97_front_panel`) without UCM: pin override, codec reconfiguration at start and restore, device policy | ECS G41T-CM3 (ALC662): front headset plays along with the rear jack |
| `card.<selector>.ucm_hda`: HDA cards with alsa-ucm-conf's HDA profile (separate speaker / headphones ports, jack switching) | VIA VT1708S (B75 desktop): line out plays and mics are listed, with the alsa-ucm-conf HDA fixes. Conexant CX20590 (Lenovo G480): with the jack mic headset template, Android connects the plugged headset and plays on Headphones with the speaker off, and back on Speaker unplugged. Output volumes at unity on both (Realtek ALC887 B85, CX20590). Not verified: capture by ear, HDMI / passthrough with UCM, the AC97 front panel with UCM on the G41 |
| AC97 front panel with UCM: front headphones as a bus output | Realtek ALC887 (B85): "PCH: Headphones", picked in Default audio devices, plays |

## Known issues

1. High bit rate on Tegra 210 (also 210B01, same HDA block) with a mainline
   kernel. The HDMI pin (NID 0x05) does not report `HBR` in its `Pincap`
   line, and mainline's `hdmi_pin_hbr_setup()` refuses high bit rate at
   prepare time on such a pin. The hardware can do it: the tester says
   TrueHD and DTS-HD MA work with NVIDIA's downstream kernel. So this looks
   like a mainline kernel issue (the pin capability, or Tegra specific
   handling downstream has and mainline lacks), to be found by comparing
   downstream's HDA HDMI / Tegra code with mainline's
   `sound/pci/hda/patch_hdmi.c` (`tegra_hdmi_*`, `hdmi_pin_hbr_setup()`).
   The HAL keeps trying high bit rate on HDA and remembers a refusal per
   connection; the pin capability only shows in the diagnostics. All
   reports so far come from the first passthrough build (`dc33dcb`), which
   opened with its own period / buffer sizes and took any `-EINVAL` for a
   high bit rate refusal. Its 8 channel / 192 kHz open failed at
   `snd_pcm_hw_params` (`-EINVAL`), before the pin check at prepare, so
   that may have been the sizes, which the sink now retries with the
   driver's own; a build with `probeHdmi()` ("Test this HDMI output")
   shows the failing step and errno. Data asked: the probe results,
   `uname -a`, which downstream kernel works (version / tree), the full
   `Node 0x05` block of `codec#*`.
2. TV Settings → Surround sound was disabled ("To enable surround sound,
   go to Settings > Remotes & Accessories > ... Volume control ..."):
   `AdvancedVolumeFragment` disables it while HDMI's volume behavior is
   `DEVICE_VOLUME_BEHAVIOR_VARIABLE`. Turning CEC Volume Control off and on
   made it work, so the formats are fine (the tester's
   `dumpsys media.audio_policy` lists all 8 on the HDMI device and the
   `hdmi passthrough` port) and the HDMI volume behavior was wrong after
   boot. Likely cause, from the framework source, not confirmed: on a
   playback device AudioService (`onSystemReady()`) registers two
   HdmiControlManager listeners. The volume control one answers at once
   and makes HDMI full volume; the status one answers only after
   `HdmiControlService.queryDisplayStatus()` got the TV's power status, and
   reports CEC unavailable when the TV did not answer
   (`POWER_STATUS_UNKNOWN`), which makes HDMI variable
   (`updateHdmiCecSinkLocked()`). Toggling CEC Volume Control only reruns
   the first, and an HDMI reconnect keeps the current state
   (`onCheckVolumeCecOnHdmiConnection()`). Not a HAL bug. Data asked after
   a fresh boot, before toggling: `dumpsys hdmi_control` (`mIsCecAvailable`,
   the CEC message history) and `dumpsys audio | grep -E
   "mFullVolumeDevices|mHdmiPlaybackClient|mHdmiCecVolumeControlEnabled"`.
   The passthrough port also keeps one empty dynamic profile per format
   before the filled ones; harmless for the `getFirstValidProfile*()`
   lookups, the selection of a direct output with them is not verified.

## TODO

Waiting for data:

1. Known issues 1 and 2, once the tester's next results arrive.

Verification:

2. The "not verified" / "not run" rows above, VTS first.

Waiting for a maintainer decision:

3. AAC (ADTS framing) passthrough through FFmpeg, and plain DTS through
   FFmpeg when `ffmpeg_passthrough` is set (adds little endian and 14-bit
   DTS). Take the transport parameters from
   `external/ffmpeg/libavformat/spdifenc.c`, never from memory
   (`docs/WORKFLOW.md`).

4. HDMI volume behavior (known issue 2): whether to work around the
   framework (the routing app could set HDMI to full volume with
   `AudioManager.setDeviceVolumeBehavior()`, a system API that needs
   `MODIFY_AUDIO_ROUTING`; also the only way on devices without HDMI-CEC)
   or fix AudioService once the cause is confirmed.

Deferred on purpose: see README, "Known limitations".
