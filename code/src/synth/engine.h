/** @file engine.h
 *  @brief The whole instrument: note handling (mono/poly, unison, glide,
 *  sustain, arpeggiator), the voices, the LFO, and the effects chain
 *  chorus -> delay -> reverb -> saturation/limiter.
 *
 *  Threading: everything here runs in the audio interrupt. The UI and MIDI
 *  call NoteOn/NoteOff/Cc from that same interrupt, and only write the param
 *  array from anywhere else (single float stores, so a block may see a mix of
 *  old and new values, which is harmless).
 */
#pragma once
#include "dsp.h"
#include "params.h"
#include "voice.h"
#include "lfo.h"
#include "arp.h"
#include "chorus.h"
#include "delay.h"
#include "../reverb.h"

namespace synth
{

constexpr int kMaxVoices = 6;

class Engine
{
  public:
    float params[NUM_PARAMS];
    float volume = 0.7f; // master, not saved in presets

    void Init(float sample_rate, Delay::Frame* delay_mem, size_t delay_frames, daisysp::Reverb* reverb)
    {
        sr_ = sample_rate;
        for(int i = 0; i < NUM_PARAMS; i++)
            params[i] = kParams[i].def;
        for(int i = 0; i < kMaxVoices; i++)
            voices_[i].Init(sample_rate);
        chorus_.Init(sample_rate);
        delay_.Init(delay_mem, delay_frames, sample_rate);
        reverb_ = reverb;
        reverb_->Init(sample_rate);
        // Its buffer lives in DTCM, which startup does not zero: without this
        // the first seconds replay whatever the last firmware left there.
        reverb_->Clear();
        reverb_->SetInputGain(0.25f);
        arp_.Reset();
        mono_count_ = 0;
        for(int i = 0; i < 128; i++)
            held_[i] = sustained_[i] = false;
    }

    // ---------------------------------------------------------------- notes

    /** Mono (true) or poly. On the hardware this is the toggle switch. */
    void SetMono(bool mono)
    {
        if(mono != mono_)
        {
            AllNotesOff();
            mono_ = mono;
        }
    }
    bool Mono() const { return mono_; }

    void NoteOn(int note, float velocity)
    {
        if(note < 0 || note > 127)
            return;
        held_[note]      = true;
        sustained_[note] = false;
        if(ArpOn())
            arp_.NoteOn(note, velocity);
        else
            PlayOn(note, velocity);
    }

    void NoteOff(int note)
    {
        if(note < 0 || note > 127 || !held_[note])
            return;
        held_[note] = false;
        if(sustain_)
        {
            sustained_[note] = true;
            return;
        }
        if(ArpOn())
            arp_.NoteOff(note);
        else
            PlayOff(note);
    }

    void SetSustain(bool on)
    {
        sustain_ = on;
        if(!on)
        {
            for(int n = 0; n < 128; n++)
            {
                if(sustained_[n])
                {
                    sustained_[n] = false;
                    if(ArpOn())
                        arp_.NoteOff(n);
                    else
                        PlayOff(n);
                }
            }
        }
    }

    void SetPitchBend(float semitones) { bend_st_ = semitones; }
    void SetModWheel(float v) { mod_wheel_ = v; }

    void SetArpHold(bool hold) { arp_.SetHold(hold); }
    bool ArpHold() const { return arp_.Hold(); }
    bool ArpOn() const { return arp_mode_ != ArpMode::OFF; }
    bool ArpStepFlash() { return arp_.StepFlash(); }

    void AllNotesOff()
    {
        for(int i = 0; i < kMaxVoices; i++)
        {
            voices_[i].NoteOff();
            voices_[i].note_id = -1;
        }
        mono_count_ = 0;
        arp_.Reset();
        for(int i = 0; i < 128; i++)
            held_[i] = sustained_[i] = false;
    }

    /** For the key LEDs: is this MIDI note sounding (gate on)? */
    bool NoteSounding(int note) const
    {
        for(int i = 0; i < kMaxVoices; i++)
            if(voices_[i].note_id == note && voices_[i].Gate())
                return true;
        return false;
    }

