# `modules/nodes/capture`, recording and playing back takes

A product-independent module: it models no pedal and no amplifier, and depends only on
`SushiDSP::sushi_dsp`. It moves audio between the rig and the filesystem. Nothing here is an
`AudioNode`.

Intent and the acceptance list live in
the stereo tracks design spec in the version history
§5 for recording and §6 for playback, and in
the arrangement view design spec in the version history
§3 to §5a for musical time, clips and the playback speed. This file records what the code in this
directory does.

## Which class owns what

| Concern | Class |
| --- | --- |
| An in-memory multi-channel recording and its sample rate | `Take` |
| One recording destination: rings, writer thread, drop counter | `TrackRecorder` |
| The processed/dry pair the host arms together | `RecordingBus` |
| The sample clock: position, state, loop range and playback speed | `Transport` |
| The mapping between frames and bars and beats | `MusicalClock` |
| Ownership of every loaded take, and the counts clips borrow against | `TakeLibrary` |
| One lane's clips, sorted and immutable once published | `TrackTimeline` |
| A click on every beat, on the monitor path only | `Metronome` |
| Mixing up to eight clip timelines into one output at that position and speed | `TrackPlayer` |
| Reading a take at a playback speed other than 1.0 | `VariRateReader` |
| Interleaving and the RIFF header | `SushiDSP::WavWriter`, in the core's `io/` |
| Parsing a WAV file back into samples | `SushiDSP::WavFile`, in the core's `io/` |

`TrackRecorder` knows nothing about takes, folders or naming; `RecordingBus` is the only class
here that knows a recording comes in two files, which is why renaming them touches one file.
`TrackPlayer` reads the position out of a `Transport` and never advances it; the host does that
once per block, so the same clock drives the player, the recorder and the UI. `Transport` holds a
position and a speed and nothing musical: bars and beats live in `MusicalClock`, which holds no
position of its own and is handed a frame instead. Either can be replaced without the other
noticing.

## Which thread runs what

The audio thread calls four functions here: `TrackRecorder::write`, `TrackPlayer::process`,
`Metronome::process` and `Transport::advance`. `write` is `noexcept`, allocates nothing, takes no
lock and opens no file, it copies the block into one `SpscRing<float>` per channel and returns.
`process` mixes the audible tracks into the output without allocating, locking or throwing.
`advance` is three atomic operations and one multiply by the speed word, carrying the fraction a
block leaves over in 32.32 fixed point, so a hundred thousand blocks of 256 at speed 1.3 land on
frame 33280000 rather than near it. `Transport`'s readers, `position_frames`, `state`,
`has_loop`, the two loop ends, `speed_q32`, are safe from either thread.

`Metronome::process` reads the transport position, the clock's beat step and two atomics, then
copies out of a vector the constructor already sized. It is not an `AudioNode` and it holds no
place in the `RigGraph`: the host calls it after `RecordingBus::write`, which is what keeps the
click on the monitor path and out of both recorded files. There is no second recorder that
captures it and no click track written beside the take.

`TakeLibrary` runs on the control thread alone. It carries no atomic and the audio thread has no
route into it: a lane publishes a `TrackTimeline` through an `RtSwapSlot`, a clip inside that
timeline names a take by a raw `const Take*`, and the count that keeps the take alive moves only
in `TrackTimeline`'s constructor and destructor. A slot's `reclaim()` destroys a retired timeline
only once the audio thread's watermark has passed the generation it was published at, so the
order is fixed: reclaim every lane slot first, then call `collect()` once. The other order would
free a take a retired timeline still names.

Everything else, `arm`, `stop`, `Take::load`, `Take`'s constructor, `TrackPlayer::set_take` and
its four control setters, `reclaim`, `Metronome::set_sample_rate`, runs on the control thread and
is free to allocate and throw.

The writer thread that `arm()` starts drains all the channel rings in step, up to 4096 frames per
append, and sleeps 1 ms when it finds them empty. Draining in step is what keeps the channels
aligned: a pair of frame counters, one advanced by the producer and one by the consumer, is
published only at block boundaries, so the writer never sees half a block.

Those same two counters decide whether a block fits. `write` compares the block against the free
space before it pushes anything, so a block is queued whole or not at all. A block that does not
fit is dropped and `dropped_blocks()` counts it. That is the trade the spec asks for: a disk that
cannot keep up costs recorded samples, never an audio dropout. `RecordingBus::K_RING_CAPACITY_FRAMES`
is 65536, 1.37 seconds at 48 kHz.

`RecordingBus::write(processed, dry, frames)` takes both legs in one call and asks both recorders
`has_room()` first, so a block reaches both files or neither. Dropping them independently would
let the processed and dry files end at different lengths, which is the alignment spec §10 makes a
guarantee.

`RecordingBus::arm` opens `<take_name>-processed.wav` and `<take_name>-dry.wav` in one call, which
is what makes them sample-aligned: they begin on the same block. Which transport frame they began
at is the session's fact rather than the recorder's, so `RigController::arm_recording` reads the
position before the bus opens and `stop_recording` places the dry file as a clip at that frame.

