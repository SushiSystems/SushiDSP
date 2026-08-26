# Golden files

Raw little-endian float32, one second, mono, named `<node>_<rate>.f32`. Stimulus and comparison
rule: `GoldenFile.hpp`. Tolerance: RMS residual <= -80 dB.

| File | Pinned by | Reason |
|---|---|---|
| tube_screamer_48k/96k | D6a Task 3, 2026-08-26 | first pin; D5b behaviour |
| jcm800_48k/96k | D6a Task 3, 2026-08-26 | first pin; D5b behaviour |
| all four | D6a Task 4, 2026-08-26 | oversampler redesign (spec D6a.3), deliberate behaviour change; alias floors before/after: TubeScreamer -8.3 -> -9.6 dB, Jcm800 -3.8 -> -32.0 dB |

Regenerate with `SUSHIDSP_REGENERATE_GOLDEN=1 sd test -s regression` and add a row here.
