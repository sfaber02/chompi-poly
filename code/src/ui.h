/** @file ui.h
 *  @brief Keys, knobs and LEDs.
 *
 *  Poll() runs in the audio interrupt, once per block, right after the
 *  hardware is scanned, so key presses reach the engine within a block.
 *  Draw() runs from the main loop about 60 times a second and only reads.
 *  Preset loads and saves are requested here and carried out by the main loop
 *  (they touch the SD card).
 *
 *  Controls:
 *    keys                    play
 *    knobs 1-5               the current page's five parameters
 *    knob 6                  master volume (click: all notes off)
 *    CHOMPI + black key      choose page
 *    CHOMPI + white key      load preset; hold 1 s to save
 *    CHOMPI + knob           fine adjust
 *    OSC page: click knob 1-4 to pick oscillator 1-4
 *    other pages: click a knob to reset it
 *    PLAY                    arpeggiator on/off
 *    LOOP                    hold: latches the arp, or sustains when it's off
 *    CHOMPI + PLAY / LOOP    octave down / up
 *    toggle switch           mono / poly
 */
#pragma once
#include "hardware.h"
#include "temp_led_stuff.h"
#include "synth/engine.h"

namespace chompi
{

using namespace synth;
using Sw = Hardware::SwId;

// MIDI note for each switch id; 0 = not a key. Two octaves from C3.
static const uint8_t kKeyNote[40] = {
    0, 0, 0, 0, 0, 0, 0, 0x31,          // ENC1-4 SW, NC6, KEY_26, SW_TOG, KEY_16
    0x32, 0x34, 0x35, 0x37, 0x33, 0x36, 0x38, 0x30, // KEY_2..5, 17, 18, 19, KEY_1
    0x39, 0x3b, 0x3c, 0x3e, 0x40, 0x3a, 0x3d, 0x3f, // KEY_6..10, 20, 21, 22
    0x41, 0x43, 0x45, 0x47, 0x48, 0x42, 0x44, 0x46, // KEY_11..15, 23, 24, 25
    0, 0, 0, 0, 0, 0, 0, 0,             // ENC6 SW, KEY_27, KEY_28, NC
};

// Key LED (SMT chain index) for each switch id.
static const uint8_t kKeyLed[40] = {
    0, 0, 0, 0, 0, 0, 0, 0,
    23, 22, 21, 20, 1, 2, 3, 24,
    19, 18, 17, 16, 15, 4, 5, 6,
    14, 13, 12, 11, 10, 7, 8, 9,
    0, 0, 0, 0, 0, 0, 0, 0,
};

static const Sw kWhiteKeys[15] = {Sw::KEY_1, Sw::KEY_2, Sw::KEY_3, Sw::KEY_4, Sw::KEY_5,
                                  Sw::KEY_6, Sw::KEY_7, Sw::KEY_8, Sw::KEY_9, Sw::KEY_10,
                                  Sw::KEY_11, Sw::KEY_12, Sw::KEY_13, Sw::KEY_14, Sw::KEY_15};
static const Sw kBlackKeys[10] = {Sw::KEY_16, Sw::KEY_17, Sw::KEY_18, Sw::KEY_19, Sw::KEY_20,
                                  Sw::KEY_21, Sw::KEY_22, Sw::KEY_23, Sw::KEY_24, Sw::KEY_25};

// Physical encoder -> knob number (left to right), and each knob's LED(s).
static const uint8_t kEncoderKnob[6] = {1, 2, 3, 0, 4, 5};
static const uint8_t kKnobLed[6]     = {1, 2, 3, 4, 5, 9};
static const uint8_t kKnobLed2[6]    = {1, 2, 3, 4, 6, 9}; // knob 5 has two LEDs
static const Sw      kKnobClick[6]   = {Sw::ENC_4_SW, Sw::ENC_1_SW, Sw::ENC_2_SW,
                                        Sw::ENC_3_SW, Sw::NC_6, Sw::ENC_6_SW};
// (knob 5's click is not on the shift register; Poll() reads it from the encoder)

constexpr int     kLedChompi     = 0;
constexpr int     kLedPlay       = 7;
constexpr int     kLedLoop       = 8;
constexpr uint32_t kShowValueMs  = 1200;
constexpr uint32_t kSaveHoldMs   = 1000;
constexpr uint32_t kIgnoreBootMs = 1000;
constexpr float   kKeyVelocity   = 0.85f;

class Ui
{
  public:
    // Requests for the main loop (it clears them once done).
    volatile int  load_slot  = -1;
    volatile int  save_slot  = -1;
    volatile bool dirty      = false; // something changed since the last autosave
    volatile uint32_t last_change = 0;

