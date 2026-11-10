# Changelog

All notable changes to Wii Motion Synth are documented here.

## [1.3.0] - 2026-11-10

### Added

- Classic Controller button mappings and analog sticks for vibrato and
  tremolo. See [controller documentation](docs/CONTROLLERS.md).
- Settings for audio buffer size and output volume; both are saved in presets.

### Fixed

- Audio output now starts silent before the welcome screen, resolving the
  startup beep reported in [issue #11](https://github.com/Wiirizon/Wii-Motion-Synth/issues/11).

### Hardware validation

- The user tested the release build on Wii and reports that the startup beep
  is gone.
- Classic Controller hardware was not available for testing; its button and
  stick mappings remain unverified on console.

## [1.2.0] - 2026-11-05

### Added

- In-app settings for motion sensitivity and pitch range, with bounded values
  and safe defaults.
- Four SD-card preset slots for saving and loading settings.

### Fixed

- Tremolo volume underflow, invalid sensor-derived pitch conversion, and
  per-sample random-number generation in the audio callback.
- Synth frequency is clamped to 100-2100 Hz after quantization, octave shift,
  and Nunchuk LFO. Tilt maps to 100-1000 Hz; octave shift and the LFO can push
  the output to the 2100 Hz ceiling, and the displayed frequency uses the
  same clamped value.
- Non-finite pitch values map to the 100 Hz floor instead of an undefined
  float-to-integer conversion.
- White-noise generation uses a bounded xorshift32 generator in the audio
  callback instead of calling `rand()` per sample.

### Hardware validation

- The user reports that existing features and the new settings/presets appear
  to work on Wii hardware.
- A brief beep before the welcome screen was reported; it stops when the
  welcome screen appears and has not been diagnosed. See
  [issue #11](https://github.com/Wiirizon/Wii-Motion-Synth/issues/11).

## [1.1.1] - 2026-11-01

### Changed

- Updated the Homebrew Channel developer metadata to `Wiirizon`.
- Bumped the application version metadata to `1.1.1`.

## [1.1.0] - 2026-10-22

### Added

- Triangle, white-noise, pulse, and narrow-pulse waveforms on the D-pad.
- Octave shift while holding `2`.
- Mute gate while holding `+`.
- Nunchuk joystick vibrato and tremolo controls.
- Random seed initialization for distinct white-noise output.

### Changed

- Updated the startup and statistics screens to document the expanded control
  scheme.
- Removed the non-functional Wiimote speaker behavior.
- Corrected Nunchuk LFO phase progression for the 60 FPS main loop.
- Synchronized `README.md` and `meta.xml` with the 1.1.0 controls.

## [1.0.1] - 2026-10-21

### Fixed

- Corrected Homebrew Channel metadata in `meta.xml`.

## [1.0.0] - 2026-10-21

### Added

- Motion-controlled synthesis using Wiimote pitch and roll.
- Sine, square, and sawtooth waveforms.
- C-major pentatonic pitch quantization with `B`.
- GX oscilloscope and text telemetry views toggled with `-`.
- Nunchuk LFO support.
- Double-buffered, DMA-backed audio output.

### Notes

- Initial release for Wii homebrew hardware.
