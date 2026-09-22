# motus_reference.csv

Measured decay figures of one real room: 4.9 x 4.4 x 2.9 m, concrete, V = 61.13 m3, from the
Motus dataset (Zenodo 4923187, licence CC-BY-4.0). Citation: Gotz, Falcon Perez, Schlecht and
Pulkki, I3DA 2021, doi 10.5281/zenodo.4923187. The dataset's small files hold no decay time, so
the figures are computed from three of its ambisonic room impulse responses, source 1
(`0001_1_sh_rirs.wav` empty room, `0002_1_sh_rirs.wav` carpet only, `0003_1_sh_rirs.wav`
carpet and four shelves). The receiver is at [1.825, 2.075, 1.448] m and source 1 at
[3.462, 2.075, 1.448] m, 1.637 m away. One row per octave band per file: T20, T30, EDT, DRR and
C50, all from the omnidirectional channel. An empty `t20_s` or `t30_s` cell means the decay
lacked the range for that fit; no such cell occurs in the committed file.

The method and every parameter are in the docstring of `tools/dsp/motus_reference.py`. To
reproduce, fetch the three members of `sh_rirs.zip` (16.1 GB; a ranged request per member is
enough) and run `python tools/dsp/motus_reference.py --rir 0001_1_sh_rirs.wav
0002_1_sh_rirs.wav 0003_1_sh_rirs.wav --csv tests/fixtures/motus_reference.csv`. The script
needs `numpy` and `scipy` and gives the same bytes on every run. The 125 Hz DRR is not a clean
number: the zero-phase band filter spreads the direct sound over about 11 ms, wider than the
5 ms window.