    /** Peak levels at each stage since the last call, for the DIAG clip
     *  log. Called from the main loop; a reset racing the audio interrupt
     *  only loses one block's peak. */
    struct Levels
    {
        float voices, delay_in, reverb_in, pre_out, limit_gain;
    };
    Levels TakeLevels()
    {
        Levels l{lv_voices_, delay_.TakePeak(), lv_reverb_in_, lv_pre_out_, lv_limit_};
        lv_voices_ = lv_reverb_in_ = lv_pre_out_ = 0.f;
        lv_limit_                                = 1.f;
        return l;
    }

    /** Output level 0..1 for the volume knob's meter. */
    float Meter() const { return meter_; }

    // ---------------------------------------------------------------- audio

    void Process(float* out_l, float* out_r, size_t size)
    {
        const float dt = static_cast<float>(size) / sr_;
        UpdateParams(dt);

        // Arpeggiator runs at block rate (0.5 ms steps at 48 kHz / 24).
        if(arp_mode_ != last_arp_mode_)
        {
            const bool was_off = last_arp_mode_ == ArpMode::OFF;
            const bool now_off = arp_mode_ == ArpMode::OFF;
            if(was_off && !now_off)
            {
                // Hand the keys that are down over to the arp.
                for(int v = 0; v < kMaxVoices; v++)
                    voices_[v].NoteOff();
                mono_count_ = 0;
                arp_.Reset();
                for(int n = 0; n < 128; n++)
                    if(held_[n])
                        arp_.NoteOn(n, 0.8f);
            }
            else if(!was_off && now_off)
            {
                arp_.Reset();
                ReleaseAll();
            }
            last_arp_mode_ = arp_mode_;
        }
        if(ArpOn())
        {
            Arp::Event ev[3];
            const int  n = arp_.Process(dt, ev);
            for(int i = 0; i < n; i++)
            {
                if(ev[i].on)
                    PlayOn(ev[i].note, ev[i].velocity);
                else
                    PlayOff(ev[i].note);
            }
        }

        const float lfo = lfo_.Process(lfo_hz_, dt);

        float mono[64];
        for(size_t i = 0; i < size; i++)
            out_l[i] = out_r[i] = 0.f;

        for(int v = 0; v < kMaxVoices; v++)
        {
            Voice& voice = voices_[v];
            if(!voice.Sounding())
                continue;
            for(size_t i = 0; i < size; i++)
                mono[i] = 0.f;
            voice.Render(vp_, lfo, mono, size);
            // Centre is full level on both sides; hard-panned is full on one.
            const float pl = Clamp(2.f * (1.f - voice.pan), 0.f, 1.f);
            const float pr = Clamp(2.f * voice.pan, 0.f, 1.f);
            for(size_t i = 0; i < size; i++)
            {
                out_l[i] += mono[i] * pl;
                out_r[i] += mono[i] * pr;
            }
        }

        float peak = 0.f;
        for(size_t i = 0; i < size; i++)
        {
            float l = out_l[i];
            float r = out_r[i];

            lv_voices_ = fmaxf(lv_voices_, fmaxf(fabsf(l), fabsf(r)));
            chorus_.Process(&l, &r);
            delay_.Process(&l, &r);

            // The reverb also keeps its tank in 16 bits and clips hard. Run it
            // at half level and bring it back up: 6 dB of headroom, same mix.
            lv_reverb_in_ = fmaxf(lv_reverb_in_, fmaxf(fabsf(l), fabsf(r)));
            // Amount is pinned at 1 so the reverb returns only its wet
            // signal; the mix knob's dry/wet law is applied here.
            float rl = l * 0.5f, rr = r * 0.5f;
            reverb_->Process(&rl, &rr);
            l = l * rev_dry_ + rl * 2.f * rev_wet_;
            r = r * rev_dry_ + rr * 2.f * rev_wet_;

            // DC blocker
            dc_l_ += (l - dc_l_) * 0.0005f;
            dc_r_ += (r - dc_r_) * 0.0005f;
            l -= dc_l_;
            r -= dc_r_;

            // Saturation: from clean to driven tanh, level-matched.
            l = FastTanh(l * sat_gain_) * sat_makeup_;
            r = FastTanh(r * sat_gain_) * sat_makeup_;

            // Smoothed master volume, then a fast peak limiter.
            vol_ += (volume - vol_) * 0.002f;
            l *= vol_ * 2.f;
            r *= vol_ * 2.f;
            const float p = fabsf(l) > fabsf(r) ? fabsf(l) : fabsf(r);
            lim_env_      = p > lim_env_ ? p : lim_env_ * 0.9998f;
            const float g = lim_env_ > 0.95f ? 0.95f / lim_env_ : 1.f;
            lv_limit_     = fminf(lv_limit_, g);
            lv_pre_out_   = fmaxf(lv_pre_out_, p);
            out_l[i]      = l * g;
            out_r[i]      = r * g;
            if(p > peak)
                peak = p;
        }
        meter_ = peak > meter_ ? peak : meter_ * 0.97f;
    }

