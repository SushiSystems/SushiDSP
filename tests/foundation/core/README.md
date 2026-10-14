# tests/foundation/core

## `test_denormal_guard.cpp`

### `decay_toward_zero`

`// A volatile read stops the compiler folding the product.`

Reading through a volatile keeps the compiler from folding the product at compile time, where it
would be evaluated under IEEE rules regardless of the runtime FPU mode.

## `test_rt_graph_slot.cpp`

### `CanaryNode`

`/// @brief Declares a node whose virtual ports() returns a fixed, checkable value.`

A minimal node whose ports() is a virtual call returning a fixed, checkable value. Poking it
after the owning RigGraph has (incorrectly) been freed exercises the object's vtable pointer --
freed memory that's since been reused for something else will very likely make this crash
outright (invalid/foreign vtable) rather than just quietly return stale data, which is a much
stronger use-after-free signal than a bare null-check on the RigGraph\*.

### `RtGraphSlotConcurrentPublishAndAcquireDoesNotRace`

`// A virtual call catches a dangling graph pointer.`

Actually dereference the returned graph, through a virtual call, rather than just null-checking
the pointer -- a dangling-but-non-null RigGraph\* sails straight through a null-check. Every
published graph here carries exactly one CanaryNode, so nodes() must come back with exactly one
handle, and that node's ports() (a virtual call) must always report { 1, 1 }. If
acquire_for_audio_thread() ever hands back a pointer to a graph reclaim() already freed, this
either crashes outright (the vtable slot now belongs to whatever reused the memory) or the
checks below fail.

## `test_rt_swap_slot.cpp`

### `Counted`

`/// @brief Declares a payload whose destructor is observable through a live count.`

A payload whose destructor is observable, so the deferred-reclaim contract can be checked by
counting frees rather than by inspecting RtSwapSlot's internals. The canary value is
dereferenced (not just null-checked) by the concurrent case: a dangling-but-non-null pointer
sails straight through a null-check, whereas reading a field of freed memory that has since been
reused shows up as a wrong value or a crash.

### `RtSwapSlotReclaimDestroysRetiredObjectsOnlyOnceAudioThreadHasMovedPast`

`// Generation 1 stays alive while the thread is on it.`

The audio thread is still parked on generation 1, so generation 1's object must stay alive no
matter how often reclaim() runs.

### `RtSwapSlotActiveForPublisherDoesNotAdvanceTheAudioThreadWatermark`

`// Reading the active object must not move the watermark.`

active_for_publisher() exists so the publishing thread can look at the active object without
claiming the audio thread reached it. Reading it must therefore leave reclaim()'s "provably
moved past" bound exactly where it was - if it forged the watermark instead, reclaim() would
free the object an audio thread is still using.

`// The read alone moves nothing.`

The audio thread's last recorded generation is still the second acquire above, so the first
object is genuinely retired here. Re-read it through the publisher accessor and confirm that
read alone did not move anything.

`// Only object 1 may be freed here.`

The audio thread has not acquired since generation 2 was published, so reclaim() may free
nothing published at or after it: object 2 (still the generation the audio thread last recorded)
and object 3 (active) must both survive. Only object 1 may go.

`// Only acquire_for_audio_thread() moves the watermark.`

Once the audio thread does acquire, the earlier object becomes reclaimable - proving the
watermark was still being driven by acquire_for_audio_thread() alone.

## Benchmark: the audio path
The benchmark executable. `benchmark.hpp` is the timing harness and `main.cpp` holds the cases,
each of which prints one table row.

### benchmark_harness.hpp

`measure` reports the minimum over its timed repetitions, not the mean. On a desktop operating
system the variance is scheduler noise stacked on the true cost, so the minimum is the
least-contaminated estimate of what the audio thread pays.

### benchmark_audio_path.cpp

### The JCM800 V1a block

The netlist, its component constants and the triode glue live in
`modules/foundation/circuit_blocks/include/SushiDSP/foundation/circuit_blocks/jcm_v1a_netlist.hpp`, shared with the D6b stage test and the RT-safety traps.
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
count this case prints is zero. `tests/foundation/circuits/stages/test_triode_stage/test_dk_netlist.cpp` measures the
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
