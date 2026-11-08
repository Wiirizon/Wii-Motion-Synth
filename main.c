#include <gccore.h>
#include <wiiuse/wpad.h>
#include <asndlib.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <malloc.h>
#include <time.h>
#include <fat.h>

#include "synth_core.h"
#include "preset_store.h"
#include "preset_io.h"
#include "preset_fs_stdio.h"
#include "classic_input.h"

#define SAMPLES_PER_BUFFER_MAX BUFFER_SAMPLES_MAX
#define SINE_LUT_MASK (SINE_LUT_SIZE - 1)
#define PI 3.14159265358979323846f

static volatile WaveType g_current_wave = WAVE_SINE;

static s16 sine_table[SINE_LUT_SIZE] ATTRIBUTE_ALIGN(32);
static s16 audio_buffers[2][SAMPLES_PER_BUFFER_MAX] ATTRIBUTE_ALIGN(32);
static volatile u8 current_buffer = 0;
static u32 phase_accumulator = 0;

static volatile u32 g_phase_step = 0;
// Starts silent: ASND_SetVoice begins playback before the welcome screen runs, so a
// nonzero value here (or before the main loop) is audible as a beep while the SD card mounts.
static volatile u32 g_vol_scale = 0;
// Samples per buffer in use. Changed only by apply_audio_buffer, while the voice is stopped.
static volatile u32 g_buffer_samples = BUFFER_SAMPLES_DEFAULT;

// Classic Controller buttons are mirrored onto the Wiimote bits. Classic X and Y
// use the `one` and `two` slots of the classic table.
static const ClassicButtonMasks k_classic_masks = {
    WPAD_CLASSIC_BUTTON_UP, WPAD_CLASSIC_BUTTON_DOWN,
    WPAD_CLASSIC_BUTTON_LEFT, WPAD_CLASSIC_BUTTON_RIGHT,
    WPAD_CLASSIC_BUTTON_A, WPAD_CLASSIC_BUTTON_B,
    WPAD_CLASSIC_BUTTON_X, WPAD_CLASSIC_BUTTON_Y,
    WPAD_CLASSIC_BUTTON_PLUS, WPAD_CLASSIC_BUTTON_MINUS,
    WPAD_CLASSIC_BUTTON_HOME
};

static const ClassicButtonMasks k_core_masks = {
    WPAD_BUTTON_UP, WPAD_BUTTON_DOWN,
    WPAD_BUTTON_LEFT, WPAD_BUTTON_RIGHT,
    WPAD_BUTTON_A, WPAD_BUTTON_B,
    WPAD_BUTTON_1, WPAD_BUTTON_2,
    WPAD_BUTTON_PLUS, WPAD_BUTTON_MINUS,
    WPAD_BUTTON_HOME
};

// BEGIN noise-prng
// White-noise generator for the audio callback: xorshift32 (Marsaglia 13/17/5).
// Only shifts and xors: no libc call, no float, no allocation, bounded cost.
#define NOISE_SEED_FALLBACK 0x2545F491u

// Multiplying by an odd constant is a bijection mod 2^32, so only raw == 0
// maps to 0. xorshift32 must never reach state 0, hence the fallback.
static u32 noise_seed(u32 raw) {
    u32 seed = raw * 0x9E3779B9u;
    if (seed == 0u) {
        return NOISE_SEED_FALLBACK;
    }
    return seed;
}

static u32 xorshift32_next(u32 *state) {
    u32 x = *state;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    *state = x;
    return x;
}

// Returns a sample in -128..127 scaled by vol. The product is computed
// modulo 2^32 (u32), so every vol value wraps the same way as other waves.
static s32 noise_sample(u32 *state, u32 vol) {
    s32 centered = (s32)(xorshift32_next(state) >> 24) - 128;
    return (s32)((u32)centered * vol);
}
// END noise-prng

// Written once in main() before ASND_SetVoice, then only by audio_callback
// (single voice, like phase_accumulator).
static u32 noise_state = NOISE_SEED_FALLBACK;

// Runtime settings. Read and written by the main loop only. audio_callback never
// sees this struct: it consumes the bounded values already derived from it
// (g_phase_step, g_vol_scale, g_current_wave), each a single aligned 32-bit store.
static SynthSettings g_settings;

