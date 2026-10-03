/** @file chorus.h
 *  @brief Stereo ensemble chorus in the spirit of the Juno-60: one short delay
 *  swept by a triangle, read out of phase for left and right. One knob sets
 *  both depth and mix.
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

        // 0.5 Hz sweep, slower and wider as the knob goes up.
        phase_ += (0.35f + 0.5f * (1.f - amount_)) / sr_;
        if(phase_ >= 1.f)
            phase_ -= 1.f;
        const float tri = phase_ < 0.5f ? 4.f * phase_ - 1.f : 3.f - 4.f * phase_;

        const float centre = 0.007f * sr_;               // 7 ms
        const float depth  = (0.0015f + 0.0035f * amount_) * sr_;
        const float wl     = Read(centre + depth * tri);
        const float wr     = Read(centre - depth * tri);

        const float wet = 0.35f + 0.35f * amount_;
        *l              = *l * (1.f - 0.5f * wet) + wl * wet;
        *r              = *r * (1.f - 0.5f * wet) + wr * wet;
    }

  private:
    inline float Read(float delay_samples) const
    {
        float       pos = static_cast<float>(write_) - delay_samples;
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
