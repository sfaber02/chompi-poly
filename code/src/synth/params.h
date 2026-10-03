/** @file params.h
 *  @brief Every sound parameter, its default, its knob page, and its MIDI CC.
 *
 *  All values are stored normalised 0..1 in one float array, which is what the
 *  knobs move, what presets save, and what MIDI CCs set. The engine turns them
 *  into real units once per audio block. Stepped parameters (waveform, octave,
 *  ...) use `steps` positions spread evenly over 0..1.
 */
#pragma once
#include <cstdint>

namespace synth
{

enum Param : uint8_t
{
    // Four oscillators, five parameters each. Keep this block contiguous:
    // OSC1_WAVE + osc * kOscParams + field addresses any of them.
    OSC1_WAVE,
    OSC1_OCT,
    OSC1_LEVEL,
    OSC1_DETUNE,
    OSC1_PW,
    OSC2_WAVE,
    OSC2_OCT,
    OSC2_LEVEL,
    OSC2_DETUNE,
    OSC2_PW,
    OSC3_WAVE,
    OSC3_OCT,
    OSC3_LEVEL,
    OSC3_DETUNE,
    OSC3_PW,
    OSC4_WAVE,
    OSC4_OCT,
    OSC4_LEVEL,
    OSC4_DETUNE,
    OSC4_PW,

    SYNC,
    XMOD,
    SWEEP,
    GLIDE,
    UNISON,

    CUTOFF,
    RESONANCE,
    FENV_AMT,
    KEYTRACK,
    DRIVE,

    FENV_A,
    FENV_D,
    FENV_S,
    FENV_R,
    VEL_FILTER,

    AENV_A,
    AENV_D,
    AENV_S,
    AENV_R,
    VEL_AMP,

    LFO_RATE,
    LFO_SHAPE,
    LFO_PITCH,
    LFO_PWM,
    LFO_CUTOFF,

    CHORUS,
    DLY_TIME,
    DLY_FDBK,
    DLY_MIX,
    SPREAD,

    REV_SIZE,
    REV_MIX,
    REV_TONE,
    SATURATE,
    NOISE,

    ARP_MODE,
    ARP_RANGE,
    ARP_TEMPO,
    ARP_GATE,
    TUNE,

