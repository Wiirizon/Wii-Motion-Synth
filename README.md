# Wii Motion Synth

Wii Motion Synth is a standalone Nintendo Wii homebrew application written in
C. It turns Wiimote motion into a real-time audio synthesizer with selectable
waveforms and a GX oscilloscope.

See [CHANGELOG.md](CHANGELOG.md) for release history and
[CONTRIBUTING.md](CONTRIBUTING.md) for the development workflow.

The current application metadata identifies the developer as `Wiirizon` and
the version as `1.3.0`.

## Features

- 48 kHz mono synthesis with double-buffered audio output through `libasnd`.
- Sine, square, sawtooth, triangle, white-noise, pulse, and narrow-pulse
  waveforms.
- Tilt maps to 100-1000 Hz by default. Octave shift and the Nunchuk LFO can push the output up to the 100-2100 Hz safe clamp. Volume is mapped from roll.
- Adjustable motion sensitivity (25-400 %) and pitch range (100-1050 Hz) from an
  in-app settings menu; every value is bounded. See [docs/SETTINGS.md](docs/SETTINGS.md).
- Optional C-major pentatonic quantization.
- Nunchuk joystick vibrato and tremolo.
- Classic Controller buttons and sticks; see [controller mappings](docs/CONTROLLERS.md).
- Adjustable audio buffer size and output volume.
- GX waveform oscilloscope and a text statistics screen.
- DMA-safe, 32-byte-aligned buffers and lookup-table-based sine generation.

## Controls

| Input | Action |
| --- | --- |
| Tilt up/down | Adjust pitch, 100-1000 Hz (octave shift and LFO can reach the 2100 Hz clamp) |
| Roll left/right | Adjust volume |
| Hold `A` | Square wave |
| Hold `1` | Sawtooth wave |
| D-pad up | Triangle wave |
| D-pad down | White noise |
| D-pad left | Pulse wave |
| D-pad right | Narrow pulse |
| Hold `B` | Quantize pitch to the C-major pentatonic scale |
| Hold `2` | Octave shift |
| Hold `+` | Mute gate |
| Hold `+`, press `-` | Open or close the settings menu (D-pad to navigate, `A` for Reset/Back) |
| `-` | Toggle oscilloscope/statistics UI |
| Nunchuk joystick | Vibrato and tremolo LFO |
| Classic Controller | Mirrored button controls, left stick Y vibrato, right stick X tremolo |
| `HOME` | Exit |

When no waveform selector is held, the application uses the sine waveform.

## Installation on Real Hardware

1. Format an SD card as FAT32.
2. Download `WiiMotionSynth-v1.3.0.zip` from the
   [v1.3.0 GitHub release](https://github.com/Wiirizon/Wii-Motion-Synth/releases/tag/v1.3.0).
3. Extract the archive at the SD-card root. The resulting application directory
   is `apps/WiiMotionSynth/` and contains `boot.dol`, `meta.xml`, and
   `icon.png`.
4. Launch Wii Motion Synth through the Homebrew Channel.

The application expects a compatible Wiimote. A Nunchuk is optional; without
one, the Nunchuk LFO remains inactive.

## Building from Source

### Prerequisites

- [devkitPro](https://devkitpro.org/)
- The `wii-dev` package, including devkitPPC, libogc, wiiuse, and elf2dol
- `make`

Set `DEVKITPRO` to the devkitPro installation directory and `DEVKITPPC` to the
devkitPPC directory. The Makefile intentionally uses these environment
variables instead of machine-specific paths.

### Build

```sh
git clone https://github.com/Wiirizon/Wii-Motion-Synth.git
cd Wii-Motion-Synth
make
```

Output files:

- `build/WiiMotionSynth.elf`: linked development executable.
- `build/WiiMotionSynth.dol`: Wii executable for the Homebrew Channel.

Clean generated output with:

```sh
make clean
```

### Manual Packaging

To reproduce the release archive after building:

```sh
mkdir -p apps/WiiMotionSynth
cp build/WiiMotionSynth.dol apps/WiiMotionSynth/boot.dol
cp meta.xml apps/WiiMotionSynth/
cp icon.png apps/WiiMotionSynth/
zip -r WiiMotionSynth-v1.3.0.zip apps/
```

The GitHub Actions workflow performs these steps automatically when manually
dispatched. It builds in the `devkitpro/devkitppc:latest` container and
publishes a rolling `latest` prerelease.

## Troubleshooting

### `wii.specs` cannot be found

This error usually means the compiler is not using the devkitPPC Wii
specification files. Check that both `DEVKITPRO` and `DEVKITPPC` are set in
the same shell where `make` is run, and verify that the `wii-dev` package is
installed through devkitPro's package manager.

On Windows, start a devkitPro/MSYS2 shell or load the devkitPro environment
before building. Do not hard-code a local installation path in the Makefile.
After correcting the environment, run:

```sh
make clean
make
```

If the error persists, confirm that `powerpc-eabi-gcc` and `elf2dol` are
available on the configured devkitPro installation and reinstall the `wii-dev`
package if necessary.

## Architecture

The project is intentionally small:

- `main.c` initializes video, input, and audio; maps motion to synthesis
  parameters; renders the UI; and implements the audio callback.
- `synth_core.c` / `synth_core.h` hold the pure logic (settings and their
  bounds, pitch and roll mapping, quantization, phase step). They use no
  libogc header, so the host tests build them directly.
- `tests/` contains the host unit tests and a static guard for the audio
  callback (see Development Notes).
- `Makefile` compiles `main.c` with devkitPPC and converts the ELF to a DOL.
- `meta.xml` supplies Homebrew Channel metadata and documents the controls.
- `.github/workflows/build.yml` defines the manual build-and-release workflow.
- `icon.png` is the Homebrew Channel icon.

The audio callback uses a phase accumulator and a 2048-entry sine lookup table.
The main loop updates `g_phase_step`, `g_vol_scale`, and the selected waveform;
the callback consumes those values while filling the alternating audio buffers.

White noise uses a xorshift32 generator. Its state is a `static` variable
seeded from the clock at startup and advanced only by the audio callback.

## Development Notes

The Wii audio callback is real-time code. Do not introduce blocking calls,
dynamic allocation, or expensive floating-point calculations there. Keep
buffers aligned and flush cache ranges before passing data to DMA-backed audio
 APIs. Do not call libc `rand()` or other shared-state functions there; keep
generator state in `static` variables that only the callback updates.
Changes to controls must be reflected in this README and `meta.xml`.

Host unit tests cover the pure logic in `synth_core.c`. They need only `gcc`
and `make`, not devkitPPC:

```sh
make -C tests            # unit tests + audio_callback static guard
make -C tests SAN=1      # same with UndefinedBehaviorSanitizer (needs libubsan)
```

`tests/check_callback.sh` fails if `audio_callback` mentions allocation, file
I/O, `rand()`, locks, floating point, or the settings module. A successful
`make` is the build validation for the console code; input and audio changes
also require testing on real Wii hardware.

## Known Limitations

- Audio, motion input, GX rendering, and Homebrew Channel packaging cannot be
  fully verified without compatible Wii hardware and a Wiimote.
- The host tests cover the pure logic only. A clean `make` validates
  compilation of the console code, while the settings menu, input, and audio
  behavior require manual hardware testing.
- The rolling `latest` GitHub release is a manually dispatched prerelease, not
  a fixed-version release artifact.

## License

This project is distributed under the MIT License. See [LICENSE](LICENSE).