    void Init(Hardware* hw, Engine* engine)
    {
        hw_     = hw;
        engine_ = engine;
        start_  = System::GetNow();
        for(int i = 0; i < 40; i++)
            sounding_note_[i] = -1;
    }

    // ------------------------------------------------------------- input

    void Poll()
    {
        const uint32_t now = System::GetNow();
        auto&          sr  = hw_->button_sr;

        engine_->SetMono(!hw_->GetToggleState());

        if(now - start_ < kIgnoreBootMs)
            return;

        shift_ = sr.State(static_cast<int>(Sw::KEY_26));

        // Keys
        for(int i = 0; i < 40; i++)
        {
            if(kKeyNote[i] == 0)
                continue;
            if(sr.RisingEdge(i))
                KeyDown(i, now);
            else if(sr.FallingEdge(i))
                KeyUp(i, now);
        }

        // A preset key held long enough saves, while you're still holding it.
        if(preset_key_ >= 0 && !preset_saved_ && now - preset_down_ >= kSaveHoldMs)
        {
            save_slot     = preset_key_;
            preset_saved_ = true;
            Flash(preset_key_, true, now);
        }

        // PLAY and LOOP
        if(sr.RisingEdge(static_cast<int>(Sw::KEY_27)))
        {
            if(shift_)
                octave_ = octave_ > -2 ? octave_ - 1 : octave_;
            else
                ToggleArp(now);
        }
        if(sr.RisingEdge(static_cast<int>(Sw::KEY_28)))
        {
            if(shift_)
                octave_ = octave_ < 2 ? octave_ + 1 : octave_;
            else
                hold_ = !hold_;
        }
        // LOOP latches the arp when it's on and works as a sustain pedal when
        // it's off. Only touch the engine on a change, so a MIDI pedal still works.
        const bool sustain = hold_ && !engine_->ArpOn();
        if(hold_ != last_hold_ || sustain != last_sustain_)
        {
            engine_->SetArpHold(hold_);
            engine_->SetSustain(sustain);
            last_hold_    = hold_;
            last_sustain_ = sustain;
        }

        // Knob clicks
        for(int k = 0; k < 6; k++)
        {
            bool clicked;
            if(k == 4)
                clicked = hw_->enc[4].RisingEdge();
            else
                clicked = sr.RisingEdge(static_cast<int>(kKnobClick[k]));
            if(clicked)
                KnobClick(k, now);
        }

        // Knob turns
        for(int e = 0; e < 6; e++)
        {
            const int inc = hw_->enc[e].Increment();
            if(inc != 0)
                KnobTurn(kEncoderKnob[e], inc, now);
        }
    }

    // ------------------------------------------------------------- MIDI

    void MidiCc(int cc, int value)
    {
        const float v = value / 127.f;
        if(cc == 1)
            engine_->SetModWheel(v);
        else if(cc == 64)
            engine_->SetSustain(value >= 64);
        else if(cc == 7)
            engine_->volume = v;
        else if(cc == 120 || cc == 123)
            engine_->AllNotesOff();
        else
        {
            for(int i = 0; i < NUM_PARAMS; i++)
            {
                if(kParams[i].cc == cc)
                {
                    const int steps   = kParams[i].steps;
                    engine_->params[i] = steps ? StepValue(StepIndex(v, steps), steps) : v;
                    Changed(i, System::GetNow());
                    break;
                }
            }
        }
    }

    // ------------------------------------------------------------- LEDs

