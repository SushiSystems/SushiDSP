# Stereo rig, track recording and playback

**Status:** approved by the maintainer 2026-09-07. DS0 through DS4 landed the same day; DS5, OGG
export, stays deferred. Roadmap prefix **DS** (Dsp Studio); phases DS0 through DS5 below, and
`docs/design/sushidsp.md` §3a describes what they built.

## 1. Goal

The rig becomes channel-aware end to end, so a signal can be panned and heard in stereo; the host
records what you hear and the dry guitar beside it, sample-aligned; and the takes it records can be
played back — layered under live playing, or fed back into the rig so a dry take can be re-amped
through a different amplifier.

Maintainer decisions of 2026-09-07: the graph carries channels end to end rather than a panner
bolted on the output; recording captures the processed output **and** the dry input; playback
serves both the looper and the re-amp; OGG export is allowed but only inside `apps/host_gui`.

## 2. What "stereo" means here, exactly

Every node already declares its channel counts through the `PortLayout` it returns, and
`ProcessContext` already carries an array of channel pointers. What is missing is the graph: it
routes exactly one buffer per edge. DS0 makes it route as many as the two nodes declare.

The amplifiers stay mono in and mono out, and this is not a shortcut. A guitar amplifier is a
mono circuit; running its chain twice over the same samples costs twice the CPU and produces two
identical channels, which is silence-in-the-difference, not stereo. Stereo begins where something
actually differs between the channels: the pan law, a stereo delay, two cabinets, a recorded track
placed off-centre. The graph is stereo end to end in the sense that matters — every edge carries
what its endpoints declare, and nothing in the path collapses a stereo signal by accident.

**Adaptation at an edge**, done by the graph and never by a node:

| Producer | Consumer | Rule |
| --- | --- | --- |
| 1 channel | 2 channels | The sample is copied to both. |
| 2 channels | 1 channel | The two are summed and scaled by 0.5, so a centred signal keeps its level. |
| n | n | Straight through, channel for channel. |

Fan-in sums per channel after adaptation. Any pair the table does not cover makes `compile()`
throw, so an unroutable rig is refused at build time rather than misbehaving at run time.

## 3. DS0 — the channel-aware graph

`RigGraph` today acquires one `BufferPool` buffer per node output and gathers one input pointer
per edge. It gains, per node, an array of `PortLayout::output_channels` buffers, and gathers
`PortLayout::input_channels` pointers per input, applying the §2 table. `BufferPool` is unchanged:
it already hands out single-channel buffers, and a stereo node takes two. `max_live_buffers` is
sized from the compiled graph's total channel count rather than its node count, and `compile()`
reports the figure it needed so the host can size the pool.

`kMaxFanInOut` stays 8, now counted in edges rather than channels.

RT safety is unchanged: the gather arrays are fixed-size members sized at `compile()`, and
`process()` still allocates nothing.

**Why `channels_of` reads `input_channels` as a slot count.** `PortLayout::input_channels` has
counted a node's input slots since the graph was written: `MixerNode` declares `kMaxFanIn` of them
and reads one edge from each. A node's channel width therefore comes from `output_channels`,
narrowed by `input_channels` so that a widening node (1 in, 2 out) is still fed the single channel
it consumes. Changing the field's meaning would rewrite every node's `ports()`, so the graph reads
it the way the nodes already write it.

**Why the pool reserve is one gather, not one per edge.** The adaptation buffers live only while
one node's inputs are being gathered and go back to the pool before the next node runs, so the
reserve is the widest single gather, `kMaxFanInOut * kMaxChannels` = 16, added once. Sizing it per
edge would multiply the pool by the graph's edge count for buffers that are never live together.

**Why a fan-in edge past the last slot folds into that slot.** A consumer declaring fewer slots
than the edges reaching it would otherwise drop the surplus silently. Folding them into the last
slot sums them per channel instead, which is what §2 says fan-in does, and keeps `compile()` the
only place a rig is refused.

## 4. DS1 — pan and the stereo output

- `routing::PanNode`, `PortLayout{1, 2}`, one parameter `pan` in [-1, 1] with a constant-power law
  (`cos`/`sin` of the quarter-turn), smoothed by `SmoothedParameter`. Registered under Routing.
- `apps/host_gui` requests two output channels from the device and writes the rig's terminal node's
  channels to them under the §2 rules, so a mono rig still fills both speakers.
- The device layer already carries `output_channels` in `DeviceFormat`; `Sdl2AudioDevice` and
  `AsioAudioDevice` are checked against a two-channel request and the ASIO channel selection is
  extended if it assumes one.

## 5. DS2 — recording

A new product-independent module `modules/capture`, depending only on `sushi_dsp`.

