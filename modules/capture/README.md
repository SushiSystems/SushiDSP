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
| Interleaving and the RIFF header | `SushiDSP::WavWriter`, in the core's `io/` |
| Parsing a WAV file back into samples | `SushiDSP::WavFile`, in the core's `io/` |

`TrackRecorder` knows nothing about takes, folders or naming; `RecordingBus` is the only class
here that knows a recording comes in two files, which is why renaming them touches one file.

## Which thread runs what

`TrackRecorder::write` is the only function in the module the audio thread calls. It is
`noexcept`, it allocates nothing, takes no lock and opens no file: it copies the block into one
`SpscRing<float>` per channel and returns. Everything else — `arm`, `stop`, `Take::load`,
`Take`'s constructor — runs on the control thread and is free to allocate and throw.

The writer thread that `arm()` starts drains all the channel rings in step, up to 4096 frames per
append, and sleeps 1 ms when it finds them empty. Draining in step is what keeps the channels
aligned: a pair of frame counters, one advanced by the producer and one by the consumer, is
published only at block boundaries, so the writer never sees half a block.

Those same two counters decide whether a block fits. `write` compares the block against the free
space before it pushes anything, so a block is queued whole or not at all. A block that does not
fit is dropped and `dropped_blocks()` counts it. That is the trade the spec asks for: a disk that
cannot keep up costs recorded samples, never an audio dropout. `RecordingBus::kRingCapacityFrames`
is 65536, 1.37 seconds at 48 kHz.

`RecordingBus::arm` opens `<take_name>-processed.wav` and `<take_name>-dry.wav` in one call, which
is what makes them sample-aligned — they begin on the same block. It also records the block index
they began at, so a later re-amp lines the dry file back up against the rig's timeline.

Files are 32-bit IEEE float, so a recorded ramp reads back bit-identical;
`tests/functional/unit/test_track_recorder.cpp` asserts exactly that, and
`TrackRecorderWritePerformsNoHeapAllocation` in `tests/functional/unit/test_rt_safety.cpp` holds
the RT bar.

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
