# benchmark_harness

Internal module: the timing harness the benchmark program is built from. Its headers are never
installed, and no product links it.

It depends on `core`, for the `AudioNode` a node case drives.

A unit's benchmark sits beside its tests, in `tests/<tier>/<module>/benchmark_<name>.cpp`, and
registers its cases with a static `BenchmarkRegistration`. `tests/benchmark_main.cpp` runs every
registered case, so adding a benchmark adds one file and one line in `tests/CMakeLists.txt`.

## benchmark_harness.hpp

`measure` reports the minimum over its timed repetitions, not the mean. On a desktop operating
system the variance is scheduler noise stacked on the true cost, so the minimum is the
least-contaminated estimate of what the audio thread pays. `print` writes one table row.
`K_SAMPLE_RATE` and `K_BLOCK_SIZE` are the rate and block every case is costed at.

## reference_tone.hpp

`reference_tone` returns the 220 Hz tone at a quarter of full scale that the regression
snapshots use, so a case exercises the path the goldens hold.

## node_benchmark.hpp

`measure_node` prepares a mono node, lets the caller set its knobs, and times `process()` over
one block of the reference tone.

## solver_counters.hpp

`print_solver_counters` prints a circuit's solves, non-converged solves, worst iteration count
and NaN fallbacks under its row. A solver's cost and its convergence are one measurement: a
block that got fast by giving up early is not faster. `measure` replays the same 256-frame
buffer, so the tone jumps back to sample 0 once per repetition, the one thing a real stream
never does.

## benchmark_registry.hpp

`BenchmarkRegistration` adds a named group of cases to `registered_cases()`. `run_selected` runs
every group whose name holds one of the program's arguments, or every group when there is none,
ordered by group name.
