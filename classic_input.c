#include <stddef.h>
#include "classic_input.h"

#define STICK_DEADZONE 10
#define STICK_FULL_SCALE 100

static unsigned int map_bit(unsigned int raw, unsigned int from, unsigned int to)
{
    return (raw & from) != 0u ? to : 0u;
}

static unsigned int classic_all_bits(const ClassicButtonMasks *m)
{
    return m->up | m->down | m->left | m->right | m->a | m->b |
           m->one | m->two | m->plus | m->minus | m->home;
}

static unsigned int core_equivalents(unsigned int raw, const ClassicButtonMasks *classic,
                                     const ClassicButtonMasks *core)
{
    return map_bit(raw, classic->up, core->up) |
           map_bit(raw, classic->down, core->down) |
           map_bit(raw, classic->left, core->left) |
           map_bit(raw, classic->right, core->right) |
           map_bit(raw, classic->a, core->a) |
           map_bit(raw, classic->b, core->b) |
           map_bit(raw, classic->one, core->one) |
           map_bit(raw, classic->two, core->two) |
           map_bit(raw, classic->plus, core->plus) |
           map_bit(raw, classic->minus, core->minus) |
           map_bit(raw, classic->home, core->home);
}

unsigned int classic_translate_buttons(unsigned int raw, int expansion_is_classic,
                                       const ClassicButtonMasks *classic,
                                       const ClassicButtonMasks *core)
{
    if (classic == NULL || core == NULL) {
        return raw;
    }
    if (!expansion_is_classic) {
        return raw & ~classic_all_bits(classic);
    }
    return raw | core_equivalents(raw, classic, core);
}

int classic_stick_norm(int pos, int center, int min, int max)
{
    long long delta = (long long)pos - (long long)center;
    long long span;
    long long value;

    if (delta == 0) {
        return 0;
    }
    if (delta > 0) {
        span = (long long)max - (long long)center;
    } else {
        span = (long long)center - (long long)min;
    }
    if (span <= 0) {
        return 0;
    }

    value = delta * STICK_FULL_SCALE / span;
    if (value > STICK_FULL_SCALE) {
        value = STICK_FULL_SCALE;
    } else if (value < -STICK_FULL_SCALE) {
        value = -STICK_FULL_SCALE;
    }
    if (value <= STICK_DEADZONE && value >= -STICK_DEADZONE) {
        return 0;
    }
    return (int)value;
}

int classic_link_ok(int wpad_err, int expansion_is_classic)
{
    if (wpad_err != 0) {
        return 0;
    }
    return expansion_is_classic != 0 ? 1 : 0;
}
