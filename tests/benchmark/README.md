# tests/benchmark

The benchmark executable. `benchmark.hpp` is the timing harness and `main.cpp` holds the cases,
each of which prints one table row.

## benchmark.hpp

`measure` reports the minimum over its timed repetitions, not the mean. On a desktop operating
system the variance is scheduler noise stacked on the true cost, so the minimum is the
least-contaminated estimate of what the audio thread pays.

## main.cpp

### The JCM800 V1a block

The netlist, its component constants and the triode glue live in
`tests/circuits/jcm_v1a_netlist.hpp`, shared with the D6b stage test and the RT-safety traps.
`build_jcm_v1a(netlist, true)` appends the series RC that takes the block from the SPICE
stage's six reactive states to the seven the D6b budget is written against.

### The Tube Screamer blocks

The Tube Screamer's two blocks come from the product module, because the circuits are the
pedal. The bench prices exactly the description Task 7's node will run.

### `measure_dk_circuit`

The solver statistics are cleared after the settle and never again. The counters printed after
the run therefore cover exactly the solves `measure()` drives: every warm-up repetition and
every timed one, 2048 per repetition. `total_samples()` is the circuit's own count, so the
reported rate cannot drift from the harness's repetition constants.

The interelectrode capacitances make the buffer wrap cost no solve, so the non-converged
count this case prints is zero. `tests/functional/unit/test_dk_triode_stage.cpp` measures the
figure for a real stream.

### `measure_dk_potentiometer_rederivation`

Plan Ruling 3 lets a pot move re-derive the DK matrices on the audio thread at block cadence, so
the cost of one re-derivation is the number the ruling rests on.
`measure_dk_potentiometer_update` cannot supply it. A re-derivation is under one percent of the
block it sits in, so subtracting two block timings would report scheduler noise. This case
calls `set_potentiometer` and `update_matrices_if_dirty` 256 times and nothing else, so the
harness's per-sample column reads directly as nanoseconds per re-derivation.

### `measure_resampler_round_trip`

48 kHz down to 20 kHz and back is the resampler pair's worst case. The decimating stage's half
span is 34 / (20000 / 48000) = 81.6 input samples, so `interpolate()` runs two loops of 82 and
touches 164 taps per output. Both resamplers are prepared outside the timed body, since
`prepare()` is where every buffer is allocated.

The case prices the resampler pair in isolation at that clock. It does not decompose the DD-3's
own number. The node pushes 256 single frames per block where this case pushes one block of
256, and the node's clock follows D.TIME instead of sitting at 20 kHz, so the kernel span
differs as well. That is why the 0.10 ms the case prints against is advisory.

### `measure_room_beside_jcm800`

The graph's own per-block cost is not the sum of its nodes' isolated costs, because the gather
and the buffer pool add their own share. The room's reported share is therefore the whole chain
less the amplifier measured on its own. That is the best estimate the harness can give without
instrumenting `RigGraph::process()` itself.