  private:
    // ------------------------------------------------- voice allocation

    void PlayOn(int note, float velocity)
    {
        if(mono_)
        {
            // Last-note priority stack; legato notes glide without retriggering.
            for(int i = 0; i < mono_count_; i++)
            {
                if(mono_stack_[i] == note)
                {
                    for(int j = i; j < mono_count_ - 1; j++)
                        mono_stack_[j] = mono_stack_[j + 1];
                    mono_count_--;
                    break;
                }
            }
            if(mono_count_ < kStack)
                mono_stack_[mono_count_++] = note;

            const bool legato = voices_[0].Gate();
            const int  n      = MonoVoices();
            for(int v = 0; v < n; v++)
            {
                voices_[v].detune_ = UnisonDetune(v, n);
                voices_[v].pan     = UnisonPan(v, n);
                voices_[v].note_id = note;
                voices_[v].NoteOn(static_cast<float>(note), velocity, !legato, glide_on_ || legato);
            }
            return;
        }

        // Poly: reuse a voice already on this note, else the quietest free
        // voice, else steal the oldest.
        int pick = -1;
        for(int v = 0; v < kMaxVoices; v++)
            if(voices_[v].note_id == note)
                pick = v;
        if(pick < 0)
        {
            float lowest = 2.f;
            for(int v = 0; v < kMaxVoices; v++)
            {
                if(!voices_[v].Gate() && voices_[v].Level() < lowest)
                {
                    lowest = voices_[v].Level();
                    pick   = v;
                }
            }
        }
        if(pick < 0)
        {
            uint32_t oldest = 0xFFFFFFFF;
            for(int v = 0; v < kMaxVoices; v++)
            {
                if(age_[v] < oldest)
                {
                    oldest = age_[v];
                    pick   = v;
                }
            }
        }

        Voice& voice  = voices_[pick];
        age_[pick]    = ++age_counter_;
        voice.detune_ = 0.f;
        voice.pan     = KeyPan(note);
        voice.note_id = note;
        voice.NoteOn(static_cast<float>(note), velocity, true, glide_on_);
    }

    void PlayOff(int note)
    {
        if(mono_)
        {
            int i = 0;
            for(; i < mono_count_; i++)
                if(mono_stack_[i] == note)
                    break;
            if(i == mono_count_)
                return;
            for(int j = i; j < mono_count_ - 1; j++)
                mono_stack_[j] = mono_stack_[j + 1];
            mono_count_--;

            const int n = MonoVoices();
            if(mono_count_ == 0)
            {
                for(int v = 0; v < kMaxVoices; v++)
                    voices_[v].NoteOff();
            }
            else
            {
                const int back = mono_stack_[mono_count_ - 1];
                for(int v = 0; v < n; v++)
                {
                    voices_[v].note_id = back;
                    voices_[v].Glide(static_cast<float>(back));
                }
            }
            return;
        }

        for(int v = 0; v < kMaxVoices; v++)
            if(voices_[v].note_id == note && voices_[v].Gate())
                voices_[v].NoteOff();
    }

