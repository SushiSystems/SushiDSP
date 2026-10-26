# `modules/foundation/profile`

SushiDSP's front over SushiProfiler: the `SD_ZONE` macros, the session a product or a benchmark
starts, the record it gets back and the names of the timelines, zones and counters. It is the
only code below the products that includes a SushiProfiler header, and its public headers include
none: `ProfileSession` and `ProfileRecord` hold SushiProfiler's objects behind a private
implementation.

## Owns

- `modules/foundation/profile/include/SushiDSP/foundation/profile/profile.hpp`
- `modules/foundation/profile/include/SushiDSP/foundation/profile/profile_record.hpp`
- `modules/foundation/profile/include/SushiDSP/foundation/profile/profile_run.hpp`
- `modules/foundation/profile/include/SushiDSP/foundation/profile/profile_runtime.hpp`
- `modules/foundation/profile/include/SushiDSP/foundation/profile/profile_session.hpp`
- `modules/foundation/profile/include/SushiDSP/foundation/profile/sushidsp_profile_names.hpp`

## Depends on

SushiProfiler, the version `cli/sushistack.deps.toml` pins. `sd` resolves its checkout and hands
it to CMake as `SUSHIPROFILER_DIR`. The choice of target is per build: with the cache option
`SD_PROFILER_STATIC` off, every library links the shared `SushiProfiler::SushiProfiler` and a process
has one collector; with it on, every library links SushiProfiler's static target and defines
`SUSHIPROFILER_STATIC`. The plug-in package is built in a tree of its own with it on, so the plug-in
keeps a collector of its own. A library compiled static and linked to the shared profiler would
lose the collector's imported data on Windows.

## The switch

`SD_ENABLE_PROFILER` is defined by the generated `profile_config.hpp`, with `SD_PROFILE_LEVEL`: 1 in
every build but a plug-in package that ships into other people's hosts. With it 0 a zone, a counter and a lane are statements that
do nothing and do not evaluate their arguments, and no SushiProfiler header is included.
`ProfileSession` then refuses to start with `NotBuilt`, and a benchmark times itself under the
steady clock. `SD_PROFILE_LEVEL`, 1 by default, is the level a translation unit compiles in.

## Using it

```cpp
SD_ZONE_GROUP(profile::names::K_PROCESS_ZONE, profile::names::K_SESSION_GROUP);
SD_COUNTER(profile::names::K_CLAMPED_COUNTER, profile::names::K_COUNT_UNIT, Add, samples);
```

Source: not compiled

A zone is one 24-byte store when the gate is open and one relaxed load when it is closed. The
first record of a site registers it under a lock, and the first record of a thread makes its ring,
so a warm-up period precedes any window that is judged, and a callback thread calls
`register_this_thread()` once at the top of its first block.

A product starts a session with `ProfileSession::start_from_environment()`, which reads the file
the variable `SD_PROFILE_CONFIG` names, or with `start_window()` for a benchmark or a test. While
a session runs, `mark_block(frames, rate)` from a device callback opens the next period of the
`dsp.block` timeline with the budget of that block. A node names a zone at run time by the id
`intern_name()` returns, which is made at `compile()` or `prepare()` because it takes a lock.

`run_profiled` runs a body once per period of a timeline and returns the `ProfileRecord`.
The record answers the questions a benchmark or a test asks: the samples of a subject, the
summary of a timeline, how many times a zone ran, the lanes zones ran in and the verdicts of a
checks file.

## The amplifiers and the tube screamer

Each of the four amplifiers writes six zones under a group of its own, `jcm800`, `peavey5150`,
`mesa_mark2c` and `diezel_vh4`: `<module>.input`, `.upsample`, `.oversampled_loop`, `.downsample`,
`.post` and `.clamp`. The Mesa has no linear stage after the decimator, so it writes no `.post` zone.
The tube screamer writes `tubescreamer.input`, `.upsample`, `.oversampled_loop`, `.downsample` and
`.output`. A stage inside the oversampled loop carries no zone: 14 stages over 2048 oversampled
samples would write 28,672 events a block and cost more than the stage, so per-stage cost stays
with the isolated stage benchmarks.

At the end of each block an amplifier adds the Newton solves its tube stages ran to the counters
`newton.solves`, `newton.iteration_caps`, `newton.non_finite_steps` and `newton.worst_iterations`,
and the samples its output clamp cut or found not a number to `output.clamped`. The tube
screamer adds its clip circuit's statistics to `solver.solves`, `solver.non_converged`,
`solver.nan_fallback` and `solver.worst_iterations`. Each counter is recorded in every block,
as 0 when nothing happened.

## What is safe on the audio thread

A zone, a counter, an instant, a lane and `mark_block` after the thread is registered and the site
has been used once. `intern_name`, `ProfileSession` and the record are control-thread calls.
