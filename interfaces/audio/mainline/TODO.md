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
| Passthrough formats reaching the policy (TV Settings → Surround sound) | **fails** on the same setup, see known issue 2 |
| Bitstreaming AC-3, E-AC-3 (incl. JOC), DTS | not verified |
| Bitstreaming DTS-HD, DTS-HD MA, TrueHD, 7.1 IEC 61937 (high bit rate) | **fails** on the same setup, see known issue 1 |
| UI sounds during a direct / passthrough stream (arbiter pre-emption) | not verified |
| FAST primary output (`fast_latency_ms`): FastMixer, fast tracks | not verified |
| VTS (`VtsHalAudioCoreModuleTargetTest`), in particular the dynamic `hdmi passthrough` port and the probe answer in `setAudioPortConfig()` | not run |
| FFmpeg packer: flush / seek (the packer is rebuilt), positions for A/V sync | not verified |

## Known issues

1. High bit rate (8 channels at 192 kHz) is refused on Tegra already at
   `snd_pcm_hw_params` with `-EINVAL`, not at prepare time as expected from
   the HDA HDMI pin check. The kernel narrows the PCM to the ELD, which
   announces 8 channels and 192 kHz, so the limit is the HDA codec /
   controller, the Tegra display side, or the period / buffer sizes of
   `PassthroughSink::Open()`. Data asked from the tester: Tegra SoC,
   `/proc/asound/card0/eld#*`, `/proc/asound/card0/codec#*` (HBR, rates,
   channels; both are in the settings app's report now),
   `tinypcminfo -D 0 -d 3`, `dmesg` (hda / hdmi / sor). Also
   asked: retry with Kodi's TrueHD / DTS-HD receiver options off, to see
   whether the 2 channel formats work.
2. The formats do not show in Android TV's Surround sound settings,
   although the HAL fills the passthrough port. They get there through the
   policy's probe of the dynamic port (see `AGENTS.md`), which this is the
   first real test of. Data asked: `dumpsys media.audio_policy`, logcat for
   `CompletePassthroughProbe|UpdatePassthroughMixPort|updateAudioProfiles`,
   the Surround sound mode.
3. After one refused high bit rate open, the head never offers high bit
   rate again until the HAL restarts (`HbrFailures`), and any `-EINVAL`
   counts, including one caused by the buffer sizes. README claims it only
   lasts until the next connection, which is wrong.

## TODO

Promised to the tester:

1. Known issue 3: on a refused strict open, retry with relaxed period /
   buffer sizes before suspecting high bit rate; record an HBR failure only
   when the device refuses 8 channels at 192 kHz itself; forget failures
   when HDMI reconnects or the sink changes; fix the README sentence.

Waiting for data:

2. Known issues 1 and 2, once the tester's logs arrive.

Verification:

3. The "not verified" / "not run" rows above, VTS first.

Waiting for a maintainer decision:

4. AAC (ADTS framing) passthrough through FFmpeg, and plain DTS through
   FFmpeg when `ffmpeg_passthrough` is set (adds little endian and 14-bit
   DTS). Take the transport parameters from
   `external/ffmpeg/libavformat/spdifenc.c`, never from memory
   (`docs/WORKFLOW.md`).

Deferred on purpose: see README, "Known limitations".
