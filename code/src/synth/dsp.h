/** @file dsp.h
 *  @brief Small math helpers shared by the synth DSP. Pure C++, no libDaisy, so
 *  the whole engine also builds on the desktop (see host/).
 */
#pragma once
#include <cmath>
#include <cstddef>
#include <cstdint>

namespace synth
{

using std::size_t;

constexpr float kPi = 3.14159265358979f;

inline float Clamp(float x, float lo, float hi)
{
    return x < lo ? lo : (x > hi ? hi : x);
}

/** 2^x, accurate to ~0.01 % over the range we use. Cheap enough to run per sample. */
inline float FastExp2(float x)
{
    x = Clamp(x, -30.f, 30.f);
    const float fl = floorf(x);
    const float f  = x - fl;
    // 4th-order minimax fit of 2^f on [0,1)
    const float p = 1.f + f * (0.6931472f + f * (0.2402265f + f * (0.0555041f + f * 0.0096181f)));
    return ldexpf(p, static_cast<int>(fl));
}

/** Pade tanh, clamped where it would turn back. */
inline float FastTanh(float x)
{
    if(x > 3.f)
        return 1.f;
    if(x < -3.f)
        return -1.f;
    const float x2 = x * x;
    return x * (27.f + x2) / (27.f + 9.f * x2);
}

inline float MidiToHz(float note)
{
    return 440.f * FastExp2((note - 69.f) * (1.f / 12.f));
}

/** Knob 0..1 to a time in seconds, exponential between lo and hi. */
inline float KnobToTime(float v, float lo, float hi)
{
    return lo * powf(hi / lo, v);
}

/** Per-sample coefficient for a one-pole that settles in about `seconds`. */
inline float TimeToCoef(float seconds, float sr)
{
    if(seconds <= 0.f)
        return 1.f;
    return 1.f - expf(-1.f / (seconds * sr));
}

/** xorshift32 white noise in [-1, 1). */
struct Noise
{
    uint32_t s = 0x9E3779B9u;
    inline float Process()
    {
        s ^= s << 13;
        s ^= s >> 17;
        s ^= s << 5;
        return static_cast<int32_t>(s) * (1.f / 2147483648.f);
    }
};

} // namespace synth
