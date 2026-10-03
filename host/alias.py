"""Aliasing check: in saw_sweep.wav, measure how much energy sits off the
harmonic series for a few notes. Lower dB = cleaner. A naive (non-band-limited)
saw at the same pitch is shown for comparison."""
import sys, wave, numpy as np
path = sys.argv[1] if len(sys.argv) > 1 else "out/saw_sweep.wav"
w = wave.open(path); sr = w.getframerate()
x = np.frombuffer(w.readframes(w.getnframes()), dtype=np.int16).reshape(-1, 2)[:, 0] / 32768.0

def alias_db(sig, f0):
    n = len(sig); win = np.blackman(n)
    spec = np.abs(np.fft.rfft(sig * win)) ** 2
    freqs = np.fft.rfftfreq(n, 1 / sr)
    harm = np.zeros_like(spec, dtype=bool)
    k = 1
    while k * f0 < sr / 2:
        harm |= np.abs(freqs - k * f0) < 3 * sr / n
        k += 1
    audible = freqs < 16000
    return 10 * np.log10(spec[~harm & audible].sum() / spec[harm & audible].sum())

for note in (72, 84, 96, 102):
    f0 = 440 * 2 ** ((note - 69) / 12)
    t0 = 0.1 + (note - 36) * 0.08 + 0.02
    seg = x[int(t0 * sr): int((t0 + 0.04) * sr)]
    t = np.arange(len(seg)) / sr
    naive = 2 * ((t * f0) % 1) - 1
    print(f"note {note} ({f0:7.1f} Hz): synth {alias_db(seg, f0):6.1f} dB   naive saw {alias_db(naive, f0):6.1f} dB")
