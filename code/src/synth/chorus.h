/** @file chorus.h
 *  @brief Ensemble chorus: three taps on one short delay, each swept by the
 *  same triangle a third of a cycle apart (the string-machine trick), spread
 *  left / centre / right. Three voices drifting against the dry signal give
 *  real pitch shimmer, so it thickens in mono too, not only in stereo.
 *
 *  One knob: more amount = deeper sweep, faster shimmer, more wet.
 */
#pragma once
#include "dsp.h"

namespace synth
{

class Chorus
{
  public:
    static constexpr size_t kSize = 2048; // 42 ms at 48 kHz, power of two

    void Init(float sample_rate)
    {
        sr_ = sample_rate;
        for(size_t i = 0; i < kSize; i++)
            line_[i] = 0.f;
        write_ = 0;
        phase_ = 0.f;
    }

    void SetAmount(float a) { amount_ = Clamp(a, 0.f, 1.f); }

    /** Stereo in place. The delay line is fed the mono sum. */
    inline void Process(float* l, float* r)
    {
        line_[write_] = 0.5f * (*l + *r);
        write_        = (write_ + 1) & (kSize - 1);

        if(amount_ <= 0.f)
            return;

        // 0.6 Hz at a touch up to 1.6 Hz flat out.
        phase_ += (0.6f + 1.0f * amount_) / sr_;
        if(phase_ >= 1.f)
            phase_ -= 1.f;

        // Depth 2 ms to 7 ms around a 12 ms centre: enough to hear
        // straight away, never seasick.
        const float centre = 0.012f * sr_;
        const float depth  = (0.002f + 0.005f * amount_) * sr_;
        const float t0     = Read(centre + depth * Tri(phase_));
        const float t1     = Read(centre + depth * Tri(phase_ + 1.f / 3.f));
        const float t2     = Read(centre + depth * Tri(phase_ + 2.f / 3.f));

        const float wl = t0 + 0.5f * t1;
        const float wr = t2 + 0.5f * t1;

        // Like a Juno it's strong from the first click: wet starts near
        // half and rises to match the dry.
        const float wet = 0.55f + 0.35f * amount_;
        const float dry = 1.f - 0.35f * wet;
        *l              = *l * dry + wl * wet * 0.67f;
        *r              = *r * dry + wr * wet * 0.67f;
    }

  private:
    static inline float Tri(float p)
    {
        p -= floorf(p);
        return p < 0.5f ? 4.f * p - 1.f : 3.f - 4.f * p;
    }

    inline float Read(float delay_samples) const
    {
        float pos = static_cast<float>(write_) - delay_samples;
        while(pos < 0.f)
            pos += kSize;
        const size_t i0 = static_cast<size_t>(pos);
        const float  f  = pos - static_cast<float>(i0);
        const float  a  = line_[i0 & (kSize - 1)];
        const float  b  = line_[(i0 + 1) & (kSize - 1)];
        return a + (b - a) * f;
    }

    float  sr_     = 48000.f;
    float  line_[kSize];
    size_t write_  = 0;
    float  phase_  = 0.f;
    float  amount_ = 0.f;
};

} // namespace synth
