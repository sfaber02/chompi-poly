/** @file env.h
 *  @brief Analog-style ADSR.
 *
 *  Each stage is an RC curve chasing a target. Attack aims past 1.0 so it
 *  rises in a near-straight line and lands on 1 in about the set time, the way
 *  a capacitor-charging envelope does. Decay and release fall exponentially.
 *  Retriggering starts the attack from wherever the envelope is, so there is
 *  no click.
 */
#pragma once
#include "dsp.h"

namespace synth
{

class Adsr
{
  public:
    enum class Stage : uint8_t
    {
        IDLE,
        ATTACK,
        DECAY,
        SUSTAIN,
        RELEASE,
    };

    void Init(float sample_rate)
    {
        sr_    = sample_rate;
        value_ = 0.f;
        stage_ = Stage::IDLE;
        SetTimes(0.005f, 0.2f, 0.7f, 0.3f);
    }

    /** Seconds for A, D, R; sustain level 0..1. */
    void SetTimes(float a, float d, float s, float r)
    {
        // Attack aims at 1.3 and stops at 1: ln(1.3/0.3) ~ 1.47 time constants.
        attack_coef_  = TimeToCoef(a / 1.47f, sr_);
        // Decay/release: reach ~1 % (4.6 time constants) in the set time.
        decay_coef_   = TimeToCoef(d / 4.6f, sr_);
        release_coef_ = TimeToCoef(r / 4.6f, sr_);
        sustain_      = s;
    }

    void Gate(bool on)
    {
        if(on)
            stage_ = Stage::ATTACK;
        else if(stage_ != Stage::IDLE)
            stage_ = Stage::RELEASE;
    }

    inline float Process()
    {
        switch(stage_)
        {
            case Stage::ATTACK:
                value_ += (1.3f - value_) * attack_coef_;
                if(value_ >= 1.f)
                {
                    value_ = 1.f;
                    stage_ = Stage::DECAY;
                }
                break;
            case Stage::DECAY:
                value_ += (sustain_ - value_) * decay_coef_;
                break;
            case Stage::RELEASE:
                value_ += (0.f - value_) * release_coef_;
                if(value_ < 0.0001f)
                {
                    value_ = 0.f;
                    stage_ = Stage::IDLE;
                }
                break;
            default: break;
        }
        return value_;
    }

    inline bool  Active() const { return stage_ != Stage::IDLE; }
    inline bool  Releasing() const { return stage_ == Stage::RELEASE; }
    inline float Value() const { return value_; }

  private:
    float sr_           = 48000.f;
    float value_        = 0.f;
    float sustain_      = 0.7f;
    float attack_coef_  = 1.f;
    float decay_coef_   = 1.f;
    float release_coef_ = 1.f;
    Stage stage_        = Stage::IDLE;
};

} // namespace synth
