/** @file voice.h
 *  @brief One synth voice: four oscillators with sync and cross-mod, noise, a
 *  ladder filter with its own envelope, and an amp envelope.
 */
#pragma once
#include "dsp.h"
#include "osc.h"
#include "ladder.h"
#include "env.h"
#include "params.h"

namespace synth
{

/** Everything a voice needs from the knobs, worked out once per block by the
 *  engine and shared by all voices. */
struct VoiceParams
{
    Wave  wave[kNumOscs];
    float pitch_offset[kNumOscs]; // semitones: octave + detune + tune
    float level[kNumOscs];
    float pw[kNumOscs];
    bool  sync;
    float xmod;        // 0..1
    float sweep_st;    // semitones of filter envelope into osc 2-4 pitch
    float noise;
    float cutoff_st;   // semitones above 16.35 Hz (C0)
    float fenv_st;     // signed
    float keytrack;    // 0..1
    float resonance;
    float drive;
    float vel_filter;
    float vel_amp;
    float glide_coef;  // per block, 1 = instant
    float lfo_pitch_st;
    float lfo_pw;
    float lfo_cutoff_st;
    float bend_st;
};

class Voice
{
  public:
    void Init(float sample_rate)
    {
        sr_ = sample_rate;
        for(int i = 0; i < kNumOscs; i++)
            osc_[i].Reset(0.25f * i); // free-running analog oscillators never start in phase
        filter_.Init(sample_rate);
        fenv_.Init(sample_rate);
        aenv_.Init(sample_rate);
    }

    void NoteOn(float note, float velocity, bool retrigger, bool glide)
    {
        target_note_ = note;
        if(!glide || !aenv_.Active())
            note_ = note;
        velocity_ = velocity;
        gate_     = true;
        if(retrigger || !aenv_.Active())
        {
            fenv_.Gate(true);
            aenv_.Gate(true);
        }
    }

    void Glide(float note) { target_note_ = note; }

    void NoteOff()
    {
        gate_ = false;
        fenv_.Gate(false);
        aenv_.Gate(false);
    }

    void SetEnvelopes(float fa, float fd, float fs, float fr, float aa, float ad, float as, float ar)
    {
        fenv_.SetTimes(fa, fd, fs, fr);
        aenv_.SetTimes(aa, ad, as, ar);
    }

    /** Adds `size` samples of this voice to out (mono). */
    void Render(const VoiceParams& p, float lfo, float* out, size_t size)
    {
        if(!aenv_.Active())
            return;

        // Control-rate work: pitch, glide, filter cutoff for the end of this block.
        note_ += (target_note_ - note_) * p.glide_coef;
        const float note = note_ + p.bend_st + lfo * p.lfo_pitch_st + detune_;

        float inc[kNumOscs];
        for(int i = 0; i < kNumOscs; i++)
            inc[i] = MidiToHz(note + p.pitch_offset[i]) / sr_;

        // Sync sweep: the filter envelope pushes osc 2-4 up, so the synced
        // waveform's harmonics sweep. Uses the envelope at the start of the block.
        const float fenv_now = fenv_.Value();
        if(p.sweep_st > 0.f)
        {
            const float mult = FastExp2(p.sweep_st * fenv_now * (1.f / 12.f));
            for(int i = 1; i < kNumOscs; i++)
                inc[i] *= mult;
        }

        const float vel_f = 1.f - p.vel_filter + p.vel_filter * velocity_;
        const float vel_a = 1.f - p.vel_amp + p.vel_amp * velocity_;

        float pw[kNumOscs];
        for(int i = 0; i < kNumOscs; i++)
        {
            osc_[i].SetWave(p.wave[i]);
            pw[i] = p.pw[i] + lfo * p.lfo_pw;
        }

        filter_.SetResonance(p.resonance);
        filter_.SetDrive(p.drive);

        // Cutoff tracks the envelope as it will be at the end of the block;
        // the filter ramps its coefficient across the block to get there.
        float fenv_block[64];
        for(size_t s = 0; s < size; s++)
            fenv_block[s] = fenv_.Process();
        const float cutoff_st = p.cutoff_st + p.keytrack * (note - 60.f)
                                + fenv_block[size - 1] * p.fenv_st * vel_f
                                + lfo * p.lfo_cutoff_st;
        filter_.SetCutoffBlock(16.35f * FastExp2(cutoff_st * (1.f / 12.f)), size);

        const float xmod_depth = p.xmod * 3.f; // octaves of FM at full depth

        for(size_t s = 0; s < size; s++)
        {
            float wrap;
            const float o1 = osc_[0].Process(inc[0], pw[0], -1.f, &wrap);
            const float sync_t = p.sync ? wrap : -1.f;
            const float fm     = xmod_depth > 0.f ? FastExp2(o1 * xmod_depth) : 1.f;

            float mix = o1 * p.level[0];
            for(int i = 1; i < kNumOscs; i++)
            {
                float w;
                if(p.level[i] > 0.f || p.sync)
                    mix += osc_[i].Process(inc[i] * fm, pw[i], sync_t, &w) * p.level[i];
            }
            // A trace of noise always, like a real circuit: it is what lets a
            // self-oscillating filter start singing with the oscillators off.
            mix += noise_.Process() * (p.noise + 0.0005f);

            const float y = filter_.Process(mix * 0.35f);
            out[s] += y * aenv_.Process() * vel_a;
        }
    }

    inline bool  Gate() const { return gate_; }
    inline bool  Sounding() const { return aenv_.Active(); }
    inline float Level() const { return aenv_.Value(); }

    int   note_id = -1;   // the MIDI note this voice was started by
    float pan     = 0.5f; // 0 = left, 1 = right
    float detune_ = 0.f;  // unison spread, semitones

  private:
    float  sr_          = 48000.f;
    Osc    osc_[kNumOscs];
    Ladder filter_;
    Adsr   fenv_, aenv_;
    Noise  noise_;
    float  note_        = 60.f;
    float  target_note_ = 60.f;
    float  velocity_    = 1.f;
    bool   gate_        = false;
};

} // namespace synth