void init_sine_table(void) {
    for (int i = 0; i < SINE_LUT_SIZE; i++) {
        float angle = (2.0f * PI * i) / SINE_LUT_SIZE;
        sine_table[i] = (s16)(32000.0f * sinf(angle));
    }
    DCFlushRange(sine_table, sizeof(sine_table));
}

void audio_callback(int voice) {
    s16 *buf = audio_buffers[current_buffer];
    u32 step = g_phase_step;
    u32 vol = g_vol_scale;
    u32 samples = g_buffer_samples;
    WaveType wave = g_current_wave;

    for (u32 i = 0; i < samples; i++) {
        u32 lut_index = (phase_accumulator >> 16) & SINE_LUT_MASK;
        s32 sample = 0;

        if (wave == WAVE_SINE) {
            sample = (sine_table[lut_index] * vol) >> 8;
        } else if (wave == WAVE_SQUARE) {
            sample = (lut_index < (SINE_LUT_SIZE / 2)) ? (vol * 127) : -(vol * 127);
        } else if (wave == WAVE_SAWTOOTH) {
            sample = (((s32)lut_index - (SINE_LUT_SIZE / 2)) * (s32)vol * 255) / SINE_LUT_SIZE;
        } else if (wave == WAVE_TRIANGLE) {
            s32 half_size = SINE_LUT_SIZE / 2;
            s32 pos = lut_index % half_size;
            s32 val = (pos * (s32)vol * 255) / half_size;
            sample = (lut_index < half_size) ? (val - (vol * 127)) : ((vol * 127) - val);
        } else if (wave == WAVE_NOISE) {
            sample = noise_sample(&noise_state, vol);
        } else if (wave == WAVE_PULSE) {
            // Pulse wave with 25% duty cycle (SINE_LUT_SIZE / 4)
            sample = (lut_index < (SINE_LUT_SIZE / 4)) ? (vol * 127) : -(vol * 127);
        } else if (wave == WAVE_PULSE_NARROW) {
            // 12.5% duty cycle (SINE_LUT_SIZE / 8)
            sample = (lut_index < (SINE_LUT_SIZE / 8)) ? (vol * 127) : -(vol * 127);
        }       

        buf[i] = (s16)sample;
        phase_accumulator += step;
    }

    DCFlushRange(buf, samples * sizeof(s16));
    ASND_AddVoice(voice, buf, samples * sizeof(s16));
    current_buffer ^= 1;
}

// --- FEATURE: VISUAL OSCILLOSCOPE (ZOOMED IN) ---
void draw_oscilloscope(short* audio_buffer, u32 buffer_size) {
    // Zoom in by drawing only 1/4th of the buffer
    u32 display_samples = buffer_size / 4; 
    
    GX_Begin(GX_LINESTRIP, GX_VTXFMT0, display_samples);
    for(u32 i = 0; i < display_samples; i++) {
        float x = (float)i / (float)display_samples * 640.0f;
        float y = 240.0f - ((float)audio_buffer[i] / 32768.0f * 150.0f); 
        GX_Position3f32(x, y, 0.0f);
        GX_Color4u8(0, 255, 255, 255); // Cyan waveform
    }
    GX_End();
}

// Presets: the SD card is mounted on first use, never in the audio callback.
static bool g_fat_ok = false;

static bool sd_mount(void) {
    if (!g_fat_ok) g_fat_ok = fatInitDefault();
    return g_fat_ok;
}

// Applies the buffer size from g_settings. A change restarts the voice, which
// causes a short silence, so this runs from the main loop only, never from
// audio_callback. Called after settings change and when the menu closes.
static void apply_audio_buffer(void) {
    SynthSettings safe = g_settings;
    u32 wanted;

    settings_sanitize(&safe);
    wanted = (u32)safe.buffer_samples;
    if (wanted == g_buffer_samples) return;

    ASND_StopVoice(0);
    g_buffer_samples = wanted;
    memset(audio_buffers, 0, sizeof(audio_buffers));
    DCFlushRange(audio_buffers, sizeof(audio_buffers));
    current_buffer = 0;
    ASND_SetVoice(0, VOICE_MONO_16BIT, SAMPLERATE, 0, audio_buffers[0], g_buffer_samples * sizeof(s16),
                  255, 255, audio_callback);
}

#define STATUS_FRAMES 240 // about 4 seconds at 60 Hz

