// Desktop harness: runs the exact synth engine the firmware runs and writes
// WAV files, so sounds can be checked by ear and FFT without flashing.
//
//   make -C host && host/render out_dir
//
// Each test is a patch (param overrides) plus a little score of notes.

#include "../code/src/synth/engine.h"
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <string>
#include <vector>
#include <chrono>

using namespace synth;

static constexpr float  kSr    = 48000.f;
static constexpr size_t kBlock = 24;

static Delay::Frame    g_delay[96000];
static daisysp::Reverb g_reverb;

struct Note
{
    float on_s, off_s;
    int   note;
    float vel;
};

struct Test
{
    const char*                         name;
    std::vector<std::pair<Param, float>> patch;
    std::vector<Note>                   notes;
    float                               seconds;
    bool                                mono;
};

static void WriteWav(const std::string& path, const std::vector<float>& l, const std::vector<float>& r)
{
    FILE* f = fopen(path.c_str(), "wb");
    if(!f)
    {
        perror(path.c_str());
        exit(1);
    }
    const uint32_t n = l.size(), data = n * 4, sr = kSr, br = kSr * 4, riff = 36 + data;
    const uint16_t pcm = 1, ch = 2, ba = 4, bits = 16;
    const uint32_t fmt_len = 16;
    fwrite("RIFF", 1, 4, f);
    fwrite(&riff, 4, 1, f);
    fwrite("WAVEfmt ", 1, 8, f);
    fwrite(&fmt_len, 4, 1, f);
    fwrite(&pcm, 2, 1, f);
    fwrite(&ch, 2, 1, f);
    fwrite(&sr, 4, 1, f);
    fwrite(&br, 4, 1, f);
    fwrite(&ba, 2, 1, f);
    fwrite(&bits, 2, 1, f);
    fwrite("data", 1, 4, f);
    fwrite(&data, 4, 1, f);
    for(uint32_t i = 0; i < n; i++)
    {
        int16_t s[2] = {static_cast<int16_t>(Clamp(l[i], -1.f, 1.f) * 32767.f),
                        static_cast<int16_t>(Clamp(r[i], -1.f, 1.f) * 32767.f)};
        fwrite(s, 2, 2, f);
    }
    fclose(f);
}

static Engine g_engine;

static void Run(const Test& t, const std::string& dir)
{
    Engine& e = g_engine;
    e.Init(kSr, g_delay, 96000, &g_reverb);
    e.volume = 0.7f;
    for(auto& kv : t.patch)
        e.params[kv.first] = kv.second;
    e.SetMono(t.mono);

    const size_t      total = static_cast<size_t>(t.seconds * kSr / kBlock);
    std::vector<float> L, R;
    L.reserve(total * kBlock);
    R.reserve(total * kBlock);
    float  l[kBlock], r[kBlock];
    float  peak = 0.f;
    double cpu  = 0.0;

    for(size_t b = 0; b < total; b++)
    {
        const float t0 = b * kBlock / kSr, t1 = (b + 1) * kBlock / kSr;
        for(auto& n : t.notes)
        {
            if(n.on_s >= t0 && n.on_s < t1)
                e.NoteOn(n.note, n.vel);
            if(n.off_s >= t0 && n.off_s < t1)
                e.NoteOff(n.note);
        }
        auto a = std::chrono::steady_clock::now();
        e.Process(l, r, kBlock);
        cpu += std::chrono::duration<double>(std::chrono::steady_clock::now() - a).count();
        for(size_t i = 0; i < kBlock; i++)
        {
            L.push_back(l[i]);
            R.push_back(r[i]);
            peak = std::max(peak, std::max(fabsf(l[i]), fabsf(r[i])));
            if(!std::isfinite(l[i]) || !std::isfinite(r[i]))
            {
                printf("%s: NaN/inf at %.3fs\n", t.name, (b * kBlock + i) / kSr);
                exit(1);
            }
        }
    }
    WriteWav(dir + "/" + t.name + ".wav", L, R);
    printf("%-14s peak %.2f  render %.1fx realtime\n", t.name, peak, t.seconds / cpu);
}

static std::vector<Note> Chord(float on, float off, std::initializer_list<int> ns, float vel = 0.8f)
{
    std::vector<Note> v;
    for(int n : ns)
        v.push_back({on, off, n, vel});
    return v;
}

static std::vector<Note> Scale(int from, int to, float step, float gate)
{
    std::vector<Note> v;
    float t = 0.1f;
    for(int n = from; n <= to; n += 1, t += step)
        v.push_back({t, t + gate, n, 0.8f});
    return v;
}

