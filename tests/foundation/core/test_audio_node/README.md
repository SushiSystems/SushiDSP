# tests/foundation/core/test_audio_node

## `test_rt_safety.cpp`

### `RigGraphStereoProcessPerformsNoHeapAllocation`

`// The graph covers every path of the edge adaptation.`

Exercises every arithmetic path of the edge adaptation: a stereo producer summed into a mono
consumer, a fan-in summed per channel, and a mono sink widened to a stereo host.

### `TubeScreamerProcessPerformsNoHeapAllocation`

`// A moved knob re-derives the matrices inside process().`

Moving a knob before every block is the path the plain loop misses: a wiper that has changed
marks the block dirty and update_matrices_if_dirty() re-derives inside process(), which is where
a solver would allocate if it were going to. This is the D6c instance of the trap D6b's
DkCircuitPotentiometerUpdatePerformsNoHeapAllocation established, now on a shipping node rather
than on a bare circuit. Both knobs sweep their full travel so every derivation sees a genuinely
new matrix.

### `Jcm800ProcessPerformsNoHeapAllocationWithoutCabinet`

`// The per-block set_high_shelf() is pure arithmetic.`

This is also the trap for Jcm800::process()'s per-block set_high_shelf() call: recomputing the
presence shelf's RBJ coefficients every block looks expensive, but it is pure double arithmetic
(pow/sin/cos/sqrt) with no heap, lock, syscall, or throw behind it, so it must not register
here.

### `CabinetNodeProcessPerformsNoHeapAllocationWithCabinet`

`// 500 blocks wrap the spectrum history at least once.`

Cabinet convolution is now sushidsp.cabinet's own surface, not any amp's: it runs
StreamingConvolution's FFT, spectrum history, and overlap-add buffers, all of which must have
been sized in prepare(). 500 blocks is more than the impulse response's partition count, so the
circular spectrum history wraps at least once under the trap.

### `CabinetNodeSetCabinetSwapsWithoutAllocatingOnTheAudioThread`

`// The trap is armed around the blocks, not set_cabinet().`

A cabinet published from the control thread must reach process() through RtSwapSlot's single
atomic acquire - no allocation, no lock. Only the audio thread's half of a swap has to be
RT-safe (publish() builds the convolver and is expected to allocate), so the trap is armed
around the processing blocks and disarmed across each set_cabinet(). That way every trapped
block is one that picks up a freshly published cabinet, which is the case the name promises - a
version of this test that published everything up front would only be proving that convolving
allocates nothing.

### `File scope`

`// The netlist lives in modules/foundation/circuit_blocks/include/SushiDSP/foundation/circuit_blocks/jcm_v1a_netlist.hpp.`

The netlist, its component constants and the triode glue live in
modules/foundation/circuit_blocks/include/SushiDSP/foundation/circuit_blocks/jcm_v1a_netlist.hpp, shared with the D6b stage test and the benchmark.

### `DkCircuitPotentiometerUpdatePerformsNoHeapAllocation`

`// A pot move rebuilds the DK matrices on the audio thread.`

The one audio-thread path that re-runs a derivation (plan Ruling 3): a pot move invalidates the
DK matrices and update_matrices_if_dirty() rebuilds them at the top of the next block. If
anything in that rebuild allocated, Ruling 3 would have to be replaced by a control- thread
rebuild plus an RtSwapSlot hand-off, so this case is the one that decides it.

### `ResamplerProcessPerformsNoHeapAllocation`

`// At the slowest clock interpolate() touches 164 taps.`

The DD-3's decimating leg at the VCO's slowest clock, which is where the Resampler is widest:
the prototype's half width of 34 (input samples; true crossings are fewer at fc = 0.45)
stretches to 34 / (20000 / 48000) = 81.6, so interpolate() runs two loops of 82 and touches 164
taps per output, and prepare() sizes the history ring for exactly that.

### `ResamplerRateChangePerformsNoHeapAllocation`

`// One 256-frame block calls set_rates 32 times.`

The case plan Ruling 3 rests on. The DD-3 re-clocks every 8 frames, so one 256-frame block calls
set_rates 32 times; a ratio change that rebuilt a filter bank would put the node off the audio
thread entirely. The sweep runs the VCO's whole 20-82.5 kHz range, so the ratio reaches both
ends of the clamp prepare() was given.

### `Dd3DelayProcessPerformsNoHeapAllocation`

