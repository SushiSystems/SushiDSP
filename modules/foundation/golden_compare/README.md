# golden_compare

Internal module: the stimulus and the residual comparison that the golden-file regression tests
use. Its headers are never installed, and no product links it.

The reference `.f32` files sit in the `goldens/` folder of the unit whose regression they pin, for
example `tests/nodes/jcm800/goldens/jcm800/`. Their record is the table below.

## golden_file.hpp

`stimulus(sample_rate)` is the one-second input every golden file was rendered from.
`compare_or_regenerate(directory, name, rendered)` compares `rendered` with `<directory>/<name>.f32`
and returns the RMS residual in dB. The caller passes the folder that holds the file. Setting
`SUSHIDSP_REGENERATE_GOLDEN` rewrites the file instead.

The module depends on `log` and logs under `K_GOLDEN_COMPARE_LOG_CATEGORY`, declared in
`golden_compare_log_category.hpp`. A file it cannot open or that holds too few samples is an
`ERROR` and reads as a 0 dB residual; a regeneration is an `INFO`, shown with
`SD_LOG_LEVEL=warn,golden_compare=info`.

## Pins

Raw little-endian float32, one second, mono, named `<node>_<rate>.f32`. Stimulus and comparison
rule: `golden_file.hpp`. Tolerance: RMS residual <= -80 dB.

| File | Pinned by | Reason |
|---|---|---|
| tube_screamer_48k/96k | D6a Task 3, 2026-08-26 | first pin; D5b behaviour |
| jcm800_48k/96k | D6a Task 3, 2026-08-26 | first pin; D5b behaviour |
| all four | D6a Task 4, 2026-08-26 | oversampler redesign (spec D6a.3), deliberate behaviour change; alias floors before/after: TubeScreamer -8.3 -> -9.6 dB, Jcm800 -3.8 -> -32.0 dB |
| jcm800_48k/96k | D6a Task 5, 2026-08-26 | tone stack moved into the single 8x region (spec D6a.3); Jcm800 alias floor -32.0 -> -20.4 dB |
| jcm800_48k/96k | D6a Task 5 fix, 2026-08-26 | 12AX7 Miller input capacitance on the V1b/V2a/PI grids, the interstage bandwidth the single 8x region exposed as missing; Jcm800 alias floor -20.4 -> -43.4 dB |
| mesa_mark2c_48k/96k | DA2 Task 5, 2026-09-06 | first pin; Mark IIC+ lead mode after fix round 1 (`8c1ef6b`) |
| diezel_vh4_48k/96k | DA2 Task 6, 2026-09-06 | first pin; VH4 channel 3 after fix round 2 (`c75f393`) |
| dd3_48k/96k | DR2 Task 2.6, 2026-09-07 | first pin; DD-3 variable-rate loop with the 8x-substepped NE570 rectifier (`9ba392a`), mode M / TIME 0.4 / F.BACK 0.45 |
| peavey5150_48k/96k | DA2 Task 4, 2026-09-06 | first pin; 5150 lead after fix round 2 (`d877855`) |
| diezel_vh4_48k/96k | DA3 final fix, 2026-09-06 | input low-pass removed (`9c7c66d`); default peak -6.159 -> -6.044 dBFS |
| dd3_48k/96k | DR2 Task 2.6, 2026-09-07 | first pin; DD-3 variable-rate loop with the 8x-substepped NE570 rectifier (`9ba392a`), mode M / TIME 0.4 / F.BACK 0.45 |
| tube_screamer_48k/96k | D6c Task 8, 2026-09-07 | TubeScreamer rebuilt on the ts_clip and ts_tone DK blocks inside one 4x region (spec D6c); the memoryless clipper, the 20 Hz input biquad and the LP/HP tone crossfade are gone, and K_LEVEL_UNITY_GAIN is re-calibrated 1.1078783 -> 0.6942987 against the measured chain |

| jcm800, peavey5150, mesa_mark2c, diezel_vh4 (48k/96k) | DA4, 2026-09-07 | plate-swing TriodeStage, level staging removed, LTP degeneration, power-grid scale, first-order couplings, corrected long-tailed pair with the shunt form, shared AC tail and local mu (spec `docs/archive/superpowers/specs/2026-09-07-triode-plate-swing-design.md`); JCM800 alias floor -43.4 -> -6.7 dB, known issue |
| mesa_mark2c_48k/96k | v0.1.0 phase A3, 2026-09-20 | the IIC+'s tone stack is terminated by V1A's 44 k plate impedance and Volume 1's 1 M, which the model omitted; residual against the old pin -28.2 dB, default peak still inside the -7 to -5 dBFS window so K_MASTER_UNITY_GAIN is unchanged (plan `docs/agent/2026_09_20_V0_1_0_RELEASE/PLAN.md`) |

Regenerate one node with `SUSHIDSP_REGENERATE_GOLDEN=1 sd test --filter '<Suite>\..*Golden'`, or
every node with `--label medium`, and add a row here. Confirm with `git status` that only the
intended files moved. On
2026-09-20 the FmvToneStack netlist moved jcm800, peavey5150 and diezel_vh4 by -149 to -156 dB,
float32 rounding against a -80 dB rule, and their pins were kept.