// ---------------------------------------------------------------------------
// Factory patches: written to card/SYNTH as P01.txt.. and rendered as demos.

struct Factory
{
    const char*                          name;
    bool                                 mono; // what the demo plays it as
    std::vector<std::pair<Param, float>> patch;
    int                                  slot = 0; // 0 = next in order, else 1-15
};

static const std::vector<Factory> kFactory = {
    {"sync_lead", true,
     {{OSC1_LEVEL, .25f}, {OSC2_LEVEL, 1.f}, {OSC2_DETUNE, .5f}, {SYNC, 1.f}, {SWEEP, .55f},
      {CUTOFF, .72f}, {RESONANCE, .25f}, {FENV_AMT, .62f}, {FENV_D, .55f}, {FENV_S, .15f},
      {GLIDE, .25f}, {LFO_PITCH, .12f}, {LFO_RATE, .5f}, {DLY_MIX, 0.12f}, {DLY_TIME, .5f},
      {REV_MIX, 0.059f}}},
    {"brass", false,
     {{OSC2_LEVEL, .85f}, {OSC2_DETUNE, .54f}, {OSC3_WAVE, 0.f}, {OSC3_LEVEL, .5f},
      {CUTOFF, .42f}, {RESONANCE, .12f}, {FENV_AMT, .76f}, {FENV_A, .33f}, {FENV_D, .5f},
      {FENV_S, .45f}, {FENV_R, .4f}, {AENV_A, .2f}, {AENV_R, .4f}, {CHORUS, .45f},
      {REV_MIX, 0.106f}}},
    {"unison_bass", true,
     {{OSC2_LEVEL, .8f}, {OSC3_LEVEL, .6f}, {OSC3_OCT, 0.f}, {UNISON, .45f}, {CUTOFF, .33f},
      {RESONANCE, .35f}, {FENV_AMT, .72f}, {FENV_D, .35f}, {FENV_S, .1f}, {AENV_R, .2f},
      {DRIVE, .55f}, {SATURATE, .45f}, {REV_MIX, 0.f}}},
    {"pwm_strings", false,
     {{OSC1_WAVE, .5f}, {OSC2_WAVE, .5f}, {OSC2_LEVEL, .7f}, {OSC2_DETUNE, .56f},
      {LFO_PWM, .7f}, {LFO_RATE, .32f}, {CUTOFF, .6f}, {FENV_AMT, .55f}, {AENV_A, .55f},
      {AENV_R, .6f}, {AENV_S, .9f}, {CHORUS, .85f}, {REV_MIX, 0.23f}, {REV_SIZE, .7f}}},
    {"xmod_bell", false,
     {{OSC1_LEVEL, .15f}, {OSC2_WAVE, 1.f}, {OSC2_OCT, 1.f}, {OSC2_LEVEL, 1.f},
      {OSC2_DETUNE, .5f}, {XMOD, .55f}, {CUTOFF, .85f}, {FENV_AMT, .5f}, {AENV_A, 0.f},
      {AENV_D, .68f}, {AENV_S, 0.f}, {AENV_R, .62f}, {REV_MIX, 0.194f}, {DLY_MIX, 0.08f}}},
    {"arp_pluck", false,
     {{OSC2_LEVEL, .6f}, {OSC2_OCT, 2 / 3.f}, {CUTOFF, .38f}, {RESONANCE, .45f},
      {FENV_AMT, .8f}, {FENV_D, .3f}, {FENV_S, 0.f}, {AENV_D, .32f}, {AENV_S, 0.f},
      {AENV_R, .3f}, {ARP_MODE, StepValue(3, 6)}, {ARP_RANGE, StepValue(1, 3)},
      {ARP_TEMPO, .4f}, {ARP_GATE, .4f}, {DLY_MIX, 0.16f}, {DLY_FDBK, .45f}, {DLY_TIME, .55f},
      {CHORUS, .3f}}},
    {"acid", true,
     {{OSC1_WAVE, 0.f}, {OSC2_LEVEL, 0.f}, {CUTOFF, .3f}, {RESONANCE, .82f}, {FENV_AMT, .78f},
      {FENV_D, .3f}, {FENV_S, 0.f}, {AENV_S, .9f}, {AENV_R, .1f}, {GLIDE, .2f}, {DRIVE, .6f},
      {SATURATE, .5f}, {VEL_FILTER, .6f}, {DLY_MIX, 0.08f}, {REV_MIX, 0.f}}},

    // Init: as close to one sine wave as this synth gets. One triangle, with
    // the filter tracking the keyboard at twice the note's pitch so its few
    // overtones are shaved off (3rd harmonic ~40 dB down). Everything else
    // off; organ envelope. A clean starting point for building a sound.
    {"init", false,
     {{OSC1_WAVE, 1.f}, {OSC1_OCT, 1 / 3.f}, {OSC1_LEVEL, .8f}, {OSC1_DETUNE, .5f}, {OSC1_PW, .5f},
      {OSC2_LEVEL, 0.f}, {OSC2_DETUNE, .5f}, {OSC3_LEVEL, 0.f}, {OSC3_DETUNE, .5f},
      {OSC4_LEVEL, 0.f}, {OSC4_DETUNE, .5f},
      {SYNC, 0.f}, {XMOD, 0.f}, {SWEEP, 0.f}, {GLIDE, 0.f}, {UNISON, 0.f},
      {CUTOFF, .5f}, {RESONANCE, 0.f}, {FENV_AMT, .5f}, {KEYTRACK, 1.f}, {DRIVE, 0.f},
      {VEL_FILTER, 0.f}, {AENV_A, 0.f}, {AENV_D, .5f}, {AENV_S, 1.f}, {AENV_R, .15f},
      {VEL_AMP, 0.f}, {LFO_PITCH, 0.f}, {LFO_PWM, 0.f}, {LFO_CUTOFF, 0.f},
      {CHORUS, 0.f}, {DLY_MIX, 0.f}, {REV_MIX, 0.f}, {SATURATE, 0.f}, {NOISE, 0.f},
      {SPREAD, .5f}, {ARP_MODE, 0.f}, {TUNE, .5f}},
     15},
};

