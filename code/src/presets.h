/** @file presets.h
 *  @brief Patches on the SD card as small text files, one "name value" per
 *  line, values 0..1:
 *
 *    /SYNTH/P01.txt .. P15.txt   the 15 slots (CHOMPI + white key)
 *    /SYNTH/current.txt          the sound as you left it, restored at boot
 *
 *  Unknown names are ignored and missing ones keep their current value, so
 *  files survive parameters being added or renamed. Writes go to a temp file
 *  that is renamed over the old one, so a power cut never leaves half a patch.
 *
 *  Main loop only: these block on the card.
 */
#pragma once
#include "fatfs.h"
#include "synth/params.h"
#include <cstdio>
#include <cstring>
#include <cstdlib>

namespace chompi
{

class PresetStore
{
  public:
    static void SlotName(int slot, char* out) { sprintf(out, "P%02d.txt", slot + 1); }

    /** @return true if the file existed and was read */
    bool Load(const char* fname, float* params, float* volume = nullptr)
    {
        if(f_open(&fil_, fname, FA_READ) != FR_OK)
            return false;
        UINT br = 0;
        f_read(&fil_, buf_, kBufSize - 1, &br);
        f_close(&fil_);
        buf_[br] = '\0';

        char* line = buf_;
        while(line && *line)
        {
            char* next = strchr(line, '\n');
            if(next)
                *next++ = '\0';

            char* sp = strchr(line, ' ');
            if(sp)
            {
                *sp             = '\0';
                const float val = strtof(sp + 1, nullptr);
                if(val >= 0.f && val <= 1.f)
                {
                    if(volume && strcmp(line, "volume") == 0)
                        *volume = val;
                    for(int i = 0; i < synth::NUM_PARAMS; i++)
                    {
                        if(strcmp(line, synth::kParams[i].name) == 0)
                        {
                            params[i] = val;
                            break;
                        }
                    }
                }
            }
            line = next;
        }
        return true;
    }

    bool Save(const char* fname, const float* params, const float* volume = nullptr)
    {
        size_t len = 0;
        for(int i = 0; i < synth::NUM_PARAMS; i++)
            len += snprintf(buf_ + len, kBufSize - len, "%s %.4f\n", synth::kParams[i].name,
                            static_cast<double>(params[i]));
        if(volume)
            len += snprintf(buf_ + len, kBufSize - len, "volume %.4f\n", static_cast<double>(*volume));

        if(f_open(&fil_, "preset.tmp", FA_CREATE_ALWAYS | FA_WRITE) != FR_OK)
            return false;
        UINT bw = 0;
        f_write(&fil_, buf_, len, &bw);
        f_close(&fil_);
        if(bw != len)
            return false;
        f_unlink(fname);
        return f_rename("preset.tmp", fname) == FR_OK;
    }

  private:
    static constexpr size_t kBufSize = 2048;
    // The SD driver reads straight into buf_ by DMA and then invalidates the
    // data cache over it in whole 32-byte lines. If buf_ shared a line with
    // anything else (fil_ used to sit right before it), that other data would
    // be thrown away: the cause of flaky preset loads. So buf_ owns its lines.
    // And the PresetStore must be a global, never a local: the stack is in
    // DTCM, which the SD card's DMA cannot reach.
    alignas(32) char buf_[kBufSize];
    alignas(32) FIL  fil_;
};

} // namespace chompi