Files are 32-bit IEEE float, so a recorded ramp reads back bit-identical;
`tests/nodes/capture/test_track_recorder/test_behaviour.cpp` asserts exactly that, and
`Medium_TrackRecorder.WritePerformsNoHeapAllocation` in
`tests/nodes/capture/test_track_recorder/test_rt_safety.cpp` holds the RT bar.

## Playing a take back

`Take` holds the whole recording in memory, which is the only way the audio thread can read it
with no syscall: at 48 kHz, stereo float, five minutes is 115 MB. `Take::load(path)` runs on the
control thread and reads the channels apart, so a stereo file stays stereo.

`TrackPlayer::set_timeline` publishes a lane's clips into its `RtSwapSlot<TrackTimeline>`, which
swaps a pointer and retires the old object instead of freeing it. `process()` acquires the active
timeline for the block and records the generation it read, so the clips under a playing lane can be
replaced mid-block without the audio thread ever touching freed memory. `TrackPlayer::set_take`
still exists for a host that plays one whole recording: it hands the take to `takes()` and
publishes a one-clip timeline starting at frame 0. `TrackPlayer::reclaim()` on the control thread
,  once per GUI frame is enough, reclaims all eight lane slots and only then collects the
library, and a host that never calls it leaks every timeline and every take it replaces.

A clip's de-click ramp is `TrackTimeline::edge_gain`, and `TrackPlayer` asks for it only at an edge
that cuts into the take. A clip that plays a take from its own frame 0 to its own last frame is
flat, which is what keeps a whole recording sample for sample against the file it was loaded from.

`process()` adds into the output rather than overwriting it, so the caller fills or clears the
buffer first. It mixes the soloed tracks when any track is soloed and the unmuted ones otherwise,
folding each track's gain and its constant-power pan into one gain per output channel before the
run. It reads the position out of the `Transport` and honours the loop range, so a block crossing
the loop end continues from the loop begin exactly where `Transport::advance()` will put the next
one.

The block walks a lane in timeline frames, not output frames. `Transport::advance` moves the
position by `speed` frames per output frame, so a clip's `start_frame` and its take frames are the
same unit and a clip at timeline frame 12345 reads take frame 0 there whatever the speed. The two
are converted at the run's own base with the 32.32 speed word, which costs at most K_MAX_CLIPS
comparisons per run, and a run ends at the loop end or the block end. Speed 1.0 is a bypass:
the clip is copied straight out of the take and never enters the lane's `VariRateReader`, so a
golden file compares bits rather than a filter's output.

`Transport` keeps the loop range in a single `std::atomic<std::uint64_t>`: the begin frame in the
high half, the length in the low. One load gives the audio thread two ends that agree, so no block
can read a begin from before a `set_loop` and an end from after it, and no seqlock is needed. The
cost is that a loop must lie inside the first 2^32 frames, 24.8 hours at 48 kHz, and `set_loop`
clears the loop rather than setting a bad one when its arguments do not fit or do not increase.
`seek()` concurrent with `advance()` can be overwritten by that block's store; the host serialises
the two, and the header says so as a `@pre` rather than paying for a generation counter no caller
needs. `tests/nodes/capture/test_track_player.cpp` covers the mix, and its
`ProcessPerformsNoHeapAllocation` and `TimelineSwapPerformsNoHeapAllocationOnTheAudioThread`
hold the RT bar.

Spec §6 carries the intent behind all of it.

`TrackPlayer::process_lane` renders one lane alone and before its fader: at unity gain, a mono
take on every channel, whatever the lane's gain, pan, mute and solo say. A host that runs each
lane through a processor of its own reads its lanes this way and mixes them itself. A lane is
rendered once a block, by this call or by `process`, never by both.

## Playing a take back at another speed

`VariRateReader` reads a take at a speed between 0.5 and 2.0, and the pitch falls with the speed
the way a tape machine's does. Half speed is an octave down. Nothing here stretches time, and
spec §5a states that as the
behaviour the maintainer asked for.

Each channel gets its own `SushiDSP::Resampler`, sized in `prepare()` on the control thread for a
push of one input frame and a smallest ratio of `1 / K_MAX_SPEED`. `read()` clamps the speed, calls
`set_rates(speed, 1.0)` when it moved, then pushes take frames one at a time until it has written
the exact block the caller asked for, zero-filling past the take's last frame. Every output costs
`speed` take frames, so the push loop is bounded by `output_frames * K_MAX_SPEED` plus the kernel's
half span, and it allocates nothing;
`Small_VariRateReader.ReadPerformsNoHeapAllocation` in `tests/nodes/capture/test_vari_rate_reader.cpp`
holds that bar.

`read()` returns the take frame the next call should start at, so the caller carries the cursor
and the reader carries the phase. A seek, a speed change or a move to another clip invalidates the
converter history, and `reset()` is what discards it.

