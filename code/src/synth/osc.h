/** @file osc.h
 *  @brief Band-limited analog-style oscillator: saw, pulse (with PWM) and
 *  triangle, with hard sync.
 *
 *  Discontinuities are smoothed with a polyBLEP residual spread over this
 *  sample and the next, the same scheme Mutable Instruments' Plaits uses. The
 *  oscillator therefore runs one sample late, which keeps sync exact: a reset
 *  part way through a sample is corrected on both sides of the jump.
 */
#pragma once
#include "dsp.h"

namespace synth
{

enum class Wave : uint8_t
{
    SAW,
    PULSE,
    TRI,
    COUNT,
};

class Osc
{
  public:
    void Reset(float phase = 0.f)
    {
        phase_ = phase;
        next_  = 0.f;
        high_  = phase_ < pw_;
    }

    /** One sample.
     *  @param inc     frequency / sample rate, 0..0.45
     *  @param pw      pulse width 0..1 (only used by PULSE)
     *  @param sync_t  >= 0 if the master wrapped this sample: how far through the
     *                 sample (0..1, measured back from its end) the wrap happened.
     *                 Pass -1 when there is no sync.
     *  @param wrap_t  set to how far back from the end of the sample this
     *                 oscillator wrapped, or -1. Feed it to slaves as sync_t.
     */
    float Process(float inc, float pw, float sync_t, float* wrap_t)
    {
        inc = Clamp(inc, 0.f, 0.45f);
        pw  = Clamp(pw, 0.05f, 0.95f);
        pw_ = pw;

        float this_sample = next_;
        float next_sample = 0.f;
        *wrap_t           = -1.f;

        if(sync_t >= 0.f)
        {
            // Run up to the reset point, then jump to phase 0.
            float at_reset = phase_ + inc * (1.f - sync_t);
            at_reset -= floorf(at_reset);
            const float jump = Value(0.f) - Value(at_reset);
            this_sample += jump * ThisBlep(sync_t);
            next_sample += jump * NextBlep(sync_t);
            phase_ = inc * sync_t;
            high_  = phase_ < pw_;
        }
        else
        {
            phase_ += inc;

            if(wave_ == Wave::PULSE && high_ && phase_ >= pw_ && phase_ < 1.f)
            {
                const float t = (phase_ - pw_) / inc;
                this_sample -= 2.f * ThisBlep(t);
                next_sample -= 2.f * NextBlep(t);
                high_ = false;
            }

            if(phase_ >= 1.f)
            {
                phase_ -= 1.f;
                const float t = phase_ / inc;
                *wrap_t       = t;

                if(wave_ == Wave::SAW)
                {
                    this_sample -= 2.f * ThisBlep(t);
                    next_sample -= 2.f * NextBlep(t);
                }
                else if(wave_ == Wave::PULSE)
                {
                    // A pulse that wrapped without passing pw this sample fell
                    // and rose inside one sample: both edges cancel.
                    if(!high_)
                    {
                        this_sample += 2.f * ThisBlep(t);
                        next_sample += 2.f * NextBlep(t);
                    }
                    high_ = true;
                    if(phase_ >= pw_)
                    {
                        const float t2 = (phase_ - pw_) / inc;
                        this_sample -= 2.f * ThisBlep(t2);
                        next_sample -= 2.f * NextBlep(t2);
                        high_ = false;
                    }
                }
            }
        }

        next_sample += Value(phase_);
        next_ = next_sample;
        return this_sample;
    }

    void SetWave(Wave w)
    {
        if(w != wave_)
        {
            wave_ = w;
            high_ = phase_ < pw_;
        }
    }

  private:
    static inline float ThisBlep(float t) { return 0.5f * t * t; }
    static inline float NextBlep(float t)
    {
        t = 1.f - t;
        return -0.5f * t * t;
    }

    inline float Value(float p) const
    {
        switch(wave_)
        {
            case Wave::SAW: return 2.f * p - 1.f;
            case Wave::PULSE: return p < pw_ ? 1.f : -1.f;
            default: return p < 0.5f ? 4.f * p - 1.f : 3.f - 4.f * p;
        }
    }

    float phase_ = 0.f;
    float next_  = 0.f;
    float pw_    = 0.5f;
    bool  high_  = true;
    Wave  wave_  = Wave::SAW;
};

} // namespace synth
