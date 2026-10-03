/** @file arp.h
 *  @brief Arpeggiator: up, down, up/down, random, and as-played, over 1-3
 *  octaves, sixteenth notes at the set tempo. Hold (latch) keeps the pattern
 *  going after you let go; the next fresh chord replaces it.
 */
#pragma once
#include "dsp.h"

namespace synth
{

enum class ArpMode : uint8_t
{
    OFF,
    UP,
    DOWN,
    UP_DOWN,
    RANDOM,
    PLAYED,
    COUNT,
};

class Arp
{
  public:
    static constexpr int kMaxNotes = 16;

    struct Event
    {
        bool  on;
        int   note;
        float velocity;
    };

    void Reset()
    {
        count_   = 0;
        pressed_ = 0;
        step_    = -1;
        dir_     = 1;
        clock_   = 0.f;
        playing_ = -1;
    }

    void SetMode(ArpMode m) { mode_ = m; }
    void SetOctaves(int o) { octaves_ = o < 1 ? 1 : (o > 3 ? 3 : o); }
    void SetTempo(float bpm) { step_seconds_ = 60.f / bpm / 4.f; }
    void SetGate(float g) { gate_ = Clamp(g, 0.05f, 1.f); }

    void SetHold(bool hold)
    {
        hold_ = hold;
        if(!hold_ && pressed_ == 0)
            count_ = 0; // releasing hold with no keys down stops the pattern
    }
    bool Hold() const { return hold_; }
    bool Running() const { return count_ > 0; }

    void NoteOn(int note, float velocity)
    {
        // First key of a new chord while holding replaces the latched pattern.
        if(pressed_ == 0 && hold_)
            count_ = 0;
        pressed_++;
        for(int i = 0; i < count_; i++)
            if(notes_[i] == note)
                return;
        if(count_ < kMaxNotes)
        {
            notes_[count_]    = note;
            velocity_[count_] = velocity;
            count_++;
        }
        if(count_ == 1)
            clock_ = step_seconds_; // start right away on the first note
    }

    void NoteOff(int note)
    {
        if(pressed_ > 0)
            pressed_--;
        if(hold_)
            return;
        for(int i = 0; i < count_; i++)
        {
            if(notes_[i] == note)
            {
                for(int j = i; j < count_ - 1; j++)
                {
                    notes_[j]    = notes_[j + 1];
                    velocity_[j] = velocity_[j + 1];
                }
                count_--;
                break;
            }
        }
    }

    /** Advance by dt seconds. Writes up to two events (off, then on).
     *  @return number of events written */
    int Process(float dt, Event* ev)
    {
        int n = 0;
        if(count_ == 0)
        {
            if(playing_ >= 0)
            {
                ev[n++]  = {false, playing_, 0.f};
                playing_ = -1;
            }
            step_ = -1;
            return n;
        }

        clock_ += dt;
        if(playing_ >= 0 && clock_ >= step_seconds_ * gate_ && gate_ < 1.f)
        {
            ev[n++]  = {false, playing_, 0.f};
            playing_ = -1;
        }

        if(clock_ >= step_seconds_)
        {
            clock_ -= step_seconds_;
            if(clock_ > step_seconds_)
                clock_ = 0.f;
            if(playing_ >= 0)
                ev[n++] = {false, playing_, 0.f};

            float vel;
            const int note = NextNote(&vel);
            ev[n++]  = {true, note, vel};
            playing_ = note;
        }
        return n;
    }

    /** True for one call after each new step; drives the play-key LED. */
    bool StepFlash()
    {
        const bool f = flash_;
        flash_       = false;
        return f;
    }

  private:
    int NextNote(float* vel)
    {
        flash_ = true;

        // Notes in pitch order, or press order for PLAYED.
        int order[kMaxNotes];
        for(int i = 0; i < count_; i++)
            order[i] = i;
        if(mode_ != ArpMode::PLAYED)
        {
            for(int i = 1; i < count_; i++)
                for(int j = i; j > 0 && notes_[order[j]] < notes_[order[j - 1]]; j--)
                {
                    const int t  = order[j];
                    order[j]     = order[j - 1];
                    order[j - 1] = t;
                }
        }

        const int len = count_ * octaves_;
        int       pos;
        switch(mode_)
        {
            case ArpMode::DOWN:
                step_ = (step_ + 1) % len;
                pos   = len - 1 - step_;
                break;
            case ArpMode::UP_DOWN:
                if(len == 1)
                    pos = step_ = 0;
                else
                {
                    step_ += dir_;
                    if(step_ >= len)
                    {
                        step_ = len - 2;
                        dir_  = -1;
                    }
                    else if(step_ < 0)
                    {
                        step_ = len > 1 ? 1 : 0;
                        dir_  = 1;
                    }
                    pos = step_;
                }
                break;
            case ArpMode::RANDOM:
                pos = static_cast<int>((rng_.Process() * 0.5f + 0.5f) * len);
                if(pos >= len)
                    pos = len - 1;
                break;
            default: // UP, PLAYED
                step_ = (step_ + 1) % len;
                pos   = step_;
                break;
        }

        const int idx    = order[pos % count_];
        const int octave = pos / count_;
        *vel             = velocity_[idx];
        return notes_[idx] + 12 * octave;
    }

    int     notes_[kMaxNotes];
    float   velocity_[kMaxNotes];
    int     count_        = 0;
    int     pressed_      = 0;
    int     step_         = -1;
    int     dir_          = 1;
    int     octaves_      = 1;
    int     playing_      = -1;
    float   clock_        = 0.f;
    float   step_seconds_ = 0.125f;
    float   gate_         = 0.5f;
    bool    hold_         = false;
    bool    flash_        = false;
    ArpMode mode_         = ArpMode::OFF;
    Noise   rng_;
};

} // namespace synth
