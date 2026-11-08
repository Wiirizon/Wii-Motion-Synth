/*
 * Pure Classic Controller input mapping. No libogc or devkitPPC headers: this
 * module builds on the host so tests/ can exercise it without the Wii toolchain.
 * main.c supplies the real WPAD_* mask values; this file never names them.
 */
#ifndef CLASSIC_INPUT_H
#define CLASSIC_INPUT_H

/* One bit mask per logical button. For the classic table, `one` means the X
 * button and `two` means the Y button. For the core (Wiimote) table, `one` and
 * `two` are the Wiimote 1 and 2 buttons. */
typedef struct {
    unsigned int up;
    unsigned int down;
    unsigned int left;
    unsigned int right;
    unsigned int a;
    unsigned int b;
    unsigned int one;
    unsigned int two;
    unsigned int plus;
    unsigned int minus;
    unsigned int home;
} ClassicButtonMasks;

/* Returns the button word with Classic buttons mirrored onto the Wiimote bits.
 * expansion_is_classic != 0: each Classic bit set in raw also sets its core bit.
 * expansion_is_classic == 0: every Classic mask bit is cleared, so absent or
 * unsupported hardware cannot inject buttons. Core bits and unknown bits are
 * left unchanged in both cases. NULL tables return raw unchanged. */
unsigned int classic_translate_buttons(unsigned int raw, int expansion_is_classic,
                                       const ClassicButtonMasks *classic,
                                       const ClassicButtonMasks *core);

/* Maps a stick position to -100..100 relative to center. Each side is scaled by
 * its own span (max - center above center, center - min below it). Results with
 * an absolute value of 10 or less are 0. Degenerate spans return 0. */
int classic_stick_norm(int pos, int center, int min, int max);

/* Returns 1 only when the WPAD status is WPAD_ERR_NONE (0) and the expansion is
 * a Classic Controller. Used to gate stick reads. */
int classic_link_ok(int wpad_err, int expansion_is_classic);

#endif /* CLASSIC_INPUT_H */