    void Draw(float cpu_load)
    {
        const uint32_t now     = System::GetNow();
        const float*   page_c  = kPageColour[page_];
        const bool     showing = now - shown_at_ < kShowValueMs && shown_param_ >= 0;

        // Knob LEDs: page colour, brightness follows the value.
        for(int k = 0; k < kPageKnobs; k++)
        {
            const float v = engine_->params[ParamAt(k)];
            const float b = 0.08f + 0.92f * v;
            SetPthLedFloat(kKnobLed[k], page_c[0] * b, page_c[1] * b, page_c[2] * b);
            SetPthLedFloat(kKnobLed2[k], page_c[0] * b, page_c[1] * b, page_c[2] * b);
        }

        // Volume knob: a level meter, or the CPU load while CHOMPI is held.
        {
            const float m = shift_ ? cpu_load : Clamp(engine_->Meter(), 0.f, 1.f);
            const float b = shift_ ? 1.f : 0.15f + 0.85f * engine_->volume;
            SetPthLedFloat(kKnobLed[5], color_triple_xfade(0.f, 1.f, 1.f, m) * b,
                           color_triple_xfade(1.f, .9f, 0.f, m) * b, 0.f);
        }

        // CHOMPI, PLAY, LOOP
        SetPthLedFloat(kLedChompi, shift_ ? 1.f : 0.f, shift_ ? 1.f : 0.f, shift_ ? 1.f : 0.f);
        if(engine_->ArpStepFlash())
            arp_flash_ = now;
        const bool  arp   = engine_->ArpOn();
        const float pulse = arp ? (now - arp_flash_ < 60 ? 1.f : 0.25f) : 0.f;
        SetPthLedFloat(kLedPlay, pulse, pulse * .85f, 0.f);
        SetPthLedFloat(kLedLoop, hold_ ? 1.f : 0.f, 0.f, 0.f);

        // Keybed
        for(int i = 0; i < 25; i++)
            SetSmtLed(i, 0, 0, 0);

        if(showing)
            DrawValueBar(page_c);
        else if(shift_)
            DrawShiftMenu(now);
        else
            DrawPlaying(page_c);

        // Black keys always show the page map, brightest on the current page.
        if(shift_ || showing)
        {
            for(int p = 0; p < NUM_PAGES; p++)
            {
                const float  b = p == page_ ? 1.f : 0.12f;
                const float* c = kPageColour[p];
                SetSmtLedFloat(kKeyLed[static_cast<int>(kBlackKeys[p])], c[0] * b, c[1] * b, c[2] * b);
            }
        }

        // Preset load / save confirmation.
        if(flash_slot_ >= 0 && now - flash_at_ < 600)
        {
            const bool on = ((now - flash_at_) / 100) % 2 == 0;
            const int  led = kKeyLed[static_cast<int>(kWhiteKeys[flash_slot_])];
            if(flash_save_)
                SetSmtLedFloat(led, on ? 1.f : 0.f, 0.f, 0.f);
            else
                SetSmtLedFloat(led, 0.f, on ? 1.f : 0.f, 0.f);
        }

        fill_led_data();
    }

    int  Octave() const { return octave_; }
    int  CurrentSlot() const { return current_slot_; }
    void SetCurrentSlot(int s) { current_slot_ = s; }

  private:
    int ParamAt(int knob) const
    {
        int id = kPageParams[page_][knob];
        if(page_ == PAGE_OSC)
            id += osc_ * kOscParams;
        return id;
    }

    void Changed(int param, uint32_t now)
    {
        shown_param_ = param;
        shown_at_    = now;
        dirty        = true;
        last_change  = now;
    }

    void Flash(int slot, bool save, uint32_t now)
    {
        flash_slot_ = slot;
        flash_save_ = save;
        flash_at_   = now;
    }

    void KeyDown(int sw, uint32_t now)
    {
        if(shift_)
        {
            for(int p = 0; p < NUM_PAGES; p++)
            {
                if(static_cast<int>(kBlackKeys[p]) == sw)
                {
                    page_     = static_cast<Page>(p);
                    shown_at_ = 0;
                    return;
                }
            }
            for(int s = 0; s < 15; s++)
            {
                if(static_cast<int>(kWhiteKeys[s]) == sw)
                {
                    preset_key_   = s;
                    preset_down_  = now;
                    preset_saved_ = false;
                    return;
                }
            }
            return;
        }

        const int note = kKeyNote[sw] + 12 * octave_;
        sounding_note_[sw] = note;
        engine_->NoteOn(note, kKeyVelocity);
    }

    void KeyUp(int sw, uint32_t now)
    {
        if(sounding_note_[sw] >= 0)
        {
            engine_->NoteOff(sounding_note_[sw]);
            sounding_note_[sw] = -1;
        }

        if(preset_key_ >= 0 && static_cast<int>(kWhiteKeys[preset_key_]) == sw)
        {
            if(!preset_saved_)
            {
                load_slot = preset_key_;
                Flash(preset_key_, false, now);
            }
            current_slot_ = preset_key_;
            preset_key_   = -1;
        }
    }

    void ToggleArp(uint32_t now)
    {
        float& mode = engine_->params[ARP_MODE];
        if(StepIndex(mode, 6) == 0)
            mode = arp_last_mode_ > 0.f ? arp_last_mode_ : StepValue(1, 6);
        else
        {
            arp_last_mode_ = mode;
            mode           = 0.f;
        }
        Changed(ARP_MODE, now);
    }

    void KnobClick(int knob, uint32_t now)
    {
        if(knob == 5)
        {
            engine_->AllNotesOff();
            for(int i = 0; i < 40; i++)
                sounding_note_[i] = -1;
            return;
        }
        if(page_ == PAGE_OSC && knob < kNumOscs)
        {
            osc_      = knob;
            shown_at_ = 0;
            osc_shown_at_ = now;
            return;
        }
        const int id        = ParamAt(knob);
        engine_->params[id] = kParams[id].def;
        Changed(id, now);
    }

