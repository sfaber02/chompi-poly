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
    /** 0..1. The last couple of percent is infinite: repeats stop fading and
     *  stop darkening, and hold until you turn it back down. */
    void SetFeedback(float fb)
    {
        infinite_ = fb >= 0.98f;
        feedback_ = infinite_ ? 1.f : Clamp(fb, 0.f, 0.98f) * (0.95f / 0.98f);
    }
    void SetMix(float mix) { MixGains(mix, &dry_, &wet_); }

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

        // At infinite, read whole samples: interpolating between them is a
        // gentle low-pass, and a few hundred repeats of it is a fade.
        float dl, dr;
        Read(infinite_ ? floorf(time_ + 0.5f) : time_, &dl, &dr);

        // Ping-pong: the mono input enters on the left, each repeat crosses over.
        const float in = 0.5f * (*l + *r);
        // Each repeat is a little darker, except at infinite, where the
        // filter steps aside so the loop doesn't fade to a muffled hum.
        const float tone = infinite_ ? 1.f : 0.35f;
        lp_l_ += (dr - lp_l_) * tone;
        lp_r_ += (dl - lp_r_) * tone;
        // Normally the loop saturates gently (tanh), which also bleeds a
        // little level each pass. At infinite the loop is exactly unity and
        // only loud peaks get limited, so repeats hold for as long as you like.
        const float wl = in + (infinite_ ? SoftLimit(lp_l_) : FastTanh(lp_l_ * feedback_));
        const float wr = infinite_ ? SoftLimit(lp_r_) : FastTanh(lp_r_ * feedback_);

        // Stored at half scale with a soft knee: 6 dB of headroom before
        // anything bends, and it bends instead of chopping (16-bit memory
        // clips hard otherwise, which is what made loud chords crunch).
        line_[write_] = {ToS16(SoftLimit(wl * 0.5f)), ToS16(SoftLimit(wr * 0.5f))};
        peak_in_      = fmaxf(peak_in_, fmaxf(fabsf(wl), fabsf(wr)));
        write_        = write_ + 1 < size_ ? write_ + 1 : 0;

        *l = *l * dry_ + dl * wet_;
        *r = *r * dry_ + dr * wet_;
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
    float  dry_        = 1.f;
    float  wet_        = 0.f;
    bool   infinite_   = false;
    float  lp_l_       = 0.f;
    float  lp_r_       = 0.f;
    float  peak_in_    = 0.f;
};

} // namespace synth
