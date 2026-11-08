# Controllers

Wii Motion Synth reads one Wiimote on channel 0. Motion (pitch and roll) always
comes from the Wiimote accelerometer. An expansion adds extra inputs: a Nunchuk
or a Classic Controller. The Wiimote motion workflow is the same with either
expansion.

## Classic Controller mapping

The Classic Controller plugs into the Wiimote, so its buttons are mirrored onto
the Wiimote button bits. Every existing Wiimote action then works from the
Classic Controller without a separate code path.

| Classic Controller | Acts as Wiimote | Effect in the app |
| --- | --- | --- |
| D-pad up | D-pad up | Hold: triangle wave |
| D-pad down | D-pad down | Hold: noise |
| D-pad left | D-pad left | Hold: pulse wave |
| D-pad right | D-pad right | Hold: narrow pulse wave |
| A | A | Hold: square wave |
| B | B | Hold: snap to scale |
| X | 1 | Hold: sawtooth wave |
| Y | 2 | Hold: octave shift |
| + | + | Mute gate, settings menu (with -) |
| - | - | Oscilloscope toggle, settings menu (with +) |
| HOME | HOME | Exit |

Sticks (only with a Classic Controller):

| Stick | Effect |
| --- | --- |
| Left stick Y | Frequency vibrato (LFO offset in Hz), same role as the Nunchuk joystick Y |
| Right stick X | Tremolo (volume reduction), same role as the Nunchuk joystick X |

Stick values are normalized to -100..100 around the calibrated center, with a
deadzone of 10. The Nunchuk path is unchanged.

## Connection and disconnection

- Connection state is checked every frame. Classic buttons are translated from
  the current frame's data, so a disconnect never leaves a button latched.
- Classic buttons are stripped and the sticks are ignored when the pad reports
  any error, or when the expansion is not a Classic Controller (for example
  after a Classic is unplugged, or a Nunchuk is attached).
- Sticks only change the LFO offset and the volume target in the control loop.
  They never run inside `audio_callback`.

## Not supported

- Wii U Pro Controller and GameCube controllers.
- A second Wiimote (only channel 0 is read).
- Classic Controller Pro: not tested. It uses the same data format as the
  Classic Controller, so it may work, but this is unverified.
- Nunchuk and Classic Controller on the same Wiimote: the Wiimote reports one
  expansion at a time.

## Not verified

- No hardware test. The mapping and the stick scaling have not been checked on
  a Wii or with a real Classic Controller.
- `main.c` is not built in this environment (no libogc or devkitPPC). The
  `WPAD_CLASSIC_BUTTON_*` macros and the `exp.classic.ljs` / `rjs` fields
  (`pos`, `center`, `min`, `max`) were checked by reading the libogc master
  headers; the libogc version installed with devkitPro may differ.
- The stick deadzone (10) and the LFO scaling (divisor 60000) were carried over
  from the Nunchuk path and not tuned for the Classic Controller.
- Host tests cover `classic_input.c` only. The control loop in `main.c` is
  covered by the review and by `tests/check_callback.sh` (audio callback only).