```
modules/capture/include/SushiDSP/capture/
    Take.hpp            an in-memory multi-channel recording and its sample rate
    TrackRecorder.hpp   one recording destination: RT-side write, writer-thread drain
    RecordingBus.hpp    the pair of recorders the host arms together
    WavWriter.hpp       streaming 16-bit or 32-bit-float WAV output
```

- **`TrackRecorder`** owns one `SpscRing<float>` per channel and one writer thread. Its RT-facing
  `write(const float* const* channels, std::uint32_t frames)` is wait-free, copies into the rings
  and returns; when a ring is full it drops the block and increments a counter the UI reads, so a
  slow disk costs samples rather than an audio dropout. The writer thread drains the rings into a
  `WavWriter`. `stop()` joins the thread and finalises the file's header.
- **`RecordingBus`** owns two `TrackRecorder`s, `processed` (as many channels as the rig's output)
  and `dry` (one channel, the device input before the rig). Arming both from one call is what makes
  the two files sample-aligned: they begin on the same block, and the bus records the block index
  each began at so a later re-amp lines up.
- **`WavWriter`** writes a header with a placeholder size, appends frames, and rewrites the two size
  fields on close. It is the write half of `WavFile`, which reads today.
- Files land in a folder the host chooses, named `<take>-processed.wav` and `<take>-dry.wav`.

Nothing here runs on the audio thread except `TrackRecorder::write`, and that is trapped in
`test_rt_safety.cpp` like every other RT path.

## 6. DS3 — playback

Same module, because a take is the same object read instead of written.

```
    TrackPlayer.hpp     N tracks mixed into an output, each with its own state
    Transport.hpp       play, stop, record, loop, and the shared sample position
```

- **`Take`** holds the whole recording in memory. At 48 kHz stereo float a five-minute take is
  115 MB, which is affordable, and it is the only way the RT thread can read a take without a
  syscall. Loading is a control-thread `Take::load(path)`, published to the RT side through the
  existing `RtSwapSlot`.
- **`TrackPlayer`** holds up to `kMaxTracks` (8) tracks. Each track has a `RtSwapSlot<Take>`, a
  gain, a pan, mute and solo, and the player mixes the unmuted ones (or the soloed ones) into its
  output at the transport's position. Its `process()` is allocation-free and lock-free.
- **`Transport`** owns the sample position, the play and record states, and an optional loop range.
  The RT thread advances it; the UI reads it. A single `std::atomic<std::uint64_t>` position and an
  atomic state word are enough — no lock either way.

**Why the loop range is packed into one word.** The audio thread needs both ends of the range to
agree with each other. Two atomics let a block read a begin from before a `set_loop` and an end
from after it, and wrap somewhere neither range contains. Packing the begin into the high half and
the length into the low half makes the pair a single 64-bit load, so a half-applied range cannot be
observed and no seqlock is needed. The cost is that a loop must lie inside the first 2^32 frames of
the timeline, a little over 24 hours at 48 kHz, which is longer than any take this host records.

**Re-amping** needs no node. The host already assembles the buffer it hands the graph; DS3 gives it
a switch: the rig's input is the live device input, a dry take from the player, or both summed. A
dry take played into a different amplifier is then an ordinary playback with the switch thrown.

## 7. DS4 — the host

- A transport bar: record, play, stop, loop, the position in bars or seconds, and the recorder's
  dropped-block counter when it is not zero.
- A track list: per track, name, source file, mute, solo, level, pan, and a button to send that
  track to the rig input for re-amping.
- A pan control for the rig's own output, which is the `PanNode` parameter when the rig has one and
  a host-level trim when it does not.
- The preset gains the track list and the transport's loop range, so a session reopens where it was.

## 8. DS5 — OGG export, deferred

`libvorbis` and `libogg` enter as an `apps/host_gui` dependency only, per the maintainer's decision
of 2026-09-06; the core keeps its zero-dependency rule. Export converts a finished take, off the
audio thread. This phase is written up when DS0 through DS4 have shipped.

## 9. Out of scope, stated

No metronome, no tempo map, no punch-in, no crossfades, no per-track effects, no undo of a take.
A take is recorded, played, muted, panned, levelled and re-amped; anything else is a later phase.

## 10. Acceptance

- A mono rig fills both device channels; a rig with a `PanNode` at its end moves between them, and
  a hard-panned signal reads at least 40 dB below on the opposite channel.
- `compile()` refuses an edge whose channel counts the §2 table does not cover, and says which edge.
- Recording a take and reloading it reproduces the rendered output within the golden files'
  tolerance, and the processed and dry files are the same length to the sample.
- A dry take re-amped through the same rig, with the same settings, matches the processed file it
  was recorded beside within that tolerance. This is the test that proves the alignment claim.
- `TrackRecorder::write` and `TrackPlayer::process` pass the `operator new` trap.
- Eight tracks playing while the rig runs stay inside the block deadline at 48 kHz and 256 frames,
  measured by the benchmark.
