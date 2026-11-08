/* Host unit tests for classic_input. No framework: CHECK counts and reports. */
#include <limits.h>
#include <stddef.h>
#include <stdio.h>

#include "classic_input.h"

static int g_checks = 0;
static int g_failed = 0;

#define CHECK(cond)                                                  \
    do {                                                             \
        g_checks++;                                                  \
        if (!(cond)) {                                               \
            g_failed++;                                              \
            printf("%s:%d: check failed: %s\n", __FILE__, __LINE__, #cond); \
        }                                                            \
    } while (0)

#define FIELD_COUNT 11

/* Arbitrary distinct masks: classic bits 16..26, core bits 0..10. The test does
 * not depend on the libogc numeric values; main.c supplies the real ones. */
static const ClassicButtonMasks k_classic = {
    1u << 16, 1u << 17, 1u << 18, 1u << 19, 1u << 20, 1u << 21,
    1u << 22, 1u << 23, 1u << 24, 1u << 25, 1u << 26
};

static const ClassicButtonMasks k_core = {
    1u << 0, 1u << 1, 1u << 2, 1u << 3, 1u << 4, 1u << 5,
    1u << 6, 1u << 7, 1u << 8, 1u << 9, 1u << 10
};

static const size_t k_offsets[FIELD_COUNT] = {
    offsetof(ClassicButtonMasks, up),    offsetof(ClassicButtonMasks, down),
    offsetof(ClassicButtonMasks, left),  offsetof(ClassicButtonMasks, right),
    offsetof(ClassicButtonMasks, a),     offsetof(ClassicButtonMasks, b),
    offsetof(ClassicButtonMasks, one),   offsetof(ClassicButtonMasks, two),
    offsetof(ClassicButtonMasks, plus),  offsetof(ClassicButtonMasks, minus),
    offsetof(ClassicButtonMasks, home)
};

static const int k_edges[] = {
    INT_MIN, INT_MIN + 1, -1, 0, 1, 50, 100, INT_MAX - 1, INT_MAX
};

#define EDGE_COUNT (sizeof(k_edges) / sizeof(k_edges[0]))

static unsigned int field_of(const ClassicButtonMasks *m, int index)
{
    return *(const unsigned int *)((const char *)m + k_offsets[index]);
}

static unsigned int all_classic(void)
{
    unsigned int bits = 0u;
    int i;

    for (i = 0; i < FIELD_COUNT; i++) {
        bits |= field_of(&k_classic, i);
    }
    return bits;
}

static unsigned int all_core(void)
{
    unsigned int bits = 0u;
    int i;

    for (i = 0; i < FIELD_COUNT; i++) {
        bits |= field_of(&k_core, i);
    }
    return bits;
}

static void test_single_buttons(void)
{
    int i;

    for (i = 0; i < FIELD_COUNT; i++) {
        unsigned int raw = field_of(&k_classic, i);
        unsigned int want = raw | field_of(&k_core, i);
        unsigned int out = classic_translate_buttons(raw, 1, &k_classic, &k_core);

        CHECK(out == want);
        CHECK((out & raw) == raw);
    }
}

static void test_x_and_y_slots(void)
{
    unsigned int out_x = classic_translate_buttons(k_classic.one, 1, &k_classic, &k_core);
    unsigned int out_y = classic_translate_buttons(k_classic.two, 1, &k_classic, &k_core);

    CHECK((out_x & k_core.one) != 0u);
    CHECK((out_x & k_core.two) == 0u);
    CHECK((out_y & k_core.two) != 0u);
    CHECK((out_y & k_core.one) == 0u);
}

static void test_combos(void)
{
    unsigned int raw = k_classic.up | k_classic.a | k_classic.one | k_classic.plus;
    unsigned int want = raw | k_core.up | k_core.a | k_core.one | k_core.plus;

    CHECK(classic_translate_buttons(raw, 1, &k_classic, &k_core) == want);
    CHECK(classic_translate_buttons(all_classic(), 1, &k_classic, &k_core) ==
          (all_classic() | all_core()));
}

static void test_flag_zero_strips_classic(void)
{
    unsigned int raw = all_classic() | k_core.a | k_core.up | (1u << 12);
    unsigned int out = classic_translate_buttons(raw, 0, &k_classic, &k_core);

    CHECK(out == (k_core.a | k_core.up | (1u << 12)));
    CHECK((out & all_classic()) == 0u);
    CHECK(classic_translate_buttons(k_classic.a, 0, &k_classic, &k_core) == 0u);
}

static void test_core_untouched_with_classic(void)
{
    unsigned int core_only = k_core.a | k_core.one;

    CHECK(classic_translate_buttons(core_only, 1, &k_classic, &k_core) == core_only);
    CHECK(classic_translate_buttons(k_core.up, 1, &k_classic, &k_core) == k_core.up);
    CHECK(classic_translate_buttons(core_only, 0, &k_classic, &k_core) == core_only);
}

static void test_zero_raw(void)
{
    CHECK(classic_translate_buttons(0u, 1, &k_classic, &k_core) == 0u);
    CHECK(classic_translate_buttons(0u, 0, &k_classic, &k_core) == 0u);
}

static void test_null_tables(void)
{
    unsigned int raw = all_classic() | k_core.b | (1u << 12);

    CHECK(classic_translate_buttons(raw, 1, NULL, &k_core) == raw);
    CHECK(classic_translate_buttons(raw, 1, &k_classic, NULL) == raw);
    CHECK(classic_translate_buttons(raw, 0, NULL, NULL) == raw);
    CHECK(classic_translate_buttons(raw, 1, NULL, NULL) == raw);
}

static void test_idempotent_exhaustive(void)
{
    unsigned int r;
    int bad = 0;

    for (r = 0; r < (1u << FIELD_COUNT); r++) {
        unsigned int raw = (r << 16) | r;
        unsigned int once = classic_translate_buttons(raw, 1, &k_classic, &k_core);
        unsigned int twice = classic_translate_buttons(once, 1, &k_classic, &k_core);
        unsigned int stripped = classic_translate_buttons(raw, 0, &k_classic, &k_core);

        if (once != twice) bad++;
        if ((stripped & all_classic()) != 0u) bad++;
    }
    CHECK(bad == 0);
}

static void test_unknown_bits_preserved(void)
{
    unsigned int unknown = (1u << 12) | (1u << 31);
    unsigned int raw = unknown | k_classic.a;

    CHECK((classic_translate_buttons(raw, 1, &k_classic, &k_core) & unknown) == unknown);
    CHECK(classic_translate_buttons(raw, 0, &k_classic, &k_core) == unknown);
}

static void test_stick_center_and_deadzone(void)
{
    CHECK(classic_stick_norm(32, 32, 0, 63) == 0);
    CHECK(classic_stick_norm(0, 0, -100, 100) == 0);
    CHECK(classic_stick_norm(10, 0, -100, 100) == 0);
    CHECK(classic_stick_norm(-10, 0, -100, 100) == 0);
    CHECK(classic_stick_norm(11, 0, -100, 100) > 0);
    CHECK(classic_stick_norm(-11, 0, -100, 100) < 0);
}

static void test_stick_full_and_clamp(void)
{
    CHECK(classic_stick_norm(100, 0, -100, 100) == 100);
    CHECK(classic_stick_norm(-100, 0, -100, 100) == -100);
    CHECK(classic_stick_norm(500, 0, -100, 100) == 100);
    CHECK(classic_stick_norm(-500, 0, -100, 100) == -100);
    CHECK(classic_stick_norm(INT_MAX, 0, -100, 100) == 100);
    CHECK(classic_stick_norm(INT_MIN, 0, -100, 100) == -100);
}

static void test_stick_asymmetric_span(void)
{
    CHECK(classic_stick_norm(63, 32, 0, 63) == 100);
    CHECK(classic_stick_norm(0, 32, 0, 63) == -100);
    CHECK(classic_stick_norm(32, 32, 0, 63) == 0);
}

static void test_stick_degenerate_spans(void)
{
    CHECK(classic_stick_norm(60, 50, 0, 50) == 0);
    CHECK(classic_stick_norm(40, 50, 50, 100) == 0);
    CHECK(classic_stick_norm(60, 32, 40, 20) == 0);
    CHECK(classic_stick_norm(10, 32, 40, 20) == 0);
    CHECK(classic_stick_norm(5, 5, 5, 5) == 0);
}

static void test_stick_extremes_no_overflow(void)
{
    int bad = 0;
    size_t a, b, c, d;

    for (a = 0; a < EDGE_COUNT; a++) {
        for (b = 0; b < EDGE_COUNT; b++) {
            for (c = 0; c < EDGE_COUNT; c++) {
                for (d = 0; d < EDGE_COUNT; d++) {
                    int r = classic_stick_norm(k_edges[a], k_edges[b], k_edges[c], k_edges[d]);
                    if (r < -100 || r > 100) bad++;
                }
            }
        }
    }
    CHECK(bad == 0);
    CHECK(classic_stick_norm(INT_MAX, INT_MIN, INT_MIN, INT_MAX) == 100);
    CHECK(classic_stick_norm(INT_MIN, INT_MAX, INT_MIN, INT_MAX) == -100);
}

static void test_stick_symmetry_grid(void)
{
    static const int centers[] = {0, 32, -5, 1000};
    static const int spans[] = {1, 10, 31, 100, 1000};
    size_t ci, si;
    int bad = 0;

    for (ci = 0; ci < sizeof(centers) / sizeof(centers[0]); ci++) {
        for (si = 0; si < sizeof(spans) / sizeof(spans[0]); si++) {
            int center = centers[ci];
            int span = spans[si];
            int d;

            for (d = 0; d <= span; d++) {
                int plus = classic_stick_norm(center + d, center, center - span, center + span);
                int minus = classic_stick_norm(center - d, center, center - span, center + span);
                if (plus != -minus) bad++;
            }
        }
    }
    CHECK(bad == 0);
}

static void test_link_truth_table(void)
{
    static const int errs[] = {0, -1, -2, -3};
    static const int kinds[] = {0, 1, 2};
    size_t e, k;

    for (e = 0; e < sizeof(errs) / sizeof(errs[0]); e++) {
        for (k = 0; k < sizeof(kinds) / sizeof(kinds[0]); k++) {
            int want = (errs[e] == 0 && kinds[k] != 0) ? 1 : 0;
            CHECK(classic_link_ok(errs[e], kinds[k]) == want);
        }
    }
}

int main(void)
{
    test_single_buttons();
    test_x_and_y_slots();
    test_combos();
    test_flag_zero_strips_classic();
    test_core_untouched_with_classic();
    test_zero_raw();
    test_null_tables();
    test_idempotent_exhaustive();
    test_unknown_bits_preserved();
    test_stick_center_and_deadzone();
    test_stick_full_and_clamp();
    test_stick_asymmetric_span();
    test_stick_degenerate_spans();
    test_stick_extremes_no_overflow();
    test_stick_symmetry_grid();
    test_link_truth_table();

    printf("%d checks, %d failed\n", g_checks, g_failed);
    return g_failed == 0 ? 0 : 1;
}
