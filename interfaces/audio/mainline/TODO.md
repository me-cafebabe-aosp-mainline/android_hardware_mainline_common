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
| Bitstreaming AC-3, DTS | Intel PantherPoint HDMI (`HDA-Intel`, x86_64 build) with a Mi TV: `AudioTrack` direct playback opens as 2 channels at the content rate with the non-audio channel status, runs in real time, routed to HDMI (the TV's decoding was not checked by ear) |
| Bitstreaming E-AC-3 (incl. JOC) | same setup: 2 channels at 192 kHz opens only when the PCM device offers 192 kHz (the kernel takes the PCM rates from the sink's ELD); the Mi TV's ELD lists PCM up to 48 kHz, so E-AC-3 is not offered (`PcmLimits`). With the ELD edited through `/proc/asound/card0/eld#3.0` to a receiver's, it opens and plays. JOC not verified |
| Bitstreaming DTS-HD, DTS-HD MA, TrueHD, 7.1 IEC 61937 (high bit rate) | Intel PantherPoint (pins report `HBR`): app packed IEC 61937 at 8 channels / 192 kHz opens and sets the pin to HBR (`Pin-ctls: 0x43`) once the ELD lists 8 channel PCM at 192 kHz (edited through proc; the Mi TV lists 2 channels). Real HBR sink and FFmpeg formats not verified. **Fails** on the Tegra setup with a mainline kernel, see known issue 1 |
| HDA pin capabilities in the HBR diagnostics (`HdaPinDetail()`) | Intel PantherPoint: "HDMI pins report HBR" in the log and in the diagnostics page |
| UI sounds during a direct / passthrough stream (arbiter pre-emption) | Intel PantherPoint: a PCM stream next to an AC-3 passthrough stream, no underruns or errors in either. By ear not verified |
| FAST primary output (`fast_latency_ms`) | Intel PantherPoint HDMI, `fast_latency_ms=5`: the output is FAST with a 240 frame buffer, AudioFlinger runs a FastMixer and grants a low latency `AudioTrack` its fast track, the HDMI PCM opens with the 240 frame period, 8 s without underruns; the diagnostics page says so. The speaker / analog outputs, other rates and `fast_period` warnings not exercised |
| VTS `VtsHalAudioCoreTargetTest` (dynamic `hdmi passthrough` port, probe answer in `setAudioPortConfig()`) | Run on the x86_64 build with framework stopped (`setprop vts.native_server.on 1`): 332 pass, 2 fail (known issue 3), the rest skipped. The tester's run on Tegra had 281 failures, see known issue 4. Other VTS modules not run |
| FFmpeg packer: flush / seek (the packer is rebuilt), positions for A/V sync | not verified |
| `IControl` version 2 (HDMI / FAST diagnostics, `probeHdmi()`, `resetHbr()`) and the settings app pages using it | x86_64 build: the diagnostics page shows the HDMI sink, passthrough formats, HBR state and what Android offers; "Test this HDMI output" reports per configuration the failing step and errno. `resetHbr()` and the FAST rows not exercised |
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

3. VTS `VtsHalAudioCoreTargetTest` fails two tests on the `default` module
   that come from our configuration, not from the environment:
   - `AudioCoreModule.CheckMixPortsFlags`: `multichannel output` and
     `hdmi passthrough` both have plain `DIRECT` and both route to HDMI; the
     test (AIDL v4) wants distinct flags per routable device. Changing
     the flags changes how the policy chooses the passthrough output (the
     policy adds `IEC958_NONAUDIO` to framed IEC 61937 streams itself, so that
     one is not a candidate without a look at `AudioPolicyManager`), and
     needs a decision and a test with the HDMI sink.
   - `AudioCoreConfig.GetEngineConfigIsValid`: the `call_assistant` group of
     `config/audio_policy_engine_product_strategies.xml` has no `streamType`
     on purpose (commit `ff5db68`: the schema has no `AUDIO_STREAM_CALL_ASSISTANT`
     and the HAL aborted at start-up), and the test refuses `INVALID` for the
     default engine. Needs a valid stream type for that group, or the group
     gone if the framework allows it.
4. The tester's VTS run (Tegra, Android TV, 281 failures in
   `VtsHalAudioCoreTargetTest`) does not reproduce on the x86_64 build, where
   `CanBeRestarted` passes (the HAL restarts within a second after
   `sys.audio.restart.hal=1`). In the tester's result, 221 failures are
   `r_submix` tests whose first call, `setModuleDebug()`, fails with
   `EX_ILLEGAL_STATE` because the module still has an external device
   connected, and every `CanBeRestarted` fails; together this looks like the
   HAL not being restarted when the framework is stopped, so it keeps what
   audioserver connected. The result files have no logs. Data asked: `adb logcat -b all`
   of a run, `dmesg | grep avc`, `getprop | grep init.svc.*audio`, and
   whether the image's `audioserver.rc` is the stock one.

## TODO

Waiting for data:

1. Known issues 1, 2 and 4, once the tester's next results arrive.

Verification:

2. The "not verified" / "not run" rows above. Test setup on the x86_64
   build: `adb install --force-non-staged` replaces the HAL APEX (plain
   `--non-staged` refuses it because the APEX contains the settings app);
   `AudioTrack` direct playback of ffmpeg made AC-3 / E-AC-3 / DTS files with
   `app_process`; `echo "sadN_rates ..." > /proc/asound/card0/eld#3.0` edits
   the kernel's view of the sink (rates, channels), so a receiver can be
   imitated; `setprop vts.native_server.on 1` before running the VTS binary
   pushed to `/data/local/tmp`.

Waiting for a maintainer decision:

3. AAC (ADTS framing) passthrough through FFmpeg, and plain DTS through
   FFmpeg when `ffmpeg_passthrough` is set (adds little endian and 14-bit
   DTS). Take the transport parameters from
   `external/ffmpeg/libavformat/spdifenc.c`, never from memory
   (`docs/WORKFLOW.md`).

4. Known issue 3: how to give the two DIRECT ports distinct flags, and the
   stream type of `call_assistant`.

5. HDMI volume behavior (known issue 2): whether to work around the
   framework (the routing app could set HDMI to full volume with
   `AudioManager.setDeviceVolumeBehavior()`, a system API that needs
   `MODIFY_AUDIO_ROUTING`; also the only way on devices without HDMI-CEC)
   or fix AudioService once the cause is confirmed.

Deferred on purpose: see README, "Known limitations".