    void ReleaseAll()
    {
        for(int v = 0; v < kMaxVoices; v++)
            voices_[v].NoteOff();
        mono_count_ = 0;
        // Keys still physically down start sounding again without the arp.
        for(int n = 0; n < 128; n++)
            if(held_[n])
                PlayOn(n, 0.8f);
    }

    int MonoVoices() const { return unison_ > 0.f ? kMaxVoices : 1; }

    float UnisonDetune(int v, int n) const
    {
        if(n == 1)
            return 0.f;
        const float pos = static_cast<float>(v) / (n - 1) * 2.f - 1.f; // -1..1
        return pos * unison_ * 0.5f; // up to +/- half a semitone
    }

    float UnisonPan(int v, int n) const
    {
        if(n == 1)
            return 0.5f;
        const float pos = static_cast<float>(v) / (n - 1) - 0.5f;
        return 0.5f + pos * spread_;
    }

    /** Poly voices are placed by pitch, low left to high right, like a
     *  piano. One note around middle C sits in the centre (it used to
     *  depend on which voice it landed on, which pushed single notes
     *  left), and chords open out. */
    float KeyPan(int note) const
    {
        return Clamp(0.5f + spread_ * (note - 60) / 36.f, 0.05f, 0.95f);
    }

    // ------------------------------------------------- knobs -> units

    void UpdateParams(float dt)
    {
        const float* p = params;

        for(int o = 0; o < kNumOscs; o++)
        {
            const float* op = &p[OSC1_WAVE + o * kOscParams];
            vp_.wave[o]     = static_cast<Wave>(StepIndex(op[0], 3));
            const int oct   = StepIndex(op[1], 4); // 16' 8' 4' 2'
            // Detune: linear, +/- 60 cents, about 1 cent per encoder click.
            // (It used to be squared "for fine control near the middle", which
            // made the first dozen clicks inaudible.) Only heard against another
            // oscillator: two at nearly the same pitch beat and thicken.
            const float d   = (op[3] - 0.5f) * 2.f;
            vp_.pitch_offset[o] = 12.f * (oct - 1) + 0.6f * d;
            vp_.level[o]        = op[2] * op[2];
            vp_.pw[o]           = 0.5f + 0.45f * (op[4] - 0.5f) * 2.f;
        }
        const float tune = (p[TUNE] - 0.5f) * 2.f; // +/- 1 semitone
        for(int o = 0; o < kNumOscs; o++)
            vp_.pitch_offset[o] += tune;

        vp_.sync      = StepIndex(p[SYNC], 2) == 1;
        vp_.xmod      = p[XMOD] * p[XMOD];
        vp_.sweep_st  = p[SWEEP] * 48.f;
        vp_.noise     = p[NOISE] * p[NOISE] * 0.8f;
        vp_.cutoff_st = p[CUTOFF] * 120.f;              // C0 .. C10
        vp_.fenv_st   = (p[FENV_AMT] - 0.5f) * 2.f * 84.f; // +/- 7 octaves
        vp_.keytrack  = p[KEYTRACK];
        vp_.resonance = p[RESONANCE];
        vp_.drive     = 1.f + 7.f * p[DRIVE] * p[DRIVE];
        vp_.vel_filter = p[VEL_FILTER];
        vp_.vel_amp    = p[VEL_AMP];
        vp_.bend_st    = bend_st_;

        // Glide 0 - 2 s (time to cover most of the distance).
        glide_on_ = p[GLIDE] > 0.01f;
        const float glide_s  = glide_on_ ? KnobToTime(p[GLIDE], 0.01f, 2.f) : 0.f;
        const float block_dt = dt;
        vp_.glide_coef       = glide_on_ ? 1.f - expf(-block_dt * 3.f / glide_s) : 1.f;

        lfo_hz_ = KnobToTime(p[LFO_RATE], 0.05f, 30.f);
        lfo_.SetShape(static_cast<LfoShape>(StepIndex(p[LFO_SHAPE], 5)));
        const float wheel  = mod_wheel_ > p[LFO_PITCH] ? mod_wheel_ : p[LFO_PITCH];
        vp_.lfo_pitch_st   = wheel * wheel * 12.f;
        vp_.lfo_pw         = p[LFO_PWM] * 0.45f;
        vp_.lfo_cutoff_st  = p[LFO_CUTOFF] * p[LFO_CUTOFF] * 48.f;

        // Envelopes only need recomputing when their knobs move.
        float env[8] = {p[FENV_A], p[FENV_D], p[FENV_S], p[FENV_R],
                        p[AENV_A], p[AENV_D], p[AENV_S], p[AENV_R]};
        bool  changed = false;
        for(int i = 0; i < 8; i++)
        {
            if(env[i] != env_cache_[i])
            {
                env_cache_[i] = env[i];
                changed       = true;
            }
        }
        if(changed)
        {
            const float fa = KnobToTime(env[0], 0.001f, 10.f);
            const float fd = KnobToTime(env[1], 0.005f, 10.f);
            const float fr = KnobToTime(env[3], 0.005f, 10.f);
            const float aa = KnobToTime(env[4], 0.001f, 10.f);
            const float ad = KnobToTime(env[5], 0.005f, 10.f);
            const float ar = KnobToTime(env[7], 0.005f, 10.f);
            for(int v = 0; v < kMaxVoices; v++)
                voices_[v].SetEnvelopes(fa, fd, env[2], fr, aa, ad, env[6], ar);
        }

        unison_ = p[UNISON];
        spread_ = p[SPREAD];

        chorus_.SetAmount(p[CHORUS]);
        delay_.SetTime(KnobToTime(p[DLY_TIME], 0.02f, 1.9f));
        delay_.SetFeedback(p[DLY_FDBK]);
        delay_.SetMix(p[DLY_MIX]);
        reverb_->SetTime(0.35f + 0.63f * p[REV_SIZE]);
        reverb_->SetAmount(1.f);
        MixGains(p[REV_MIX], &rev_dry_, &rev_wet_);
        reverb_->SetLowpass(0.3f + 0.65f * p[REV_TONE]);
        reverb_->SetDiffusion(0.7f);

        const float sat = p[SATURATE];
        sat_gain_       = 0.6f + 6.f * sat * sat;
        sat_makeup_     = 1.f / FastTanh(sat_gain_) * (0.85f - 0.25f * sat);

        arp_mode_ = static_cast<ArpMode>(StepIndex(p[ARP_MODE], 6));
        arp_.SetMode(arp_mode_);
        arp_.SetOctaves(StepIndex(p[ARP_RANGE], 3) + 1);
        arp_.SetTempo(40.f + 200.f * p[ARP_TEMPO]);
        arp_.SetGate(p[ARP_GATE]);
    }

