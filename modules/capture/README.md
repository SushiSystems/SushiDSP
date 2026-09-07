# `modules/capture` — recording and playing back takes

A product-independent module: it models no pedal and no amplifier, and depends only on
`SushiDSP::sushi_dsp`. It moves audio between the rig and the filesystem. Nothing here is an
`IAudioNode`.

Intent and the acceptance list live in
[`docs/superpowers/specs/2026-09-07-stereo-tracks-design.md`](../../docs/superpowers/specs/2026-09-07-stereo-tracks-design.md)
§5 for recording and §6 for playback. This file records what the code in this directory does.

## Which class owns what

| Concern | Class |
| --- | --- |
| An in-memory multi-channel recording and its sample rate | `Take` |
| One recording destination: rings, writer thread, drop counter | `TrackRecorder` |
| The processed/dry pair the host arms together | `RecordingBus` |
| The sample clock: position, state and the loop range | `Transport` |
| Mixing up to eight takes into one output at that position | `TrackPlayer` |
| Interleaving and the RIFF header | `SushiDSP::WavWriter`, in the core's `io/` |
| Parsing a WAV file back into samples | `SushiDSP::WavFile`, in the core's `io/` |

`TrackRecorder` knows nothing about takes, folders or naming; `RecordingBus` is the only class
here that knows a recording comes in two files, which is why renaming them touches one file.
`TrackPlayer` reads the position out of a `Transport` and never advances it; the host does that
once per block, so the same clock drives the player, the recorder and the UI.

## Which thread runs what

The audio thread calls three functions here: `TrackRecorder::write`, `TrackPlayer::process` and
`Transport::advance`. `write` is `noexcept`, allocates nothing, takes no lock and opens no file —
it copies the block into one `SpscRing<float>` per channel and returns. `process` mixes the
audible tracks into the output without allocating, locking or throwing. `advance` is three atomic
operations. `Transport`'s readers — `position_frames`, `state`, `has_loop`, the two loop ends —
are safe from either thread.

Everything else — `arm`, `stop`, `Take::load`, `Take`'s constructor, `TrackPlayer::set_take` and
its four control setters, `reclaim` — runs on the control thread and is free to allocate and
throw.

The writer thread that `arm()` starts drains all the channel rings in step, up to 4096 frames per
append, and sleeps 1 ms when it finds them empty. Draining in step is what keeps the channels
aligned: a pair of frame counters, one advanced by the producer and one by the consumer, is
published only at block boundaries, so the writer never sees half a block.

Those same two counters decide whether a block fits. `write` compares the block against the free
space before it pushes anything, so a block is queued whole or not at all. A block that does not
fit is dropped and `dropped_blocks()` counts it. That is the trade the spec asks for: a disk that
cannot keep up costs recorded samples, never an audio dropout. `RecordingBus::kRingCapacityFrames`
is 65536, 1.37 seconds at 48 kHz.

`RecordingBus::write(processed, dry, frames)` takes both legs in one call and asks both recorders
`has_room()` first, so a block reaches both files or neither. Dropping them independently would
let the processed and dry files end at different lengths, which is the alignment spec §10 makes a
guarantee.

`RecordingBus::arm` opens `<take_name>-processed.wav` and `<take_name>-dry.wav` in one call, which
is what makes them sample-aligned — they begin on the same block. It also records the block index
they began at, so a later re-amp lines the dry file back up against the rig's timeline.

Files are 32-bit IEEE float, so a recorded ramp reads back bit-identical;
`tests/functional/unit/test_track_recorder.cpp` asserts exactly that, and
`TrackRecorderWritePerformsNoHeapAllocation` in `tests/functional/unit/test_rt_safety.cpp` holds
the RT bar.

## Playing a take back

`Take` holds the whole recording in memory, which is the only way the audio thread can read it
with no syscall: at 48 kHz, stereo float, five minutes is 115 MB. `Take::load(path)` runs on the
control thread and reads the channels apart, so a stereo file stays stereo.

`TrackPlayer::set_take` publishes a loaded take into a track's `RtSwapSlot<Take>`, which swaps a
pointer and retires the old object instead of freeing it. `process()` acquires the active take for
the block and records the generation it read, so the take under a playing track can be replaced
mid-block without the audio thread ever touching freed memory. The retired takes are freed by
`TrackPlayer::reclaim()` on the control thread — once per GUI frame is enough — and a host that
never calls it leaks every take it replaces.

`process()` adds into the output rather than overwriting it, so the caller fills or clears the
buffer first. It mixes the soloed tracks when any track is soloed and the unmuted ones otherwise,
folding each track's gain and its constant-power pan into one gain per output channel before the
run. It reads the position out of the `Transport` and honours the loop range, so a block crossing
the loop end continues from the loop begin exactly where `Transport::advance()` will put the next
one.

`Transport` keeps the loop range in a single `std::atomic<std::uint64_t>`: the begin frame in the
high half, the length in the low. One load gives the audio thread two ends that agree, so no block
can read a begin from before a `set_loop` and an end from after it, and no seqlock is needed. The
cost is that a loop must lie inside the first 2^32 frames — 24.8 hours at 48 kHz — and `set_loop`
clears the loop rather than setting a bad one when its arguments do not fit or do not increase.
`seek()` concurrent with `advance()` can be overwritten by that block's store; the host serialises
the two, and the header says so as a `@pre` rather than paying for a generation counter no caller
needs. `TrackPlayerProcessPerformsNoHeapAllocation` in
`tests/functional/unit/test_rt_safety.cpp` holds the RT bar, and
`tests/functional/unit/test_track_player.cpp` covers the mix.

Spec §6 carries the intent behind all of it.

## What is not modelled

- **Recovery from a write error.** If `WavWriter::append` throws on the writer thread, the
  recorder stops recording and the file keeps whatever reached it. Nothing reports the error back
  to the UI beyond `is_recording()` going false.
- **Backpressure.** There is none by design. The rings never make the audio thread wait, so the
  only response to a slow disk is a dropped block.
- **A shared writer thread.** Each `TrackRecorder` runs its own, so a `RecordingBus` costs two.
  One thread draining both would halve that and is a change confined to `TrackRecorder`.
- **Sample rate conversion.** A `Take` carries the rate it was captured at and nothing here
  resamples it; a take loaded at a rate the rig does not run at is the caller's problem.
