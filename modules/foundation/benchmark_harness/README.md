# benchmark_harness

Internal module: the timing harness the benchmark program is built from. Its headers are never
installed, and no product links it.

It depends on `core`, for the `AudioNode` a node case drives, and on `profile`, for the session
and the record each case runs on.

A unit's benchmark sits beside its tests, in `tests/<tier>/<module>/benchmark_<name>.cpp`, and
registers its cases with a static `BenchmarkRegistration`. `tests/benchmark_main.cpp` runs every
registered case, so adding a benchmark adds one file and one line in `tests/CMakeLists.txt`.

## benchmark_harness.hpp

`measure` runs its body once per period of the timeline `bench.block` inside a profiling session:
20 warm-up periods, then 2000 active ones, each with the budget `samples_per_call / sample_rate`.
It reports the median, not the minimum and not the mean: the median is the steady-state cost the
audio thread pays, and the record keeps the 99th percentile, the maximum and the jitter beside it
so a slow tail is seen rather than hidden. 2000 periods make the 99th percentile the twentieth
largest sample; 200 made it the second largest, which says little. `print` writes one table row
with those figures. `K_SAMPLE_RATE` and `K_BLOCK_SIZE` are the rate and block every case is
costed at.

When no session can start, `measure` falls back to `measure_unrecorded`, which times the same
repetitions under the steady clock and records nothing. That happens in a build with
`SD_ENABLE_PROFILER` off, and when another session already holds the process. A result without a
record is not judged: `print_verdicts` says so and fails nothing.

`print_verdicts` reads a checks file in SushiProfiler's format and prints one line per check with
its verdict and, for a failure, its reason, such as `median is 1852000 ns, above at most 1500000
ns`. `report` prints a row and then the verdicts. A check file sits beside the unit's tests and
holds the budget the row prints, so the printed budget is also the bound that is judged. A failing
check is labelled with the known issue that explains it. A benchmark never fails the run on a
verdict.

`SUSHIDSP_TESTS_DIR`, set by the build, is the folder the checks files are read from.

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