    static constexpr int kStack = 16;

    float           sr_ = 48000.f;
    Voice           voices_[kMaxVoices];
    uint32_t        age_[kMaxVoices] = {};
    uint32_t        age_counter_     = 0;
    VoiceParams     vp_{};
    Lfo             lfo_;
    float           lfo_hz_ = 1.f;
    Arp             arp_;
    ArpMode         arp_mode_      = ArpMode::OFF;
    ArpMode         last_arp_mode_ = ArpMode::OFF;
    Chorus          chorus_;
    Delay           delay_;
    daisysp::Reverb* reverb_ = nullptr;

    bool  mono_       = false;
    int   mono_stack_[kStack];
    int   mono_count_ = 0;
    bool  held_[128];
    bool  sustained_[128];
    bool  sustain_    = false;
    bool  glide_on_   = false;
    float bend_st_    = 0.f;
    float mod_wheel_  = 0.f;
    float unison_     = 0.f;
    float spread_     = 0.5f;
    float env_cache_[8] = {-1, -1, -1, -1, -1, -1, -1, -1};

    float rev_dry_ = 1.f, rev_wet_ = 0.f;
    float dc_l_ = 0.f, dc_r_ = 0.f;
    float sat_gain_ = 1.f, sat_makeup_ = 1.f;
    float vol_      = 0.f;
    float lim_env_  = 0.f;
    float meter_    = 0.f;
    float lv_voices_ = 0.f, lv_reverb_in_ = 0.f, lv_pre_out_ = 0.f, lv_limit_ = 1.f;
};

} // namespace synth
