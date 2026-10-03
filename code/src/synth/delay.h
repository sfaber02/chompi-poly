/** @file delay.h
 *  @brief Stereo ping-pong delay, tape-flavoured: the time knob is slewed so
 *  turning it bends the repeats, and the feedback path is low-passed so each
 *  repeat is darker than the last. Memory is supplied by the caller (it lives
 *  in SDRAM on the hardware), stored as int16 to halve it.
 */
#pragma once
#include "dsp.h"

namespace synth
{

class Delay
{
  public:
    struct Frame
    {
        int16_t l, r;
    };

    void Init(Frame* mem, size_t frames, float sample_rate)
    {
        line_   = mem;
        size_   = frames;
        sr_     = sample_rate;
        write_  = 0;
        time_   = time_target_ = 0.3f * sr_;
        lp_l_ = lp_r_ = 0.f;
        for(size_t i = 0; i < size_; i++)
            line_[i] = {0, 0};
    }

    void SetTime(float seconds) { time_target_ = Clamp(seconds * sr_, 16.f, size_ - 2.f); }
    void SetFeedback(float fb) { feedback_ = Clamp(fb, 0.f, 0.95f); }
    void SetMix(float mix) { mix_ = Clamp(mix, 0.f, 1.f); }

    /** Loudest signal written since the last call (1.0 = full scale at the
     *  delay's input; it bends from 1.5 up). For the DIAG clip log. */
    float TakePeak()
    {
        const float p = peak_in_;
        peak_in_      = 0.f;
        return p;
    }

    inline void Process(float* l, float* r)
    {
        // Slew the read head: ~80 ms to settle, like a tape machine's motor.
        time_ += (time_target_ - time_) * 0.0003f;

        float dl, dr;
        Read(time_, &dl, &dr);

        // Ping-pong: the mono input enters on the left, each repeat crosses over.
        const float in = 0.5f * (*l + *r);
        lp_l_ += (dr - lp_l_) * 0.35f;
        lp_r_ += (dl - lp_r_) * 0.35f;
        const float wl = in + FastTanh(lp_l_ * feedback_);
        const float wr = FastTanh(lp_r_ * feedback_);

        // Stored at half scale with a soft knee: 6 dB of headroom before
        // anything bends, and it bends instead of chopping (16-bit memory
        // clips hard otherwise, which is what made loud chords crunch).
        line_[write_] = {ToS16(SoftLimit(wl * 0.5f)), ToS16(SoftLimit(wr * 0.5f))};
        peak_in_      = fmaxf(peak_in_, fmaxf(fabsf(wl), fabsf(wr)));
        write_        = write_ + 1 < size_ ? write_ + 1 : 0;

        *l = *l + dl * mix_;
        *r = *r + dr * mix_;
    }

  private:
    static inline int16_t ToS16(float x)
    {
        x = Clamp(x, -1.f, 1.f);
        return static_cast<int16_t>(x * 32767.f);
    }

    inline void Read(float delay, float* l, float* r) const
    {
        float pos = static_cast<float>(write_) - delay;
        if(pos < 0.f)
            pos += static_cast<float>(size_);
        const size_t i0 = static_cast<size_t>(pos);
        const size_t i1 = i0 + 1 < size_ ? i0 + 1 : 0;
        const float  f  = pos - static_cast<float>(i0);
        const Frame  a  = line_[i0];
        const Frame  b  = line_[i1];
        constexpr float k = 1.f / 32768.f;
        *l = (a.l + (b.l - a.l) * f) * k * 2.f;
        *r = (a.r + (b.r - a.r) * f) * k * 2.f;
    }

    Frame* line_       = nullptr;
    size_t size_       = 0;
    size_t write_      = 0;
    float  sr_         = 48000.f;
    float  time_       = 0.f;
    float  time_target_ = 0.f;
    float  feedback_   = 0.f;
    float  mix_        = 0.f;
    float  lp_l_       = 0.f;
    float  lp_r_       = 0.f;
    float  peak_in_    = 0.f;
};

} // namespace synth