static void WriteFactory(const std::string& card_dir, const std::string& wav_dir)
{
    for(size_t i = 0; i < kFactory.size(); i++)
    {
        const Factory& f    = kFactory[i];
        const size_t   slot = f.slot ? static_cast<size_t>(f.slot) : i + 1;
        float          p[NUM_PARAMS];
        for(int k = 0; k < NUM_PARAMS; k++)
            p[k] = kParams[k].def;
        for(auto& kv : f.patch)
            p[kv.first] = kv.second;

        char name[64];
        snprintf(name, sizeof name, "%s/P%02zu.txt", card_dir.c_str(), slot);
        FILE* fp = fopen(name, "w");
        if(!fp)
        {
            perror(name);
            exit(1);
        }
        for(int k = 0; k < NUM_PARAMS; k++)
            fprintf(fp, "%s %.4f\n", kParams[k].name, p[k]);
        fclose(fp);

        // Demo: a short phrase in the octave the patch is meant for.
        Test t{f.name, f.patch, {}, 5.f, f.mono};
        const int base = (strstr(f.name, "bass") || strstr(f.name, "acid")) ? 36 : 60;
        if(f.mono)
            t.notes = {{0.1f, 0.5f, base, 1.f}, {0.5f, 0.9f, base + 3, .7f}, {0.9f, 1.3f, base + 7, 1.f},
                       {1.3f, 2.4f, base + 10, .8f}, {2.6f, 3.6f, base + 12, 1.f}};
        else
            t.notes = Chord(0.1f, 2.6f, {base, base + 4, base + 7, base + 11});
        std::string fname = std::string("P") + std::to_string(slot) + "_" + f.name;
        t.name            = fname.c_str();
        Run(t, wav_dir);
    }
}