`Transport::K_MIN_SPEED` and `K_MAX_SPEED` are the same pair, and `vari_rate_reader.cpp` carries a
`static_assert` that they stay the same pair.

## What is not modelled

- **Recovery from a write error.** If `WavWriter::append` throws on the writer thread, the
  recorder stops recording and the file keeps whatever reached it. Nothing reports the error back
  to the UI beyond `is_recording()` going false.
- **Backpressure.** There is none by design. The rings never make the audio thread wait, so the
  only response to a slow disk is a dropped block.
- **A shared writer thread.** Each `TrackRecorder` runs its own, so a `RecordingBus` costs two.
  One thread draining both would halve that and is a change confined to `TrackRecorder`.
- **Sample rate conversion.** A `Take` carries the rate it was captured at and neither
  `TrackTimeline` nor `TrackPlayer` reads it, so a 44.1 kHz file dropped on a 48 kHz rig plays
  8.8 % slow. `applications/host_gui` converts an imported mp3 on the way in, which covers songs and not
  WAV files; `docs/reference/KNOWN_ISSUES.md` carries the entry.

## Implementation notes

### What `TrackPlayer::process` mixes

`TrackPlayer::process()` mixes the soloed lanes when any lane is soloed and the unmuted ones
otherwise. It starts at the transport's position, walks on at the transport's speed, wraps at the
loop end as `Transport::advance()` will, and leaves the transport where it found it. A mono output
carries no pan, so a lane folds into it at its own gain whatever its pan position. At unity speed
a clip is copied out of its take, so the output holds the take's own samples bit for bit; at any
other speed the lane's `VariRateReader` converts it and the pitch falls with the speed. The call
runs on the audio thread and is real-time safe: it allocates nothing, takes no lock and throws
nothing.

### What a `VariRateReader::read` costs

Each output frame of `VariRateReader::read()` costs `speed` take frames, so the push loop runs at
most `output_frames * K_MAX_SPEED` iterations plus the kernel's half span widened by `K_MAX_SPEED`.
Past the take's last frame it pushes zeros. A channel the take does not carry reads the take's
first channel, which is how `TrackPlayer` folds a mono take into a stereo mix.

### The click burst's first sample

The `Metronome` constructor samples each click burst from one frame in, at `(index + 1) /
sample_rate`. The first sample written is therefore not zero, and a click's onset frame is the
frame it is written at.

### Take pointers

`TakeLibrary::insert()` hands out a raw pointer to a `Take` it has just stored. The `Take` stays
where it was heap-allocated: `entries_` growing moves the `unique_ptr`, never the object it owns,
so the pointer stays good until `collect()` frees the take.

### Reclaim order

`TrackPlayer::reclaim()` reclaims every lane slot before it collects the library, so a take a
retired timeline still names is never freed.

### The de-click ramp

A clip's de-click ramp is symmetric about the clip's middle. Asking for the gain at the middle
frame suppresses the edge on that side and leaves the other edge intact.

The de-click gain rises, holds and falls, so its smallest value over a run is at one of the run's
two ends. Both ends at one mean the whole run is flat.

### The converter's expected position

A block's own 32.32 accumulator starts at zero while `Transport::advance()` carries its fraction
on, so the position a lane's converter expects may sit one frame either side of the real one. A
wider gap than that is a seek, a jump between clips or a speed change.

### An unsized converter

A converter that has not been sized for the block cannot carry it, so the lane falls back to the
copy path instead of allocating on the audio thread.

### Disarming during a write

`TrackRecorder::write()` raises the in-flight count before it reads `recording_`, so `disarm()`
can never return while a `write()` call is still touching the rings.

### What the recorder logs

`TrackRecorder` logs under `K_CAPTURE_LOG_CATEGORY`, declared in `capture_log_category.hpp`.
`arm()` logs one `ERROR` when the take file cannot be opened and one when the writer thread
cannot be started; both run on the control thread and `arm()` still returns false. The writer
thread logs one `ERROR` when an append fails, then sets `failed_` and drains nothing more, so a
failed take produces one line. `write()` runs on the audio thread and never logs: a block it
refuses is counted in `dropped_blocks()`.

### Converter output counts

In a `VariRateReader`, every channel's converter carries the same rates and the same push count,
so they all emit the same number of outputs on every push. Channel 0's count speaks for all of
them.

## Counters on the audio thread

`TrackPlayer::counters()` counts a call that arrived with no output buffer or no channel
(`player_empty_call`) and a block that played at unity speed because it was longer than the
prepared size or the transport's speed was 0 (`player_forced_unity_speed`). The host adds both to
a reporter and writes them as one `WARN` per interval. `RecordingBus::dropped_blocks()` and
`TrackRecorder::dropped_blocks()` are read the same way, through a function the reporter calls.

## Designs

<!-- designs:begin -->
- [SUSHIDSP](../../../docs/design/SUSHIDSP/README.md): The standalone amp and pedal simulation framework: the rig graph, the DSP math library, the amplifiers and the studio, timeline and room paths.
<!-- designs:end -->