    NUM_PARAMS
};

constexpr int kNumOscs   = 4;
constexpr int kOscParams = OSC2_WAVE - OSC1_WAVE;

struct ParamInfo
{
    const char* name;    // key in preset files, keep stable
    float       def;     // normalised default
    uint8_t     steps;   // 0 = continuous, else number of positions
    bool        bipolar; // centre = zero; drives the key-LED bar display
    uint8_t     cc;      // MIDI CC in, 0 = none
};

// clang-format off
constexpr ParamInfo kParams[NUM_PARAMS] = {
    {"osc1_wave",   0.f,   3, false, 0},  // saw
    {"osc1_oct",    1/3.f, 4, false, 0},  // 8'
    {"osc1_level",  .8f,   0, false, 0},
    {"osc1_detune", .5f,   0, true,  0},
    {"osc1_pw",     .5f,   0, false, 0},
    {"osc2_wave",   0.f,   3, false, 0},
    {"osc2_oct",    1/3.f, 4, false, 0},
    {"osc2_level",  .8f,   0, false, 0},
    {"osc2_detune", .53f,  0, true,  0},
    {"osc2_pw",     .5f,   0, false, 0},
    {"osc3_wave",   .5f,   3, false, 0},  // pulse
    {"osc3_oct",    0.f,   4, false, 0},  // 16'
    {"osc3_level",  0.f,   0, false, 0},
    {"osc3_detune", .47f,  0, true,  0},
    {"osc3_pw",     .5f,   0, false, 0},
    {"osc4_wave",   1.f,   3, false, 0},  // tri
    {"osc4_oct",    2/3.f, 4, false, 0},  // 4'
    {"osc4_level",  0.f,   0, false, 0},
    {"osc4_detune", .5f,   0, true,  0},
    {"osc4_pw",     .5f,   0, false, 0},

    {"sync",        0.f,   2, false, 102},
    {"xmod",        0.f,   0, false, 103},
    {"sweep",       0.f,   0, false, 104},
    {"glide",       0.f,   0, false, 5},
    {"unison",      0.f,   0, false, 105},

    {"cutoff",      .55f,  0, false, 74},
    {"resonance",   .2f,   0, false, 71},
    {"fenv_amt",    .7f,   0, true,  106},
    {"keytrack",    .5f,   0, false, 107},
    {"drive",       .2f,   0, false, 108},

    {"fenv_a",      .05f,  0, false, 109},
    {"fenv_d",      .45f,  0, false, 110},
    {"fenv_s",      .3f,   0, false, 111},
    {"fenv_r",      .4f,   0, false, 112},
    {"vel_filter",  .3f,   0, false, 113},

    {"aenv_a",      .02f,  0, false, 73},
    {"aenv_d",      .5f,   0, false, 75},
    {"aenv_s",      .8f,   0, false, 76},
    {"aenv_r",      .35f,  0, false, 72},
    {"vel_amp",     .4f,   0, false, 114},

    {"lfo_rate",    .45f,  0, false, 77},
    {"lfo_shape",   0.f,   5, false, 78},
    {"lfo_pitch",   0.f,   0, false, 79},
    {"lfo_pwm",     0.f,   0, false, 80},
    {"lfo_cutoff",  0.f,   0, false, 81},

    {"chorus",      0.f,   0, false, 93},
    {"dly_time",    .45f,  0, false, 82},
    {"dly_fdbk",    .35f,  0, false, 83},
    {"dly_mix",     0.f,   0, false, 84},
    {"spread",      .5f,   0, false, 10},

    {"rev_size",    .5f,   0, false, 85},
    {"rev_mix",     .06f,  0, false, 91},
    {"rev_tone",    .6f,   0, false, 86},
    {"saturate",    .2f,   0, false, 87},
    {"noise",       0.f,   0, false, 88},

    {"arp_mode",    0.f,   6, false, 89},
    {"arp_range",   0.f,   3, false, 90},
    {"arp_tempo",   .4f,   0, false, 115},
    {"arp_gate",    .9f,   0, false, 116},
    {"tune",        .5f,   0, true,  117},
};
// clang-format on

/** Index of a stepped parameter's position. */
inline int StepIndex(float v, int steps)
{
    int i = static_cast<int>(v * (steps - 1) + 0.5f);
    return i < 0 ? 0 : (i >= steps ? steps - 1 : i);
}

inline float StepValue(int idx, int steps)
{
    return steps > 1 ? static_cast<float>(idx) / (steps - 1) : 0.f;
}

// ---------------------------------------------------------------------------
// Knob pages. CHOMPI + a black key picks one. Knobs 1-4 edit its four main
// parameters; CHOMPI + turn edits the second layer. On the OSC page, clicking
// knob 1-4 picks which oscillator the knobs edit.

enum Page : uint8_t
{
    PAGE_OSC,
    PAGE_MOD,
    PAGE_FILTER,
    PAGE_FENV,
    PAGE_AENV,
    PAGE_FX,
    PAGE_REVERB,
    PAGE_LFO,
    PAGE_ARP,
    PAGE_PERFORM,
    NUM_PAGES
};

constexpr int     kPageKnobs = 4;
constexpr uint8_t kNone      = 0xFF;

// [page][0..3] = knobs 1-4, [page][4..7] = CHOMPI + knobs 1-4.
// For PAGE_OSC the per-oscillator ids are oscillator 1's; add osc * kOscParams.
constexpr uint8_t kPageParams[NUM_PAGES][2 * kPageKnobs] = {
    {OSC1_WAVE, OSC1_OCT, OSC1_LEVEL, OSC1_DETUNE, OSC1_PW, kNone, NOISE, kNone},
    {SYNC, XMOD, SWEEP, GLIDE, kNone, kNone, kNone, UNISON},
    {CUTOFF, RESONANCE, FENV_AMT, KEYTRACK, kNone, DRIVE, kNone, kNone},
    {FENV_A, FENV_D, FENV_S, FENV_R, VEL_FILTER, kNone, kNone, kNone},
    {AENV_A, AENV_D, AENV_S, AENV_R, VEL_AMP, kNone, kNone, kNone},
    // Mix is knob 1 on both effect pages.
    {DLY_MIX, DLY_TIME, DLY_FDBK, CHORUS, kNone, kNone, kNone, SPREAD},
    {REV_MIX, REV_SIZE, REV_TONE, SATURATE, kNone, kNone, kNone, kNone},
    {LFO_RATE, LFO_SHAPE, LFO_PITCH, LFO_CUTOFF, kNone, kNone, LFO_PWM, kNone},
    {ARP_MODE, ARP_RANGE, ARP_TEMPO, ARP_GATE, kNone, kNone, kNone, kNone},
    {GLIDE, UNISON, SPREAD, TUNE, NOISE, SATURATE, DRIVE, kNone},
};

inline bool IsOscParam(uint8_t id) { return id < OSC2_WAVE; }

// Page colours (RGB 0..1): the knob LEDs and the page's black key show these.
constexpr float kPageColour[NUM_PAGES][3] = {
    {1.f, .45f, 0.f},  // OSC      orange
    {1.f, 0.f, .25f},  // MOD      hot pink
    {0.f, .6f, 1.f},   // FILTER   blue
    {.2f, .3f, 1.f},   // F-ENV    indigo
    {0.f, 1.f, .3f},   // A-ENV    green
    {0.f, 1.f, .85f},  // FX       teal
    {1.f, .9f, .6f},   // REVERB   warm white
    {.6f, 0.f, 1.f},   // LFO      purple
    {1.f, .85f, 0.f},  // ARP      yellow
    {1.f, 1.f, 1.f},   // PERFORM  white
};

} // namespace synth