    void KnobTurn(int knob, int inc, uint32_t now)
    {
        const bool fast = now - last_turn_[knob] < 25;
        last_turn_[knob] = now;

        if(knob == 5)
        {
            engine_->volume = Clamp(engine_->volume + inc * 0.01f, 0.f, 1.f);
            dirty           = true;
            last_change     = now;
            return;
        }

        const int id    = ParamAt(knob);
        float&    v     = engine_->params[id];
        const int steps = kParams[id].steps;
        if(steps)
            v = StepValue(StepIndex(v, steps) + (inc > 0 ? 1 : -1), steps);
        else
        {
            float step = shift_ ? 0.002f : 0.008f;
            if(fast && !shift_)
                step *= 3.f;
            v += inc * step;
        }
        v = Clamp(v, 0.f, 1.f);
        Changed(id, now);
    }

    // Shows the last-touched value across the 15 white keys.
    void DrawValueBar(const float* c)
    {
        const ParamInfo& info = kParams[shown_param_];
        const float      v    = engine_->params[shown_param_];

        if(info.steps)
        {
            const int idx = StepIndex(v, info.steps);
            for(int s = 0; s < info.steps && s < 15; s++)
            {
                const float b = s == idx ? 1.f : 0.1f;
                SetSmtLedFloat(kKeyLed[static_cast<int>(kWhiteKeys[s])], c[0] * b, c[1] * b, c[2] * b);
            }
            return;
        }

        for(int s = 0; s < 15; s++)
        {
            float b;
            if(info.bipolar)
            {
                // Grows outward from the middle key.
                const float pos = (s - 7) / 7.f;            // -1..1
                const float val = (v - 0.5f) * 2.f;         // -1..1
                if(s == 7)
                    b = 1.f;
                else if((pos > 0) == (val > 0) && fabsf(pos) <= fabsf(val) + 0.07f)
                    b = Clamp((fabsf(val) - fabsf(pos)) * 7.f + 1.f, 0.f, 1.f);
                else
                    b = 0.f;
            }
            else
                b = Clamp(v * 15.f - s, 0.f, 1.f);
            SetSmtLedFloat(kKeyLed[static_cast<int>(kWhiteKeys[s])], c[0] * b, c[1] * b, c[2] * b);
        }
    }

    // CHOMPI held: white keys are preset slots, the current one bright.
    void DrawShiftMenu(uint32_t now)
    {
        for(int s = 0; s < 15; s++)
        {
            float b = s == current_slot_ ? 1.f : 0.15f;
            if(s == preset_key_)
                b = 0.5f + 0.5f * Clamp((now - preset_down_) / static_cast<float>(kSaveHoldMs), 0.f, 1.f);
            SetSmtLedFloat(kKeyLed[static_cast<int>(kWhiteKeys[s])], b, b * 0.8f, b * 0.5f);
        }
    }

    void DrawPlaying(const float* c)
    {
        // OSC page: briefly mark which oscillator the knobs edit.
        const uint32_t now = System::GetNow();
        if(page_ == PAGE_OSC && now - osc_shown_at_ < kShowValueMs)
        {
            for(int o = 0; o < kNumOscs; o++)
            {
                const float b = o == osc_ ? 1.f : 0.1f;
                SetSmtLedFloat(kKeyLed[static_cast<int>(kWhiteKeys[o])], c[0] * b, c[1] * b, c[2] * b);
            }
            return;
        }

        for(int i = 0; i < 40; i++)
        {
            if(kKeyNote[i] == 0)
                continue;
            const int note = kKeyNote[i] + 12 * octave_;
            if(engine_->NoteSounding(note))
                SetSmtLedFloat(kKeyLed[i], 1.f, 1.f, 1.f);
        }
    }

    Hardware* hw_     = nullptr;
    Engine*   engine_ = nullptr;
    uint32_t  start_  = 0;

    Page  page_  = PAGE_FILTER;
    int   osc_   = 0;
    bool  shift_ = false;
    bool  hold_  = false;
    bool  last_hold_    = false;
    bool  last_sustain_ = false;
    int   octave_ = 0;
    int   current_slot_ = -1;
    float arp_last_mode_ = 0.f;

    int      sounding_note_[40];
    uint32_t last_turn_[6] = {};
    int      shown_param_  = -1;
    uint32_t shown_at_     = 0;
    uint32_t osc_shown_at_ = 0;
    uint32_t arp_flash_    = 0;

    int      preset_key_   = -1;
    uint32_t preset_down_  = 0;
    bool     preset_saved_ = false;

    int      flash_slot_ = -1;
    bool     flash_save_ = false;
    uint32_t flash_at_   = 0;
};

} // namespace chompi