int main(int argc, char** argv)
{
    if(argc > 3 && std::string(argv[1]) == "--factory")
    {
        WriteFactory(argv[2], argv[3]);
        return 0;
    }

    const std::string dir = argc > 1 ? argv[1] : ".";

    std::vector<Test> tests;

    // Raw saw swept across the keyboard, filter wide open: listen for aliasing.
    tests.push_back({"saw_sweep",
                     {{OSC2_LEVEL, 0.f}, {CUTOFF, 1.f}, {FENV_AMT, .5f}, {RESONANCE, 0.f},
                      {REV_MIX, 0.f}, {AENV_S, 1.f}, {DRIVE, 0.f}, {SATURATE, 0.f}},
                     Scale(36, 108, 0.08f, 0.07f), 6.5f, true});

    // Pulse with LFO PWM.
    tests.push_back({"pwm",
                     {{OSC1_WAVE, .5f}, {OSC2_LEVEL, 0.f}, {LFO_PWM, .8f}, {LFO_RATE, .4f},
                      {CUTOFF, .8f}, {FENV_AMT, .5f}, {REV_MIX, 0.f}},
                     Chord(0.1f, 3.5f, {48}), 4.f, true});

    // Self-oscillating filter swept by its envelope, no oscillators.
    tests.push_back({"res_whistle",
                     {{OSC1_LEVEL, 0.f}, {OSC2_LEVEL, 0.f}, {RESONANCE, 1.f}, {CUTOFF, .3f},
                      {FENV_AMT, .9f}, {FENV_A, .6f}, {FENV_D, .7f}, {FENV_S, 0.f}, {REV_MIX, 0.f}},
                     Chord(0.1f, 3.0f, {60}), 4.f, true});

    // The classic: sync lead with the filter envelope sweeping osc 2.
    tests.push_back({"sync_lead",
                     {{OSC1_LEVEL, 0.f}, {OSC2_LEVEL, 1.f}, {SYNC, 1.f}, {SWEEP, .6f},
                      {FENV_D, .55f}, {FENV_S, .1f}, {CUTOFF, .7f}, {GLIDE, .3f}, {DLY_MIX, .3f}},
                     {{0.1f, 0.6f, 60, .9f}, {0.6f, 1.1f, 63, .9f}, {1.1f, 2.4f, 67, .9f}}, 3.5f, true});

    // Fat poly brass with chorus and reverb.
    tests.push_back({"poly_brass",
                     {{OSC2_LEVEL, .8f}, {OSC3_LEVEL, .5f}, {CHORUS, .7f}, {REV_MIX, .3f},
                      {FENV_A, .35f}, {FENV_D, .5f}, {FENV_S, .4f}, {CUTOFF, .45f}, {FENV_AMT, .75f}},
                     Chord(0.1f, 2.0f, {48, 55, 60, 64, 67}), 4.f, false});

    // Cross-mod bell.
    tests.push_back({"xmod_bell",
                     {{OSC1_LEVEL, .2f}, {OSC2_LEVEL, 1.f}, {OSC2_OCT, 1.f}, {OSC2_WAVE, 1.f},
                      {XMOD, .6f}, {AENV_S, 0.f}, {AENV_D, .7f}, {AENV_R, .6f}, {CUTOFF, .9f},
                      {REV_MIX, .4f}},
                     Chord(0.1f, 0.3f, {72}), 3.f, false});

    // Unison mono bass with drive and saturation.
    tests.push_back({"unison_bass",
                     {{UNISON, .6f}, {DRIVE, .7f}, {SATURATE, .6f}, {CUTOFF, .35f}, {RESONANCE, .5f},
                      {FENV_D, .35f}, {FENV_S, 0.f}, {REV_MIX, 0.f}},
                     {{0.1f, 0.4f, 36, 1.f}, {0.5f, 0.8f, 36, .6f}, {0.9f, 1.2f, 39, 1.f}, {1.3f, 1.9f, 31, 1.f}},
                     2.5f, true});

    // Arpeggiator, up/down over two octaves.
    tests.push_back({"arp",
                     {{ARP_MODE, StepValue(3, 6)}, {ARP_RANGE, StepValue(1, 3)}, {ARP_TEMPO, .45f},
                      {AENV_D, .3f}, {AENV_S, .2f}, {DLY_MIX, .35f}, {CHORUS, .4f}},
                     Chord(0.1f, 3.5f, {60, 64, 67}), 4.5f, false});

    // Everything maxed at once: must stay finite and under the limiter.
    tests.push_back({"stress",
                     {{OSC2_LEVEL, 1.f}, {OSC3_LEVEL, 1.f}, {OSC4_LEVEL, 1.f}, {NOISE, 1.f},
                      {RESONANCE, 1.f}, {DRIVE, 1.f}, {SATURATE, 1.f}, {XMOD, 1.f}, {SYNC, 1.f},
                      {CHORUS, 1.f}, {DLY_MIX, 1.f}, {DLY_FDBK, 1.f}, {REV_MIX, 1.f}, {REV_SIZE, 1.f},
                      {LFO_CUTOFF, 1.f}, {LFO_PITCH, 1.f}, {LFO_PWM, 1.f}, {SWEEP, 1.f}},
                     Chord(0.1f, 3.f, {24, 36, 48, 60, 72, 84, 96}, 1.f), 4.f, false});

    for(auto& t : tests)
        Run(t, dir);
    return 0;
}