int main(int argc, char **argv) {
    noise_state = noise_seed((u32)time(NULL));

    init_sine_table();
    memset(audio_buffers, 0, sizeof(audio_buffers));
    DCFlushRange(audio_buffers, sizeof(audio_buffers));

    g_phase_step = (u32)(((440.0f * SINE_LUT_SIZE) / SAMPLERATE) * 65536.0f);
    g_vol_scale = 0; // Silent until the main loop derives a volume from the Wiimote

    VIDEO_Init();
    GXRModeObj *rmode = VIDEO_GetPreferredMode(NULL);
    void *frameBuffer = MEM_K0_TO_K1(SYS_AllocateFramebuffer(rmode));
    VIDEO_Configure(rmode);
    VIDEO_SetNextFramebuffer(frameBuffer);
    VIDEO_SetBlack(FALSE);
    VIDEO_Flush();
    VIDEO_WaitVSync();
    if(rmode->viTVMode & VI_NON_INTERLACE) VIDEO_WaitVSync();

    // Initialize the text console directly over the framebuffer
    console_init(frameBuffer, 0, 0, rmode->fbWidth, rmode->xfbHeight, rmode->fbWidth * VI_DISPLAY_PIX_SZ);

    void *gp_fifo = memalign(32, 256 * 1024);
    memset(gp_fifo, 0, 256 * 1024);
    GX_Init(gp_fifo, 256 * 1024);

    GXColor background = {0, 0, 0, 255}; // Explicitly define Black
    GX_SetCopyClear(background, 0x00ffffff);
    GX_SetViewport(0, 0, rmode->fbWidth, rmode->efbHeight, 0, 1);
    GX_SetDispCopyYScale((f32)rmode->xfbHeight / (f32)rmode->efbHeight);
    GX_SetScissor(0, 0, rmode->fbWidth, rmode->efbHeight);
    GX_SetDispCopySrc(0, 0, rmode->fbWidth, rmode->efbHeight);
    GX_SetDispCopyDst(rmode->fbWidth, rmode->xfbHeight);
    GX_SetCopyFilter(rmode->aa, rmode->sample_pattern, GX_TRUE, rmode->vfilter);
    GX_SetFieldMode(rmode->field_rendering, ((rmode->viHeight==2*rmode->xfbHeight)?GX_TRUE:GX_FALSE));
    GX_SetPixelFmt(GX_PF_RGB8_Z24, GX_ZC_LINEAR);
    GX_SetCullMode(GX_CULL_NONE);
    GX_SetDispCopyGamma(GX_GM_1_0);

    Mtx44 ortho;
    guOrtho(ortho, 0, 480, 0, 640, 0, 300);
    GX_LoadProjectionMtx(ortho, GX_ORTHOGRAPHIC);

    GX_ClearVtxDesc();
    GX_SetVtxDesc(GX_VA_POS, GX_DIRECT);
    GX_SetVtxDesc(GX_VA_CLR0, GX_DIRECT);
    GX_SetVtxAttrFmt(GX_VTXFMT0, GX_VA_POS, GX_POS_XYZ, GX_F32, 0);
    GX_SetVtxAttrFmt(GX_VTXFMT0, GX_VA_CLR0, GX_CLR_RGBA, GX_RGBA8, 0);

    GX_SetNumChans(1);
    // Initialize the color channel to pull directly from the vertex
    GX_SetChanCtrl(GX_COLOR0A0, GX_DISABLE, GX_SRC_VTX, GX_SRC_VTX, GX_LIGHTNULL, GX_DF_NONE, GX_AF_NONE);
    
    GX_SetNumTexGens(0);
    GX_SetTevOrder(GX_TEVSTAGE0, GX_TEXCOORDNULL, GX_TEXMAP_NULL, GX_COLOR0A0);
    GX_SetTevOp(GX_TEVSTAGE0, GX_PASSCLR);  

    WPAD_Init();
    WPAD_SetDataFormat(WPAD_CHAN_0, WPAD_FMT_BTNS_ACC);

    ASND_Init();
    ASND_Pause(0);
    ASND_SetVoice(0, VOICE_MONO_16BIT, SAMPLERATE, 0, audio_buffers[0], g_buffer_samples * sizeof(s16), 255, 255, audio_callback);

    bool show_oscilloscope = true; 
    bool show_welcome = true; // New Welcome Screen State
    bool show_settings = false;
    int menu_row = 0;
    settings_defaults(&g_settings);
    int lfo_phase = 0;

    StdioFsCtx sd_ctx = { sd_mount };
    PresetFs sd_fs;
    preset_fs_stdio_init(&sd_fs, &sd_ctx, PRESET_DIR);
    int preset_slot = PRESET_SLOT_MIN;
    char status[48] = "";
    int status_frames = 0;
    {
        // Silent auto-load of slot 1: a missing card or file keeps the defaults.
        SynthSettings loaded;
        if (preset_load(&sd_fs, PRESET_SLOT_MIN, &loaded) == PRESET_OK) g_settings = loaded;
    }
    apply_audio_buffer();

    // Clear the console text before starting
    printf("\x1b[2J"); 

    while(1) {
        WPAD_ScanPads();
        u32 pressed = WPAD_ButtonsDown(0);
        u32 held = WPAD_ButtonsHeld(0);
        WPADData *data = WPAD_Data(0);

        // Recomputed every frame: a pad error or a removed Classic strips its buttons.
        int classic = classic_link_ok(data->err, data->exp.type == WPAD_EXP_CLASSIC);
        pressed = classic_translate_buttons(pressed, classic, &k_classic_masks, &k_core_masks);
        held = classic_translate_buttons(held, classic, &k_classic_masks, &k_core_masks);

        if (pressed & WPAD_BUTTON_HOME) break;

        // --- WELCOME SCREEN LOGIC ---
        if (show_welcome) {
            if (pressed & WPAD_BUTTON_A) {
                show_welcome = false; // Exit welcome screen
                printf("\x1b[2J");    // Clear the text
            } else {
                printf("\x1b[4;1H"); 
                printf("             ========================================\n");
                printf("                       WII MOTION SYNTH              \n");
                printf("             ========================================\n\n");
                printf("      Turn your Wiimote into an interactive audio synthesizer.\n\n");
                printf("      [ HOW TO PLAY ]\n");
                printf("      - Tilt Up/Down (Pitch) to change the Frequency (Note).\n");
                printf("      - Twist Left/Right (Roll) to change the Volume.\n\n");
                printf("      [ CONTROLS ]\n");
                printf("      Hold B    : Snap-to-Scale\n");
                printf("      Hold A    : Square Wave\n");
                printf("      Hold 1    : Sawtooth Wave\n");
                printf("      Hold 2    : Octave Shift\n");
                printf("      D-Pad UP  : Triangle Wave\n");
                printf("      D-Pad DWN : White Noise\n");
                printf("      D-Pad LFT : Pulse Wave\n");
                printf("      D-Pad RGT : Narrow Pulse\n");
                printf("      Hold PLUS : Mute Gate\n");
                printf("      MINUS     : Toggle Oscilloscope / Stats UI\n");
                printf("      Nunchuk   : Joystick LFO (Vibrato & Tremolo)\n");
                printf("      + then -  : Settings (sensitivity, pitch range)\n\n");
                printf("                    >> PRESS 'A' TO START <<\n");

                g_vol_scale = 0; // Keep the synth completely silent 
                VIDEO_WaitVSync();
                continue; // Skip the rest of the audio/render loop
            }
        }

        // --- SETTINGS MENU (hold + and press - to open or close) ---
        // + and - are used instead of 1 and 2 because holding 1 (sawtooth) with 2 (octave)
        // is an existing playing combination.
        if ((held & WPAD_BUTTON_PLUS) && (held & WPAD_BUTTON_MINUS) &&
            (pressed & (WPAD_BUTTON_PLUS | WPAD_BUTTON_MINUS))) {
            show_settings = !show_settings;
            if (!show_settings) apply_audio_buffer();
            pressed &= ~WPAD_BUTTON_MINUS; // Do not also toggle the oscilloscope
            printf("\x1b[2J");
        }
        if (show_settings) {
            g_vol_scale = 0; // Silent while editing
            if (pressed & WPAD_BUTTON_DOWN) menu_row = (menu_row + 1) % FIELD_COUNT;
            if (pressed & WPAD_BUTTON_UP) menu_row = (menu_row + FIELD_COUNT - 1) % FIELD_COUNT;
            if (menu_row == FIELD_SLOT) {
                if (pressed & WPAD_BUTTON_RIGHT) preset_slot = preset_slot % PRESET_SLOT_MAX + 1;
                if (pressed & WPAD_BUTTON_LEFT) preset_slot = (preset_slot + PRESET_SLOT_MAX - 2) % PRESET_SLOT_MAX + 1;
            } else {
                if (pressed & WPAD_BUTTON_RIGHT) settings_field_adjust(&g_settings, (SettingsField)menu_row, 1);
                if (pressed & WPAD_BUTTON_LEFT) settings_field_adjust(&g_settings, (SettingsField)menu_row, -1);
            }
            if (pressed & WPAD_BUTTON_A) {
                if (menu_row == FIELD_SAVE || menu_row == FIELD_LOAD) {
                    PresetResult pr;
                    if (menu_row == FIELD_SAVE) {
                        pr = preset_save(&sd_fs, preset_slot, &g_settings);
                    } else {
                        SynthSettings loaded;
                        pr = preset_load(&sd_fs, preset_slot, &loaded);
                        if (pr == PRESET_OK) {
                            g_settings = loaded;
                            apply_audio_buffer();
                        }
                    }
                    snprintf(status, sizeof(status), "%s slot %d: %s", (menu_row == FIELD_SAVE) ? "Save" : "Load",
                             preset_slot, preset_strerror(pr));
                    status_frames = STATUS_FRAMES;
                }
                if (menu_row == FIELD_RESET) {
                    settings_field_adjust(&g_settings, FIELD_RESET, 1);
                    apply_audio_buffer();
                }
                if (menu_row == FIELD_BACK) {
                    show_settings = false;
                    apply_audio_buffer();
                    printf("\x1b[2J");
                }
            }
            if (show_settings) {
                printf("\x1b[4;1H");
                printf("             ========================================\n");
                printf("                            SETTINGS                 \n");
                printf("             ========================================\n\n");
                for (int f = 0; f < FIELD_COUNT; f++) {
                    char line[48];
                    if (f == FIELD_SLOT) snprintf(line, sizeof(line), "Preset slot: %d", preset_slot);
                    else settings_field_text(&g_settings, (SettingsField)f, line, sizeof(line));
                    printf("      %c %-36s\n", (f == menu_row) ? '>' : ' ', line);
                }
                printf("\n      UP/DOWN    : Select\n");
                printf("      LEFT/RIGHT : Change value\n");
                printf("      A          : Confirm Reset / Back\n");
                printf("      + then -   : Close\n\n");
                printf("      Slot 1 is loaded at startup; save to keep settings.\n");
                printf("      %-48s\n", (status_frames > 0) ? status : "");
                if (status_frames > 0) status_frames--;
                VIDEO_WaitVSync();
                continue;
            }
        }

        // --- MAIN APP LOGIC ---
        
        // Display Toggle
        if (pressed & WPAD_BUTTON_MINUS) {
            show_oscilloscope = !show_oscilloscope;
            if (!show_oscilloscope) {
                printf("\x1b[2J"); // Clear the console text when switching to UI mode
            }
        }

        // Waveforms
        if (held & WPAD_BUTTON_UP) g_current_wave = WAVE_TRIANGLE;
        else if (held & WPAD_BUTTON_DOWN) g_current_wave = WAVE_NOISE;
        else if (held & WPAD_BUTTON_LEFT) g_current_wave = WAVE_PULSE;
        else if (held & WPAD_BUTTON_RIGHT) g_current_wave = WAVE_PULSE_NARROW;
        else if (held & WPAD_BUTTON_1) g_current_wave = WAVE_SAWTOOTH;
        else if (held & WPAD_BUTTON_A) g_current_wave = WAVE_SQUARE;
        else g_current_wave = (WaveType)g_settings.wave_default;

        // Raw, unsmoothed accelerometer data
        int target_vol = synth_volume_for_roll(&g_settings, data->orient.roll);
        int lfo_offset_hz = 0;

        // Nunchuk LFO (frequency offset is applied inside synth_target_freq)
        if (data->exp.type == WPAD_EXP_NUNCHUK) {
            int joy_y = (int)data->exp.nunchuk.js.pos.y - (int)data->exp.nunchuk.js.center.y;
            int joy_x = (int)data->exp.nunchuk.js.pos.x - (int)data->exp.nunchuk.js.center.x;
            
            if (abs(joy_y) > 10) lfo_offset_hz = (sine_table[lfo_phase] * joy_y) / 60000;
            if (abs(joy_x) > 10) target_vol -= abs((sine_table[lfo_phase] * joy_x) / 60000);
            
            lfo_phase = (lfo_phase + 170) % SINE_LUT_SIZE;
        }

        // Classic Controller LFO: left stick Y = vibrato, right stick X = tremolo
        if (classic) {
            int ly = classic_stick_norm(data->exp.classic.ljs.pos.y, data->exp.classic.ljs.center.y,
                                        data->exp.classic.ljs.min.y, data->exp.classic.ljs.max.y);
            int rx = classic_stick_norm(data->exp.classic.rjs.pos.x, data->exp.classic.rjs.center.x,
                                        data->exp.classic.rjs.min.x, data->exp.classic.rjs.max.x);

            if (abs(ly) > 10) lfo_offset_hz = (sine_table[lfo_phase] * ly) / 60000;
            if (abs(rx) > 10) target_vol -= abs((sine_table[lfo_phase] * rx) / 60000);

            lfo_phase = (lfo_phase + 170) % SINE_LUT_SIZE;
        }

        // Mute Gate modifier
        if (held & WPAD_BUTTON_PLUS) {
            target_vol = 0;
        }
        if (target_vol < 0) target_vol = 0; // The tremolo subtraction must not wrap g_vol_scale

        int target_freq = synth_target_freq(data->orient.pitch, synth_snap_active(&g_settings, (held & WPAD_BUTTON_B) != 0),
                                            (held & WPAD_BUTTON_2) != 0, lfo_offset_hz, &g_settings);
        g_phase_step = freq_to_phase_step(target_freq);
        int output_vol = synth_scaled_volume(target_vol, &g_settings);
        g_vol_scale = output_vol;

        // --- RENDER LOGIC ---
        if (show_oscilloscope) {
            // Draw hardware graphics
            draw_oscilloscope(audio_buffers[current_buffer ^ 1], g_buffer_samples);
            GX_DrawDone();
            GX_CopyDisp(frameBuffer, GX_TRUE);
        } else {
            // Determine the current wave name for the text UI
            const char* wave_name = "SINE";
            if (g_current_wave == WAVE_SQUARE) wave_name = "SQUARE";
            else if (g_current_wave == WAVE_SAWTOOTH) wave_name = "SAWTOOTH";
            else if (g_current_wave == WAVE_TRIANGLE) wave_name = "TRIANGLE";
            else if (g_current_wave == WAVE_NOISE) wave_name = "NOISE";
            else if (g_current_wave == WAVE_PULSE) wave_name = "PULSE";
            else if (g_current_wave == WAVE_PULSE_NARROW) wave_name = "NARROW";

            // Draw CPU Text UI
            printf("\x1b[1;1H"); 
            printf("--- WII MOTION SYNTH ---\n\n");
            printf("Current Wave: %-8s\n", wave_name);
            printf("Frequency: %4d Hz     \n", target_freq);
            printf("Volume: %3d%%          \n", (output_vol * 100) / 255);
            printf("Range: %4d-%4d Hz  Sens: %3d%%   \n\n", g_settings.pitch_min_hz, g_settings.pitch_max_hz,
                   g_settings.sensitivity_pct);
            
            printf("--- SENSOR TELEMETRY ---\n");
            printf("Pitch (Up/Down) : %6.1f\n", data->orient.pitch);
            printf("Roll  (Twist)   : %6.1f\n", data->orient.roll);
            printf("Yaw   (Rotation): %6.1f\n\n", data->orient.yaw);
            
            printf("--- CONTROLS ---\n");
            printf("Hold B    : Snap-to-Scale\n");
            printf("Hold A    : Square Wave\n");
            printf("Hold 1    : Sawtooth Wave\n");
            printf("Hold 2    : Octave Shift\n");
            printf("D-Pad UP  : Triangle Wave\n");
            printf("D-Pad DWN : White Noise\n");
            printf("D-Pad LFT : Pulse Wave\n");
            printf("D-Pad RGT : Narrow Pulse\n");
            printf("Hold PLUS : Mute Gate\n");
            printf("+ then -  : Settings\n");
            printf("MINUS     : Toggle Visualizer\n");
            printf("HOME      : Exit\n");
        }

        VIDEO_WaitVSync();
    }

    ASND_StopVoice(0);
    WPAD_Shutdown();
    return 0;
}