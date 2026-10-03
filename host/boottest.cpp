// Boot-silence check: fill the reverb object with junk (as DTCM is after
// another firmware ran), start the engine, play no notes, report the peak.
#include "../code/src/synth/engine.h"
#include <cstdio>
#include <cstring>
using namespace synth;
static Delay::Frame    g_delay[96000];
static daisysp::Reverb g_reverb;
static Engine          g_engine;
int main()
{
    memset((void*)&g_reverb, 0x5A, sizeof g_reverb);
    g_engine.Init(48000.f, g_delay, 96000, &g_reverb);
    g_engine.params[REV_MIX] = 0.6f;
    float l[24], r[24], peak = 0.f;
    for(int b = 0; b < 2000 * 3; b++)
    {
        g_engine.Process(l, r, 24);
        for(int i = 0; i < 24; i++)
            peak = std::max(peak, std::max(fabsf(l[i]), fabsf(r[i])));
    }
    printf("boot peak with no notes: %.6f\n", peak);
    return peak > 0.001f;
}
