# `modules/nodes/room/source`

The invariants the room's source files state in one line, each under the name a comment cites.

## The decoder's transforms and cursor

Each convolver transforms its own input, so the sixteen channels are each transformed twice.
That is the unshared figure spec section 10 budgets. Sharing the spectra between the two ears
would change no interface.

In `binaural_decoder.cpp`, `output_partition_[fill_ ..)` holds the result of the previous partition
that has not been emitted yet. One cursor therefore reads an output frame and writes an input
frame.

## A tap set arriving mid-fade

A tap set that arrives while a crossfade is running restarts the fade from the bank that is
active at that moment. The host coalesces edits, so the path never holds more than two sets
(spec section 5.2).

## The tail's delay lines

The feedback delay network emits nothing until its shortest line has run, so that line is what
the handover time bounds. The sum of the sixteen lines stays `K_OVERLAP_RATIO` times the mid-band
decay, which is what fixes the tail's modal density (spec section 6.2).

A room whose handover exceeds its whole delay budget cannot start there: the budget sets the
shortest line and the input delay carries what is left. The shortest line is the network's own
latency, so the delay ahead of it is what is left of the handover time.

A Butterworth pair is power complementary, so the right ear carries the left ear's energy whatever
the split between the coherent and incoherent sums.

## The proximity boost

The proximity boost multiplies the gradient share `(1 - a) cos(theta)` and never the pressure
share `a`. The on-axis far field therefore reads `a + (1 - a) = 1`, and a 5 cm cardioid reads the
15.5 dB of spec section 8.1 at 100 Hz.

## The audio thread's share of a publish

One atomic acquire per slot is the audio thread's whole share of a publish. The set pointer is
compared against the last one the thread saw and is read only inside the block that acquired it.

## The baffle-step shelf

`cabinet_directivity.cpp` puts the shelf's pole at the baffle step and its zero an octave above it,
so the shelf reads 0 dB far below the step and -6 dB far above it. The octave mean samples the
midpoints of `K_OCTAVE_POINTS` equal log-frequency intervals, from the centre over root two to the
centre times root two.

## The interpolation window

In `delay_ring.cpp` the window starts two frames behind the integer delay. The frames whose four
samples end inside the ring are read without the mask, in one run the compiler vectorises; the
few that wrap are read through it. `EarlyReflectionRenderer` then filters four taps side by side,
each lane with the arithmetic one tap alone would run
(`EarlyReflectionRendererRendersATapTheSameAloneAndInAGroup`).

## Loading an HRTF set

`hrtf_set_loader.cpp` lands every field in a local set first and moves it into the caller's set only
on success, so a failure anywhere leaves the caller's set exactly as it was.

## A set the decoder refuses

`RoomNode::set_hrtf()` runs on the control thread. When `BinauralDecoder::prepare` throws on a
set, it logs one `ERROR` under `K_ROOM_LOG_CATEGORY`, declared in `room_log_category.hpp`, with
the set's direction count, the sample rate and the decoder's message, and returns
`HrtfStatus::DecoderRefused` with the HRTF in place untouched. A set at another rate writes one
`WARN` naming both rates and returns `SampleRateMismatch`. A publish that fails after a new set
writes one `WARN` and restores the previous set, and `prepare()` and a direct delay change that
cannot publish write one `ERROR`, because the node then renders silence or keeps the old set.
`ImageSourceSolver::solve` writes one `WARN` when the early taps reach `K_MAX_EARLY_TAPS`, and
`ModalSolver::shell` one when the modes pass `K_MAX_MODES` and the crossover moves down.

## The reported crossover

`modal_solver.cpp` reports the first mode left out as the crossover, so `enumerate()` at that
frequency returns exactly the modes the design carries.

## A patch's area

In `room_scene.cpp` a patch replaces area on its face and does not add to it, so its own material's
area is subtracted from the face before either is weighted.

## Counters, zones and the HRTF status

`RoomNode::counters()` counts the blocks answered with silence because no render set was published,
shared by every room node of the process. `process()` writes the zones `room.tail`, `room.modal` and
`room.pair` once per source, and `room.ears` and `room.microphones` once per block, under the group
`Room`; the names are in `sushidsp_profile_names.hpp`.

`RoomHost::set_hrtf` returns an `HrtfStatus`: `Installed`, `SampleRateMismatch`, `DecoderRefused` or
`PublishRefused`, with `hrtf_status_text` for the sentence. A refused set leaves the HRTF in place.
