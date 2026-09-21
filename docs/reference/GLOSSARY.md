# Glossary

The words this repository uses in a sense its own code fixes. A term is here when the tree
gives it a narrower meaning than a reader would assume, not because it is unfamiliar.

**bar** — a group of beats, four of them until `MusicalClock::set_meter` says otherwise. A bar's
first frame is `MusicalClock::frame_of(bar, 1)`, floored, so a bar line lands at most one sample
early (20 microseconds at 48 kHz) and never late.

**beat** — one tempo period, `60 / bpm` seconds, held as a 32.32 fixed-point frame count so a
grid does not drift: at 137.4 BPM the step is low by 1.1e-10 of a frame and 20000 bars sit
8.8e-6 of a frame behind ideal. Bars and beats are counted from 1, frames from 0.

**clip** — a take placed on a lane at a frame: `TrackTimeline::Clip` names a `const Take*`, the
timeline frame it starts at (`start_frame`), the take frame it starts from (`take_offset`) and
its length. A lane holds up to
`kMaxClips` = 16 of them, sorted and immutable once published. A clip borrows its take; it never
owns one.

**lane** — one of the eight tracks a `TrackPlayer` mixes, with its own timeline, gain, pan, mute
and solo. The arrangement view draws one row per lane.

**playback speed** — `Transport`'s rate multiplier between `kMinSpeed` 0.5 and `kMaxSpeed` 2.0.
The pitch follows it, the way a tape machine's does: half speed is an octave down. Nothing in
this tree stretches time. Speed 1.0 is a bypass — `TrackPlayer` copies the take rather than
entering `VariRateReader`, so a whole-take clip reproduces its file bit for bit.

**take** — a whole recording held in memory with its channel count and sample rate, `Take`. A
`TakeLibrary` owns every loaded take and is the only code that destroys one; the audio thread
reaches a take through a lane's `RtSwapSlot<TrackTimeline>` and holds it as a raw pointer.

**timeline frame** — the unit clips and the transport position are measured in, at the rig's
sample rate. A clip's `start_frame` and its take frames are the same unit whatever the playback
speed, and `TrackPlayer` converts to output frames at each run's own base.

The classes named here live in [`modules/nodes/capture/`](../../modules/nodes/capture/) and are described in
its [`README.md`](../../modules/nodes/capture/README.md); the intent behind them is in
[`docs/design/sushidsp.md`](../design/sushidsp.md) §3b.