`// Both MODE changes below move N and pay the std::fill.`

Two things happen inside this trap window that no other node does. process() drives both
Resamplers' set_rates 32 times per block through the 8-frame clock grid, and a MODE change
clears the ring (plan Ruling 14) - once through set_parameter by index, which only records the
mode so that process() applies it at the next block start, and once through set_mode, which
applies it on the spot. Middle holds 4096 samples and Long 16384, so both changes below move N
and both pay the std::fill.

### `TrackRecorderWritePerformsNoHeapAllocation`

`// The trap sees the ring copy and the drop counter.`

The one capture path the audio thread touches. arm() allocates the rings and starts the writer
thread before the trap opens, and the first block is written and drained outside it so that
WavWriter's stream buffer already exists: what the trap then sees is the ring copy and, once the
ring fills, the drop counter.

### `TrackPlayerProcessPerformsNoHeapAllocation`

`// Timelines and converters exist before the trap opens.`

Every timeline is published and every converter sized before the trap window opens, so what is
measured is the mix itself: eight slot acquisitions, eight lanes of sixteen clips walked, the
pan trigonometry, a loop range that splits each block into two runs, and both the unity copy
path and the converted one.

### `TrackPlayerTimelineSwapPerformsNoHeapAllocationOnTheAudioThread`

`// The trap covers the pick-up of a changed lane.`

Every replacement timeline is built before the run, and the window is opened only around
process(), so what is measured is the audio thread picking up a lane that changed under it: one
acquire of a new generation and a converter that has to restart on a clip it was not inside a
block ago.

### `VariRateReaderReadPerformsNoHeapAllocation`

`// The converters are sized before the trap opens.`

The converters are sized before the trap window opens, so what is measured is the read itself: a
set_rates() on every speed change and one push per take frame consumed.

### `MetronomeProcessPerformsNoHeapAllocation`

`// The bursts are built before the trap opens.`

The bursts are built before the trap window opens, so what is measured is the block: two atomic
loads, a beat index and a copy out of a vector already sized.

### `RoomNodeProcessPerformsNoHeapAllocation`

`// The first trapped block adopts the published set.`

prepare() builds the spherical head and publishes the first set outside the trap; the first
trapped block is the one that adopts it, a copy into the node's own bank.

### `RoomNodeProcessPerformsNoHeapAllocationAcrossAPublish`

`// Only the audio thread's half of a publish is trapped.`

Only the audio thread's half of a publish is trapped: set_scene solves and allocates on the
control thread, so the trap closes around it and opens around the blocks that pick the new set
up, start the crossfade and run it to its end. The node renders the microphone, and the four
publishes walk the modal bank through each of its paths: a moved microphone ramps the gains read
at it, a moved listener changes nothing the microphone hears, a changed dimension blends the
spare bank in from zero.

### `ModalBankProcessPerformsNoHeapAllocation`

`// All K_MAX_MODES resonators run at 68.6 Hz, 1 s decay.`

Every one of K_MAX_MODES resonators runs, at the garage's 68.6 Hz mode with a 1 s decay: r =
10^(-3 / 48000) = 0.99985610, a1 = -2 r cos(2 pi 68.6 / 48000), a2 = r^2.

### `CrossoverPairProcessPerformsNoHeapAllocation`

`// set_crossover is arithmetic on the audio thread.`

The pair is placed, run, moved and bypassed inside the trap, as the node does at every adopt;
set_crossover is arithmetic on the audio thread and must stay so.

### `LateReverbProcessPerformsNoHeapAllocation`

`// The trapped blocks include the 512-frame crossfade.`

The second design moves every line by 37 frames, so the trapped blocks include the 512-frame
length crossfade as well as the steady network.

### `BinauralDecoderProcessPerformsNoHeapAllocation`

`// Sixteen blocks cross the 128-frame partition 32 times.`

prepare() builds the 32 partitioned convolvers from the spherical head's decoder outside the
trap; sixteen blocks of 256 frames, which cross the 128-frame partition thirty-two times, run
inside it.

### `RoomNodeBinauralProcessPerformsNoHeapAllocation`

`// The ears render every binaural path inside the trap.`

The ears render every binaural path: the direct pair, the decoder, the diffuse pair and the
alignment delays. set_scene, the direct_delay switch and the fallback's re-install each publish
on the control thread outside the trap; the blocks that adopt each set, fade it in and decode it
run inside, and the index path stores the delay choice alone.
