# Golden files

Raw little-endian float32, one second, mono, named `<node>_<rate>.f32`. Stimulus and comparison
rule: `GoldenFile.hpp`. Tolerance: RMS residual <= -80 dB.

| File | Pinned by | Reason |
|---|---|---|
| tube_screamer_48k/96k | D6a Task 3, 2026-08-26 | first pin; D5b behaviour |
| jcm800_48k/96k | D6a Task 3, 2026-08-26 | first pin; D5b behaviour |
| all four | D6a Task 4, 2026-08-26 | oversampler redesign (spec D6a.3), deliberate behaviour change; alias floors before/after: TubeScreamer -8.3 -> -9.6 dB, Jcm800 -3.8 -> -32.0 dB |
| jcm800_48k/96k | D6a Task 5, 2026-08-26 | tone stack moved into the single 8x region (spec D6a.3); Jcm800 alias floor -32.0 -> -20.4 dB |
| jcm800_48k/96k | D6a Task 5 fix, 2026-08-26 | 12AX7 Miller input capacitance on the V1b/V2a/PI grids, the interstage bandwidth the single 8x region exposed as missing; Jcm800 alias floor -20.4 -> -43.4 dB |
| mesa_mark2c_48k/96k | DA2 Task 5, 2026-09-06 | first pin; Mark IIC+ lead mode after fix round 1 (`8c1ef6b`) |
| diezel_vh4_48k/96k | DA2 Task 6, 2026-09-06 | first pin; VH4 channel 3 after fix round 2 (`c75f393`) |
| peavey5150_48k/96k | DA2 Task 4, 2026-09-06 | first pin; 5150 lead after fix round 2 (`d877855`) |

Regenerate with `SUSHIDSP_REGENERATE_GOLDEN=1 sd test -s regression` and add a row here.
