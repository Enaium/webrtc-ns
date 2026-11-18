# WebRTC NS

A standalone extraction of the WebRTC noise suppressor
(`modules/audio_processing/ns`) from the WebRTC source tree, packaged as a
shared library with a small public API. The project layout and pixi/CMake
workflow follow the [aec3](../aec3) extraction.

## Upstream

- WebRTC revision: `c25c6676c6d09bd3161e1cf73d3b362c8875e8d0` (2026-11-18).
- `modules/audio_processing/ns` is byte-identical to that revision apart from
  include paths; there were no upstream NS changes to pull in.
- The band-splitting support (`splitting_filter`, `three_band_filter_bank` and
  the float QMF) was refreshed to the same revision.

## What is included

- `src/audio_processing/ns/` — the complete NS module: 256-point FFT,
  quantile noise estimation, speech probability estimation, Wiener filtering,
  and the multi-band noise suppressor.
- `src/audio_processing/` — the `AudioBuffer`, channel buffer, splitting
  filter, three-band filter bank and resampler support code needed by the NS
  interface.
- `src/common_audio/third_party/ooura/fft_size_256/` — the Ooura real FFT used
  by NS.
- `src/api/noise_suppression.h` — a self-contained API that does not require
  callers to construct WebRTC's internal `AudioBuffer`.

The NS DSP code is unchanged WebRTC code; only include paths were adapted to
the flat extraction layout (`modules/audio_processing/...` →
`audio_processing/...`). The surrounding standalone support code follows the
same upstream revision where its API allows it.

## Build

This project uses [pixi](https://pixi.sh) with CMake and Ninja.

```bash
pixi run -e build configure
pixi run -e build build
```

This produces `libwebrtc_ns.dylib` (macOS), `libwebrtc_ns.so` (Linux), or
`webrtc_ns.dll` (Windows) in `.build/`.

## Test

```bash
pixi run -e build configure-test
pixi run -e build build-test
pixi run -e build run-test
```

`tests/ns_test` processes `tests/noisy.wav` and writes `tests/denoised.wav`.
`tests/noisy.wav` is `tests/clean.wav` (16 kHz speech taken from the aec3
project's test data) mixed with deterministic white Gaussian noise at 10 dB
SNR. When a clean reference is supplied, the
driver estimates the algorithm delay by cross-correlation and reports:

- global SNR before/after,
- noise attenuation on noise-only frames,
- speech level change on active frames.

The default 16 kHz test currently reports roughly:

```
Input SNR          : 10.0 dB
Output SNR         : 13.4 dB
SNR improvement    : 3.4 dB
Noise attenuation  : 11.1 dB
Active-frame gain  : -0.9 dB
```

The test exits non-zero if the output is silent, the noise is not attenuated,
or the speech level collapses.

### Command line

```
ns_test [input.wav] [output.wav] [clean_reference.wav] [level]
```

- `input.wav` — 16-bit PCM, 16/32/48 kHz, any channel count.
- `output.wav` — default `tests/denoised.wav`.
- `clean_reference.wav` — optional; pass `""` to skip metrics.
- `level` — `6`, `12`, `18` or `21` dB suppression; default `12`.

Example:

```bash
.build-tests/ns_test tests/noisy.wav tests/denoised.wav tests/clean.wav 18
```

## API

```cpp
#include "api/noise_suppression.h"

webrtc::NoiseSuppression::Config config;
config.level = webrtc::NoiseSuppression::Level::k12dB;

webrtc::NoiseSuppression ns(48000, 2, config);

// 10 ms deinterleaved frame, float samples in the int16 range
// [-32768, 32767]. num_frames_per_channel() == 480 at 48 kHz.
ns.Analyze(capture_channels);
// Insert AEC/AGC here if required; they may modify the same buffers.
ns.Process(capture_channels);
```

`Analyze` must be called before `Process` for each frame. Both methods accept
the same deinterleaved channel pointers; `Process` works in place. Instances
are not thread-safe.

The NS output is delayed by the overlap-add synthesis (6 ms at 16 kHz, plus
the analysis/synthesis filter-bank delay at 32/48 kHz). A streaming consumer
can leave this constant delay in place or compensate it when aligning the
output against another signal; the test driver estimates and compensates it
for metrics only.

Supported rates are 16, 32 and 48 kHz. At 32/48 kHz WebRTC splits the signal
into 16 kHz bands and merges it after processing. That filter bank is not
perfect-reconstruction, so its own distortion limits the measured SNR at
48 kHz even though noise is attenuated. This behaviour is inherited from
WebRTC's NS pipeline.

## License

BSD 3-Clause (same as WebRTC). See [LICENSE](LICENSE).
