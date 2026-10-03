/** @file lfo.h
 *  @brief Control-rate LFO (the Mono/Poly calls it the MG). Updated once per
 *  audio block, which at 2 kHz is plenty for 0.05 - 30 Hz.
 */
#pragma once
#include "dsp.h"

namespace synth
{

enum class LfoShape : uint8_t
{
    TRI,
    SINE,
    SAW_DOWN,
    SQUARE,
    SAMPLE_HOLD,
    COUNT,
};

class Lfo
{
  public:
    /** @param rate_hz LFO frequency
     *  @param dt      seconds since the last call (one audio block)
     *  @return        -1..1 */
    float Process(float rate_hz, float dt)
    {
        phase_ += rate_hz * dt;
        if(phase_ >= 1.f)
        {
            phase_ -= floorf(phase_);
            held_ = noise_.Process();
        }

        const float p = phase_;
        switch(shape_)
        {
            case LfoShape::TRI: return p < 0.5f ? 4.f * p - 1.f : 3.f - 4.f * p;
            case LfoShape::SINE: return sinf(2.f * kPi * p);
            case LfoShape::SAW_DOWN: return 1.f - 2.f * p;
            case LfoShape::SQUARE: return p < 0.5f ? 1.f : -1.f;
            default: return held_;
        }
    }

    void SetShape(LfoShape s) { shape_ = s; }
    void Reset() { phase_ = 0.f; }

  private:
    float    phase_ = 0.f;
    float    held_  = 0.f;
    LfoShape shape_ = LfoShape::TRI;
    Noise    noise_;
};

} // namespace synth
