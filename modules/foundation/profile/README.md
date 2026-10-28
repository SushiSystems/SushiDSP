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

SushiProfiler, the version `cli/sushistack.deps.toml` pins. `sd` builds or reuses its package and
hands CMake the package folder as `SushiProfiler_DIR`; the root build finds it with
`find_package(SushiProfiler CONFIG REQUIRED)` while `SD_ENABLE_PROFILER` is on. Every library
links the shared `SushiProfiler::SushiProfiler`, so a process has one collector.

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

Each call site of `SD_ZONE` or `SD_COUNTER` is its own subject, so two sites that share a name give
two subjects of that name. `find()` returns the subject only when exactly one has the name and
null otherwise, and `lookup()` tells a missing name from an ambiguous one and counts the matches.
A record never reads half of a shared name as the whole; give each call site a name of its own.

## The amplifiers and the tube screamer

Each of the four amplifiers writes six zones under a group of its own, `jcm800`, `peavey5150`,
`mesa_mark2c` and `diezel_vh4`: `<module>.input`, `.upsample`, `.oversampled_loop`, `.downsample`,
`.post` and `.clamp`. The Mesa has no linear stage after the decimator, so it writes no `.post` zone.
The Mesa renders a block as 32-frame control periods, so it writes each zone once per control
period, eight times in a 256-frame block.
The tube screamer writes `tubescreamer.input`, `.upsample`, `.oversampled_loop`, `.downsample` and
`.output`. A stage inside the oversampled loop carries no zone: 14 stages over 2048 oversampled
samples would write 28,672 events a block and cost more than the stage, so per-stage cost stays
with the isolated stage benchmarks.

At the end of each block an amplifier adds the Newton solves its tube stages ran to the counters
`newton.solves`, `newton.iteration_caps`, `newton.non_finite_steps` and `newton.worst_iterations`,
and the samples its output clamp cut or found not a number to `output.clamped`; `mesa_mark2c` does
so at the end of each 32-frame control period instead, eight times in a 256-frame block. The tube
screamer adds its clip circuit's statistics to `solver.solves`, `solver.non_converged`,
`solver.nan_fallback` and `solver.worst_iterations`. Each counter is recorded in every block
(every control period in the Mesa), as 0 when nothing happened.

## What is safe on the audio thread

A zone, a counter, an instant, a lane and `mark_block` after the thread is registered and the site
has been used once. `intern_name`, `ProfileSession` and the record are control-thread calls.
