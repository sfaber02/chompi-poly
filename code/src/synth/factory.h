/** @file factory.h
 *  @brief The factory patches, built into the firmware.
 *
 *  A factory slot whose file (/POLY/Pnn.txt) is missing loads its built-in
 *  copy, so a bare .bin with no POLY folder still has every patch, and slot
 *  15 (init) always works. A patch you save to a slot writes the file, which
 *  then wins. host/render writes these same patches out as the files in card/POLY.
 *
 *  Each patch is the parameter defaults plus these overrides.
 */
#pragma once
#include "params.h"

namespace synth
{

struct ParamValue
{
    uint8_t id;
    float   v;
};

struct FactoryPatch
{
    const char*       name;
    bool              mono; // how the demo render plays it; the toggle decides on hardware
    uint8_t           slot; // 1-15
    const ParamValue* values;
    uint8_t           count;
};

constexpr ParamValue kSyncLead[] = {
    {OSC1_LEVEL, .25f}, {OSC2_LEVEL, 1.f}, {OSC2_DETUNE, .5f}, {SYNC, 1.f}, {SWEEP, .55f},
    {CUTOFF, .72f}, {RESONANCE, .25f}, {FENV_AMT, .62f}, {FENV_D, .55f}, {FENV_S, .15f},
    {GLIDE, .25f}, {LFO_PITCH, .12f}, {LFO_RATE, .5f}, {DLY_MIX, 0.12f}, {DLY_TIME, .5f},
    {REV_MIX, 0.059f},
};

constexpr ParamValue kBrass[] = {
    {OSC2_LEVEL, .85f}, {OSC2_DETUNE, .54f}, {OSC3_WAVE, 0.f}, {OSC3_LEVEL, .5f},
    {CUTOFF, .42f}, {RESONANCE, .12f}, {FENV_AMT, .76f}, {FENV_A, .33f}, {FENV_D, .5f},
    {FENV_S, .45f}, {FENV_R, .4f}, {AENV_A, .2f}, {AENV_R, .4f}, {CHORUS, .45f},
    {REV_MIX, 0.106f},
};

constexpr ParamValue kUnisonBass[] = {
    {OSC2_LEVEL, .8f}, {OSC3_LEVEL, .6f}, {OSC3_OCT, 0.f}, {UNISON, .45f}, {CUTOFF, .33f},
    {RESONANCE, .35f}, {FENV_AMT, .72f}, {FENV_D, .35f}, {FENV_S, .1f}, {AENV_R, .2f},
    {DRIVE, .55f}, {SATURATE, .45f}, {REV_MIX, 0.f},
};

constexpr ParamValue kPwmStrings[] = {
    {OSC1_WAVE, .5f}, {OSC2_WAVE, .5f}, {OSC2_LEVEL, .7f}, {OSC2_DETUNE, .56f}, {LFO_PWM, .7f},
    {LFO_RATE, .32f}, {CUTOFF, .6f}, {FENV_AMT, .55f}, {AENV_A, .55f}, {AENV_R, .6f},
    {AENV_S, .9f}, {CHORUS, .85f}, {REV_MIX, 0.23f}, {REV_SIZE, .7f},
};

constexpr ParamValue kXmodBell[] = {
    {OSC1_LEVEL, .15f}, {OSC2_WAVE, 1.f}, {OSC2_OCT, 1.f}, {OSC2_LEVEL, 1.f},
    {OSC2_DETUNE, .5f}, {XMOD, .55f}, {CUTOFF, .85f}, {FENV_AMT, .5f}, {AENV_A, 0.f},
    {AENV_D, .68f}, {AENV_S, 0.f}, {AENV_R, .62f}, {REV_MIX, 0.194f}, {DLY_MIX, 0.08f},
};

constexpr ParamValue kArpPluck[] = {
    {OSC2_LEVEL, .6f}, {OSC2_OCT, 2.f / 3.f}, {CUTOFF, .38f}, {RESONANCE, .45f},
    {FENV_AMT, .8f}, {FENV_D, .3f}, {FENV_S, 0.f}, {AENV_D, .32f}, {AENV_S, 0.f}, {AENV_R, .3f},
    {ARP_MODE, .6f}, {ARP_RANGE, .5f}, {ARP_TEMPO, .4f}, {ARP_GATE, .4f}, {DLY_MIX, 0.16f},
    {DLY_FDBK, .45f}, {DLY_TIME, .55f}, {CHORUS, .3f},
};

constexpr ParamValue kAcid[] = {
    {OSC1_WAVE, 0.f}, {OSC2_LEVEL, 0.f}, {CUTOFF, .3f}, {RESONANCE, .82f}, {FENV_AMT, .78f},
    {FENV_D, .3f}, {FENV_S, 0.f}, {AENV_S, .9f}, {AENV_R, .1f}, {GLIDE, .2f}, {DRIVE, .6f},
    {SATURATE, .5f}, {VEL_FILTER, .6f}, {DLY_MIX, 0.08f}, {REV_MIX, 0.f},
};

constexpr ParamValue kInit[] = {
    {OSC1_WAVE, 1.f}, {OSC1_OCT, 1.f / 3.f}, {OSC1_LEVEL, .8f}, {OSC1_DETUNE, .5f},
    {OSC1_PW, .5f}, {OSC2_LEVEL, 0.f}, {OSC2_DETUNE, .5f}, {OSC3_LEVEL, 0.f},
    {OSC3_DETUNE, .5f}, {OSC4_LEVEL, 0.f}, {OSC4_DETUNE, .5f}, {SYNC, 0.f}, {XMOD, 0.f},
    {SWEEP, 0.f}, {GLIDE, 0.f}, {UNISON, 0.f}, {CUTOFF, .5f}, {RESONANCE, 0.f}, {FENV_AMT, .5f},
    {KEYTRACK, 1.f}, {DRIVE, 0.f}, {VEL_FILTER, 0.f}, {AENV_A, 0.f}, {AENV_D, .5f},
    {AENV_S, 1.f}, {AENV_R, .15f}, {VEL_AMP, 0.f}, {LFO_PITCH, 0.f}, {LFO_PWM, 0.f},
    {LFO_CUTOFF, 0.f}, {CHORUS, 0.f}, {DLY_MIX, 0.f}, {REV_MIX, 0.f}, {SATURATE, 0.f},
    {NOISE, 0.f}, {SPREAD, .5f}, {ARP_MODE, 0.f},
};

// Init (slot 15): as close to one sine wave as this synth gets. One
// triangle, the filter tracking the keyboard at twice the note's pitch so its
// overtones are shaved off (3rd harmonic ~36 dB down). Everything else off;
// organ envelope. A clean place to start building a sound.

#define POLY_FACTORY(name, mono, slot, arr) {name, mono, slot, arr, sizeof(arr) / sizeof(arr[0])}
constexpr FactoryPatch kFactoryPatches[] = {
    POLY_FACTORY("sync_lead", true, 1, kSyncLead),
    POLY_FACTORY("brass", false, 2, kBrass),
    POLY_FACTORY("unison_bass", true, 3, kUnisonBass),
    POLY_FACTORY("pwm_strings", false, 4, kPwmStrings),
    POLY_FACTORY("xmod_bell", false, 5, kXmodBell),
    POLY_FACTORY("arp_pluck", false, 6, kArpPluck),
    POLY_FACTORY("acid", true, 7, kAcid),
    POLY_FACTORY("init", false, 15, kInit),
};
#undef POLY_FACTORY
constexpr int kNumFactoryPatches = sizeof(kFactoryPatches) / sizeof(kFactoryPatches[0]);

/** The sound a brand-new card starts on (no saved sound yet): brass, the
 *  classic Mono/Poly poly brass, which works with the switch either way. */
constexpr int kFirstBootSlot = 2;

/** Built-in patch for a slot (1-15), or nullptr. */
inline const FactoryPatch* FactoryForSlot(int slot)
{
    for(int i = 0; i < kNumFactoryPatches; i++)
        if(kFactoryPatches[i].slot == slot)
            return &kFactoryPatches[i];
    return nullptr;
}

/** Writes the whole patch (defaults + overrides) into params. Global
 *  (instrument) settings are left as they are. */
inline void ApplyFactory(const FactoryPatch& f, float* params)
{
    for(int i = 0; i < NUM_PARAMS; i++)
        if(!kParams[i].global)
            params[i] = kParams[i].def;
    for(int i = 0; i < f.count; i++)
        params[f.values[i].id] = f.values[i].v;
}

} // namespace synth
