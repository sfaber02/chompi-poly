CHOMPI SYNTH  beta 1  (build edca5c6)
=====================================

A four-oscillator analog-style synth for the CHOMPI, inspired by the Korg Mono/Poly.
Each voice has four oscillators (saw / pulse / triangle) with sync and cross-mod,
a resonant 24 dB filter, two envelopes, an LFO, chorus, delay, reverb and an
arpeggiator. The toggle switch picks mono (up) or 6-voice poly (down).

BETA: tested on one CHOMPI so far. Back up your card first.


QUICK START (2 minutes)
-----------------------

The one rule: the CHOMPI key (red, top left) is SHIFT. Hold it to get
to everything else.

1. Play some keys. You're on the FILTER page: knob 1 = cutoff, knob 2 =
   resonance. The big purple dial is ALSO cutoff, on every page. Sweep it.

2. Load a patch: hold CHOMPI, tap white key 1. Try keys 1-7.
   (1 sync lead, 2 brass, 3 unison bass, 4 strings, 5 bell, 6 arp, 7 acid,
   15 INIT = one plain oscillator with everything off: start from scratch)

3. Flip the toggle switch: UP = mono (big leads and basses),
   DOWN = poly (chords).

4. Change page: hold CHOMPI. The black keys light up in colours: each one
   is a page. Tap one. The keybed flashes that page's colour, and the
   knob LEDs take its colour, so you always know where you are.

      black keys, left to right:
      OSC  MOD  |  FILTER  F-ENV  A-ENV  |  FX  REVERB  |  LFO  ARP  PERFORM

5. Turn knobs 1-4 on that page. The white keys light up to show the value.
   Hold CHOMPI while turning to reach each knob's second control.

6. On the OSC page, CLICK knob 1, 2, 3 or 4 to pick which of the four
   oscillators you're editing (white keys 1-4 show which). Oscillators 3
   and 4 start silent: turn up their level (knob 3).

7. Press PLAY for the arpeggiator (again to stop). Hold some keys. LOOP
   latches it (again to unlatch). CHOMPI + volume knob = arp tempo.
   Gate (ARP page, knob 4) all the way up = legato.

8. Save: hold CHOMPI, then HOLD a white key for 1 second. It blinks red
   when saved. While CHOMPI is held, the patch you're on is blue and
   slots with a patch glow dim. Your current sound also comes back by itself at power-on.

Lost? Hold CHOMPI and click a knob to reset it. Click the volume knob
(far right) to stop all notes.


INSTALL
-------

You need a card set up with the multi-firmware launcher (v1.1):
https://github.com/sfaber02/CHOMPI/releases

1. Copy FIRMWARE/04_SYNTH.bin into /FIRMWARE on the card. It goes on key 4;
   rename 04 to any free number 1-15 to put it on another key.
2. Copy the SYNTH folder to the root of the card (the factory patches).
3. Power on, press the key.

Or send 04_SYNTH.bin over USB MIDI from the launcher
(https://ugrossek.github.io/CHOMPI/), then copy the SYNTH folder over USB
storage (key 15).


KNOBS (left to right)
---------------------

  1-4  speedometer, two flags, wand   the current page's four controls
       CHOMPI + turn                  each knob's second control
       CHOMPI + click                 reset to default
  5    big purple dial                filter cutoff, always
       CHOMPI + turn                  resonance
  6    volume                         master volume (click: all notes off)
       CHOMPI + turn                  arp tempo while the arp is on,
                                      saturation otherwise

When you turn a knob the white keys show its value:
  bar from the left      normal knob
  bar from the middle    +/- knob (centre = zero)
  one lit key            a stepped choice (wave, octave, shape...)


KEYS
----

  keys                  play
  CHOMPI + black key    choose a page (the keybed flashes its colour)
  CHOMPI + white key    load patch 1-15; HOLD 1 s to save to that slot
  PLAY                  arpeggiator on/off
  LOOP                  latch the arp, or sustain when the arp is off
  CHOMPI + PLAY / LOOP  octave down / up

Your last sound comes back when you power on.


PAGES (black keys, left to right; CHOMPI + turn in brackets)
------------------------------------------------------------

  1  OSC      wave (pulse width) | octave | level (noise) | detune
              click knob 1-4 to pick which oscillator you edit
  2  MOD      sync | cross-mod | sync sweep | glide (unison)
  3  FILTER   cutoff | resonance (drive) | env amount | key tracking
  4  F-ENV    attack (velocity) | decay | sustain | release
  5  A-ENV    attack (velocity) | decay | sustain | release
  6  FX       delay mix | delay time | feedback | chorus (stereo spread)
              mix: middle = 50/50, top = only echoes
              feedback: top = infinite (repeats hold until you turn it down)
  7  REVERB   mix | size | tone | saturation
              mix: middle = 50/50, top = only reverb
  8  LFO      rate | shape | to pitch (to pulse width) | to cutoff
  9  ARP      mode | octave range | tempo | gate
  10 PERFORM  glide (noise) | unison (saturation) | spread (drive) | tune

Patches 1-7: sync lead, brass, unison bass, PWM strings, X-mod bell,
arp pluck, acid. Patch 15: INIT, one plain oscillator (nearly a sine) with
everything else off. Start there to build your own.

MIDI in (DIN and USB): notes with velocity, pitch bend, mod wheel (vibrato),
sustain pedal, and CCs for most parameters (cutoff 74, resonance 71, ...).


Feedback to hiwatts. Built on CHOMPI Club's open-source firmware (MIT).
Community firmware, not an official CHOMPI Club release.
