# Arrangement view: bars, a metronome, clips and dropped files

**Status:** approved by the maintainer 2026-09-08. The four rulings of that date are folded into
§5, §5a, §6, §8, §9 and §11, and §10 records them. Roadmap prefix **DT** (Dsp
Timeline); phases DT0 through DT5 below. It builds on
`docs/superpowers/specs/2026-09-07-stereo-tracks-design.md`, whose §9 refused a metronome — that
refusal is what this spec overturns, and §9 says what stays refused.

## 1. Goal

The host's recording side becomes an arrangement: a ruler numbered in bars with a playhead over
it, eight lanes that hold recordings placed at a position rather than one take anchored at zero,
a metronome that clicks the beat, and a file that can be dragged onto a lane where the user wants
it to start. The maintainer's reference is `image2.png`, AmpliTube 5's arrangement view.

Three things in that screenshot the host already has: the channel strip down the left is
`ui::StagePanel` drawing the selected node; the per-lane R/M/S buttons, level slider and pan
control are `SushiDSP::TrackPlayer`'s four controls, drawn today by `ui::DrawTrackPanel` as a row
list; the record, play, rewind and loop buttons are `ui::DrawTransportBar`. What it has none of is
musical time. `SushiDSP::Transport` counts frames and nothing else.

Closing `docs/reference/KNOWN_ISSUES.md`'s "Spec §5's timeline alignment is not implemented" is
part of this work, not a side effect. A take today re-amps from frame 0 whatever block it was
recorded at. On a ruler with bar lines that error is visible, so DT2 gives a recording a start
frame and this spec is not finished until the alignment test passes.

## 2. Where musical time lives

Tempo is a reading of the sample clock, not a property of it. `Transport` owns the position, the
state and the loop range, and adding a BPM field to it would make one class answer both "what
frame is next" and "what does that frame mean in bars" — two reasons to change. DT0 puts the
mapping in its own class, `SushiDSP::MusicalClock`, in `modules/capture` beside `Transport`.

It has two consumers with different needs:

- **The ruler** asks, once per GUI frame, what bar and beat a frame falls on, and where bar *n*
  starts. Control thread, `double` arithmetic, no constraint worth naming.
- **The metronome** asks, once per audio block, which frames inside the block carry a click. Audio
  thread, and it must not drift: a click computed by adding `frames_per_beat` to a running
  `double` walks off the grid over a five-minute take, and a seek would leave it on a phase of its
  own.

Both are answered by one number, frames per beat, and the drift is removed by deriving the beat
index from the absolute transport position every block instead of accumulating. `MusicalClock`
publishes frames-per-beat as a single `std::atomic<std::uint64_t>` in 32.32 fixed point, written
by the control thread when the tempo, the meter or the sample rate changes and read with one
relaxed load per block. Beat index at frame *p* is then `(p << 32) / step`, an integer division;
the frame that beat starts on is `(beat * step) >> 32`. No rounding accumulates, a seek lands on
the right beat by construction, and the audio thread performs no floating-point conversion of a
tempo it did not compute.

So: the audio thread needs the mapping, but only as one scalar it does not have to interpret. The
bars-and-beats formatting the transport readout shows stays on the control thread.

One tempo per session, one meter. §9 says why.

## 3. DT0 — `MusicalClock`

```
modules/capture/include/SushiDSP/capture/MusicalClock.hpp
```

- `set_tempo(double bpm)`, `set_meter(std::uint32_t beats_per_bar, std::uint32_t beat_unit)`,
  `set_sample_rate(double)`. Control thread. Each recomputes the fixed-point step and stores it.
- `beat_step_q32() const noexcept` — the 32.32 frames-per-beat word. Audio thread, one relaxed
  load.
- `struct BarBeat { std::uint64_t bar; std::uint32_t beat; double fraction; };` and
  `BarBeat position_of(std::uint64_t frame) const`, `std::uint64_t frame_of(std::uint64_t bar,
  std::uint32_t beat) const`. Control thread; these are what the ruler and the position readout
  call. Bars and beats are numbered from 1, as the screenshot's `1 · 1 · 1` readout is.
- It holds no position. The position is `Transport`'s, and a caller passes it in, which is what
  keeps the two classes detachable.

`beat_unit` divides the beat the clock counts, so 6/8 clicks eight notes; the ruler draws a bar
line every `beats_per_bar` of them. Nothing else in the tree reads the meter.

Tests: `frame_of(position_of(f))` is `f` rounded down to its beat, for 120 BPM and 137.4 BPM at
44100, 48000, 96000 and 192000 Hz; bar 100 at 120 BPM and 48 kHz starts at frame 9504000 exactly,
with no accumulated error, which is the drift claim measured.

## 4. DT1 — the metronome

**It makes sound and it is not an `IAudioNode`.** A node lives in the `RigGraph`, so its click
would go through whatever follows it in the chain, land in `<take>-processed.wav` through
`RecordingBus::write`, and be re-amped when that take is played back into the rig. A click is a
monitoring signal, not part of the rig, and the one place it belongs is the same place
`TrackPlayer::process` already occupies: added to the output after the rig has run.

`SushiDSP::Metronome`, in `modules/capture`, is shaped like `TrackPlayer` on purpose — the two are
siblings, both "an RT-safe source that reads a `Transport` and adds into an output":

```cpp
void process(float* const* outputs, std::uint32_t channels, std::uint32_t frames,
             const Transport& transport, const MusicalClock& clock) noexcept;
```

It adds nothing when it is disabled or the transport is stopped. Per block it reads the position,
divides by the beat step to get the first beat at or after it, and for each beat inside the block
starts a click at that beat's frame offset. A click that overruns the block continues in the next
one from a phase counter, which is the only state it keeps.

**Where the click comes from: synthesis, not a file.** `modules/capture` depends on `sushi_dsp`
alone, and a WAV click would add a file to find, a path to store in the preset and a load that can
fail. The click is two sine bursts with an exponential decay, built once in the constructor into
two `std::vector<float>` one-shots — 1760 Hz for a downbeat, 880 Hz for the others, 30 ms with a
1 ms raised-cosine attack so the burst itself does not click. `process` reads them by index. The
downbeat is beat index modulo `beats_per_bar` equal to zero, so the accent follows the meter.

The host calls it in `main.cpp`'s audio callback **after** `RecordingBus::write`, which is what
keeps the click out of both recorded files. That ordering is an acceptance item, not a comment.

The maintainer's ruling of 2026-09-08 settles what follows from it: **the click reaches no
recorded file, ever.** There is no second recorder that captures it and no click track written
beside the take. It is heard on the monitor path and nowhere else, which is the whole reason it
is not an `IAudioNode`. Anyone who later wants a click inside a mix has a metronome to record
through a loudspeaker or a phase of their own to argue for.

Level is one control-thread `float`; the metronome has no pan and no mute beyond its enable.

## 5. DT2 — clips, and the end of "everything starts at zero"

Today `TrackPlayer::Track` holds one `RtSwapSlot<Take>` and `process` reads it at the transport
position, so every take starts at frame 0. A timeline needs a recording placed at a frame, and a
lane that holds more than one.

The smallest step that is honest is not the smallest step available. Adding a per-track
`std::atomic<std::uint64_t> start_frame` alone would close the alignment issue in an afternoon and
then be deleted by the clip list two phases later, because a start frame is a clip list of length
one with the length field left out. DT2 therefore introduces the list once:

```cpp
struct Clip
{
    const Take*   take;          ///< Borrowed; owned by the TakeLibrary this timeline was built from.
    std::uint64_t start_frame;   ///< Timeline frame the take's frame 0 lands on.
    std::uint64_t take_offset;   ///< First take frame the clip plays.
    std::uint64_t length_frames; ///< How many frames of the take the clip plays.
};

class TrackTimeline  ///< One lane's clips, sorted by start_frame, immutable once published.
{
    static constexpr std::size_t kMaxClips = 16;
    ...
};
```

`kMaxClips` is 16, the maintainer's ruling of 2026-09-08. Eight lanes of 16 is 128 clips per
session, and the cap is what keeps a lane a flat array the audio thread can walk.

A track's `RtSwapSlot<Take>` becomes an `RtSwapSlot<TrackTimeline>`. The whole lane is swapped as
one immutable object, so the audio thread never reads a list being edited, and the retire-and-
reclaim path `TrackPlayer::reclaim()` already runs is unchanged. Moving one clip republishes its
lane, which costs a `TrackTimeline` allocation on the control thread and no lock anywhere.

**What it does to `process`'s real-time guarantees.** Nothing that matters, and the reason is the
sort. Clips are ordered by `start_frame` when the timeline is built, so the block walks from the
first clip whose end is past the block start and stops at the first whose start is past the block
end. Work per track per block is bounded by `kMaxClips` comparisons plus the mixing it already
did; there is no allocation, no lock and no branch on a value the audio thread wrote.
`mix_run` is unchanged apart from taking the clip's take-relative position instead of the
transport's. `TrackPlayerProcessPerformsNoHeapAllocation` in
`tests/functional/unit/test_rt_safety.cpp` covers it as it stands and keeps covering it.

**Who owns a `Take`.** Two clips cut from one recording must not hold two copies of 115 MB, so a
take is shared between clips. It is not shared by `std::shared_ptr`. A shared pointer inside a
struct the audio thread reads is one careless copy away from an atomic increment on the audio
thread, and the decrement that follows it can be the one that frees 115 MB inside `process()`.
The maintainer's ruling of 2026-09-08 removes it: nothing the audio thread reads holds a
refcount.

Ownership goes to a control-thread class instead, `SushiDSP::TakeLibrary`, one per
`TrackPlayer`:

```cpp
class TakeLibrary   ///< Owns every loaded Take. Control thread only; no atomics, no RT entry point.
{
public:
    /// @brief Takes ownership of a loaded take and hands back the borrowed pointer clips use.
    const Take* insert(std::unique_ptr<const Take> take);

    /// @brief Records that one more live TrackTimeline points at this take.
    void retain(const Take* take);

    /// @brief Records that one fewer does. Called by ~TrackTimeline.
    void release(const Take* take);

    /// @brief Destroys every take no live timeline points at any more.
    void collect();
};
```

The counts are plain `std::size_t`. They move only on the control thread, because the only
things that retain and release are `TrackTimeline`'s constructor and destructor, and a
`TrackTimeline` is built on the control thread and destroyed on the control thread inside
`RtSwapSlot<TrackTimeline>::reclaim()`.

That is the whole lifetime argument, and it rests on `RtSwapSlot`'s own guarantee. The audio
thread reaches a `Take*` by exactly one route: `acquire_for_audio_thread()` on a lane's slot,
then a clip in the timeline it returned. `reclaim()` destroys a retired `TrackTimeline` only
once the audio thread's generation watermark has passed the generation that timeline was
published at, so no timeline the audio thread can still be reading is destroyed. A take's count
therefore cannot fall to zero while any timeline the audio thread might read still names it, and
`collect()` frees only takes at zero. The order is fixed and is an acceptance item:
`TrackPlayer::reclaim()` calls `reclaim()` on all eight lane slots first, then `collect()` once.
Calling them the other way round would free a take that a timeline still in `retired_` names.

`TakeLibrary` also gives the audio thread's `Take*` its stability: a take is heap-allocated once
and never moves, so a clip's pointer stays good for the take's whole life.

**A clip edge gets a 5 ms fade.** A clip starting mid-waveform steps from silence to a sample and
clicks. The fade is a per-clip ramp at each end, computed inside `mix_run`. This is not a
crossfade — two overlapping clips are refused at build time, so nothing is faded against anything
— and §9 keeps crossfades refused.

**Alignment.** `RecordingBus::arm()` records the block index each file began at;
`docs/reference/KNOWN_ISSUES.md` says the accessor was deleted because nothing could hold the
value. DT2 gives it somewhere to go. Arming stamps the transport position, and stopping creates a
`Clip` at that `start_frame` on the lane the recording was armed to. The KNOWN_ISSUES entry closes
when a dry take recorded from bar 5 re-amps from bar 5.

## 5a. DT2 — playback speed, and the pitch that comes with it

The maintainer wants to slow a take down to work a riff out. The ruling of 2026-09-08 grants
that and grants it in the cheapest honest form: **the whole arrangement plays at a chosen rate,
and the pitch follows the rate down, the way a tape machine's does.** Half speed is an octave
below. Nothing here stretches time, and this paragraph exists so that "playback speed" is never
read later as a licence to build a phase vocoder. Pitch as a control independent of speed stays
refused in §9.

Speed belongs to `Transport`, because it is how fast the playhead moves and the playhead is
already `Transport`'s. One 32.32 fixed-point word, `speed_q32`, written by the control thread and
read once per block. `advance(frames)` multiplies by it and carries the fraction in a member
accumulator, so the position never drifts against a long render; `position_frames()` keeps
returning whole timeline frames. Everything downstream of the position follows for free — the
ruler, the playhead, the loop range and the metronome all read the position and none of them
learns that a speed exists. A slowed session clicks slowly, which is what a guitarist working a
riff out wants.

Reading a clip at that rate is `math/Resampler`, which the tree already has: an arbitrary-ratio
polyphase converter whose `set_rates()` and `process()` are allocation-, lock-, syscall- and
throw-free, with `prepare()` doing the sizing on the control thread. At speed *s* the clip is
converted at ratio 1/*s* — half speed pushes half as many take samples per output frame and the
kernel does the rest.

`Resampler` is push-driven: it takes input and returns however many outputs that produced. A
block needs an exact count. That gap gets its own brick rather than a loop wedged into
`TrackPlayer::process`:

```cpp
class VariRateReader   ///< Reads a Take at a speed, one Resampler per channel, exact output counts.
{
public:
    /// @brief Sizes the converters for the worst case. Control thread; allocates.
    void prepare(std::uint32_t channels, std::uint32_t max_output_frames);

    /// @brief Discards the converter history, so the next read starts from silence.
    void reset() noexcept;

    /// @brief Reads exactly output_frames output samples from take, starting at take_frame.
    /// @param speed Take frames consumed per output frame; clamped to [kMinSpeed, kMaxSpeed].
    /// @return The take frame the next call should start at.
    std::uint64_t read(const Take& take, std::uint64_t take_frame, double speed,
                       float* const* outputs, std::uint32_t channels,
                       std::uint32_t output_frames) noexcept;
};
```

`read` pushes take samples until it has the outputs asked for. The loop is bounded, not open:
each output costs `speed` inputs, so it runs at most `output_frames * kMaxSpeed` plus the
kernel's half span, and it allocates nothing, which is what the RT trap measures.

`kMinSpeed` is 0.5 and `kMaxSpeed` is 2.0, by the maintainer's ruling of 2026-09-08. The converter
allows far more — `Resampler::kMaximumRatio` is 16 — so the bound is a choice about what a
guitarist reaches for, not a limit of the machinery. Half speed drops the take an octave and is
the classic way to learn a passage; a quarter speed is two octaves down and no longer a usable
reference, and nobody works a riff out at four times.

**Speed 1.0 is a bypass, not a ratio of one.** At unity `TrackPlayer` copies the take as it does
today and never enters `VariRateReader`, so every alignment acceptance item in §11 and every
golden file compares sample for sample rather than through a filter. A converter at ratio 1 is
close to transparent and close is not what a golden test measures.

A speed change, a `Transport::seek` and a move to the next clip each call `reset()`, because the
converter's history holds samples from where the playhead used to be. All three are one atomic
read and a memset of a sized buffer, so they stay inside the block.

Speed is one control-thread `double` and one control in the transport bar. The maintainer's
ruling of 2026-09-08 saves it in the session: it is a practical setting he will want back where
he left it, and it costs one field in §8.

## 6. DT3 — the arrangement view

`ui::DrawTrackPanel` is **replaced**, not extended. Its rows are lanes with a different geometry
and a different hit test: a row is a fixed-width strip of widgets, a lane is a strip of widgets
plus a scrollable, zoomable canvas sharing one horizontal scroll with a ruler above it. Keeping
the row list and bolting a canvas on would leave two things claiming to own a track's appearance.
The name, level, pan, R/M/S and the re-amp button move into the lane header, unchanged in
behaviour.

```
apps/host_gui/ui/ArrangementView.{hpp,cpp}   ruler, playhead, lanes, clips, drop targets
apps/host_gui/ui/TimelineScale.{hpp,cpp}     frames <-> pixels, and the snap
```

`TimelineScale` is its own brick because three call sites need the same conversion — the ruler's
bar lines, a clip's rectangle, and a drop's landing frame — and a second copy of it is how a clip
ends up drawn one pixel off the bar it plays on. It holds pixels-per-frame and a scroll offset,
converts both ways, and snaps a frame to the nearest bar, beat or nothing under a UI toggle.

`ui::DrawTransportBar` is **extended**, not replaced. The buttons it draws are the buttons the
screenshot shows. It gains a bars-and-beats readout from `MusicalClock::position_of`, a BPM field,
a TAP button (the mean of the last four taps, discarding a gap over two seconds), a meter field
and a metronome toggle. The loop range, which today is typed in seconds, is typed in bars.

It also gains the §5a speed control: one slider from 0.5 to 2.0 with a click back to 1.0, and a
label that says what the speed does to the pitch, so the control is not mistaken for a
time-stretcher. Pitch, which the screenshot shows beside it, is refused in §9.

## 7. DT4 — dragging a file onto a lane

The maintainer's request, "dosya sürüklensin chanellere", is ambiguous between two mechanisms and
both are in scope, in this order.

**ImGui's payload API, first.** `ui::FileBrowser` already lists `.wav` files and the host already
uses `ImGui::SetDragDropPayload` for gear (`GEAR_DESCRIPTOR_ID` in `ui/GearBrowser.cpp`, accepted
in `ui/ChainStrip.cpp`). A file entry becomes a drag source carrying `TAKE_FILE_PATH`; a lane's
canvas is the target, and the drop frame is the mouse x through `TimelineScale`, snapped. This
reuses what exists and needs no window plumbing at all.

**GLFW's drop callback, second.** A file dragged from Explorer never enters ImGui's payload
system; it arrives through `glfwSetDropCallback`, outside the ImGui frame, with a screen position
that has to be resolved to a lane. `apps/host_gui` gains a small `FileDropQueue`: the callback
pushes the paths and the cursor position, and the next GUI frame drains it and routes each path
the same way an ImGui drop is routed. One routing function, two entry points, which is the reason
the queue exists rather than the callback loading a take itself.

A dropped path is loaded by `RigController::load_track`'s successor — a `place_clip(track, path,
start_frame, error_out)` — on the control thread, so a file that will not parse produces the same
error string the browser produces today.

## 8. DT5 — the session

`RigController::save_to_json` today writes `take_folder`, a `tracks` array of eight rows and a
`loop` object of two frame counts. It gains a `tempo` object — BPM, beats per bar, beat unit,
metronome enabled, metronome level — and each track row gains a `clips` array of `path`,
`start_frame`, `take_offset` and `length_frames`. The loop keeps its frame counts, because a
session reopened after a tempo change must loop over the same audio, not the same bar numbers.

Playback speed is not written. §5a says why: it is a practice setting, and a session that
reopened at 0.4 with no visible cause would read as a bug.

A preset written before DT5 has no `tempo` object and no `clips` arrays; it loads with 120 BPM,
4/4, the metronome off, and each track's file as one clip at frame 0, which is what that file
meant when it was saved.

## 9. Out of scope, stated

The stereo-tracks spec's §9 refused six things. Taking them one at a time:

- **Metronome** — overturned. DT1.
- **Tempo map** — stays refused. A tempo change mid-session means the ruler, the metronome, every
  clip's bar position and the loop range all have to agree about a piecewise mapping, and
  `MusicalClock`'s single fixed-point step becomes a searched table the audio thread reads. One
  tempo per session covers a guitarist recording over a click. It reopens when someone actually
  records a song that changes tempo.
- **Punch-in** — stays refused. Recording starts where the playhead is and stops where it is
  stopped; a punch range is a second pair of positions and an armed-but-not-writing state in
  `RecordingBus`, for a workflow nobody here has asked for.
- **Crossfades** — stays refused. Overlapping clips on one lane are refused when the timeline is
  built, so there is nothing to fade between. The 5 ms clip-edge fade of §5 is a de-click ramp and
  is in scope.
- **Per-track effects** — stays refused. A track goes to the output or to the rig input for
  re-amping, which is the routing this host has.
- **Undo of a take** — stays refused for a recording. Moving a clip is undone by dragging it back,
  and the clip's frame is shown numerically so "back" is exact.

From the reference screenshot, playback speed is **in scope** as of the ruling of 2026-09-08 and
is specified in §5a: a resampling ratio on the clip read, with the pitch falling as the speed
does. What stays refused is **pitch as a control of its own** — a semitone shift at constant
speed, or a speed change that holds the pitch. Either needs a time stretcher that does not exist
in the tree, and it would be a phase of its own with its own acceptance for artefacts. It
reopens when someone wants to transpose a take rather than slow one down.

Refused as well, from the same screenshot: the 1–8 pad grid;
the MASTER DAW and INPUT meters along the bottom, which are `ui::MeterBar` placements and not part
of this work. A count-in before recording is refused — the metronome clicks while the transport
rolls, and a count-in is a negative-position state in `Transport`.

Audio that follows the tempo is out: nothing here time-stretches a clip when the BPM changes, so a
clip keeps its frames and moves nowhere. A clip's bar position is read from its frame, never the
other way round.

## 10. Decisions the maintainer made, 2026-09-08

1. **Clips in one step.** The `TrackTimeline` lands in DT2 as §5 argues, not a per-track
   `start_frame` first. A start frame is a clip list of length one and would be deleted two
   phases later.
2. **Playback speed yes, pitch no.** §5a. Slowing a take drops its pitch with it, tape-style;
   that is accepted, and an independent pitch control stays refused in §9.
3. **`kMaxClips = 16` per lane**, as §5 proposed. 16 × 8 lanes is 128 clips per session, and the
   cap is what keeps a lane a flat array.
4. **The metronome never reaches a recorded file.** §4. It is not an `IAudioNode`, it runs after
   `RecordingBus::write`, and there is no second recorder and no click track file.
5. **No `std::shared_ptr<const Take>` in anything the audio thread reads.** §5. A clip borrows a
   raw `const Take*`; `TakeLibrary` owns the takes on the control thread and `RtSwapSlot`'s
   `reclaim()` is what makes the borrow safe.
6. **The metronome's click is synthesised**, not a WAV. A sampled click sounds better and costs a
   file to find, a path in the preset and a load that can fail.
7. **`kMaxTracks` stays 8**, which the screenshot's eight lanes match.
8. **The metronome's level sits beside its toggle** in the transport bar, not as a ninth lane.

## 11. Acceptance

- `MusicalClock::frame_of(1, 1)` for bar 100 at 120 BPM, 4/4, 48 kHz returns 9504000, and
  `position_of` of that frame returns bar 100 beat 1 fraction 0, at all four sample rates of §3.
- `Metronome::process` passes the `operator new` trap in
  `tests/functional/unit/test_rt_safety.cpp`, alongside `TrackPlayer::process`.
- A take recorded for ten seconds with the metronome enabled contains no click, in **both** files:
  `<take>-processed.wav` correlates with the same rig rendered offline within the golden
  tolerance, `<take>-dry.wav` correlates with the input it was fed, and neither has energy in a
  1760 Hz or 880 Hz bin above the noise floor at any beat frame. Only one recorder exists, and
  the test that no click track is written is that the take folder holds two files.
- The metronome's click onsets land on `frame_of` of each beat to the sample over a five-minute
  render, including across a mid-render `Transport::seek`. This is the drift claim.
- `TrackPlayer::process` passes the `operator new` trap with 8 lanes of `kMaxClips` = 16 clips
  each, and building a lane of 17 clips is refused.
- No type reachable from `TrackPlayer::process` holds a `std::shared_ptr`, checked by a
  `static_assert` on `Clip` and by grepping `TrackTimeline.hpp` and `Clip`'s definition in the
  source-comment checker's pass.
- Two clips built from one `Take*` hold one copy of the samples and free it once: a `TakeLibrary`
  test inserts a take, publishes two timelines naming it, republishes both lanes empty, and
  asserts `TakeLibrary::size()` falls to zero on the `collect()` after the last `reclaim()` and
  not before it.
- `TrackPlayer::reclaim()` calls every lane's `RtSwapSlot::reclaim()` before `TakeLibrary::collect()`.
  A test that publishes a timeline, retires it, and calls `collect()` with the audio thread's
  watermark held back asserts the take is still alive.
- `VariRateReader::read` passes the `operator new` trap at speed 0.5, 1.0 and 2.0.
- At speed 1.0 `TrackPlayer::process` reproduces its take bit for bit, so the converter is
  bypassed rather than run at ratio one.
- A 440 Hz sine take played at speed 0.5 reads 220 Hz, measured by the peak bin of a 32768-point
  FFT of the output, and lasts twice as long. This is the ruling that pitch follows speed, stated
  as a measurement.
- At speed 0.5 the metronome's click onsets land twice as far apart in output frames, on
  `frame_of` of each beat, which is the same drift test run through a speed.
- A clip placed at frame N plays sample 0 of its take at output frame N, measured by rendering a
  unit impulse take placed at three different frames.
- A dry take recorded from bar 5 and re-amped through the same rig matches the processed file it
  was recorded beside, at the same timeline position, within the golden tolerance. This is
  `docs/reference/KNOWN_ISSUES.md`'s open item, and its entry moves to
  `docs/reference/CHANGELOG.md` when this passes.
- Building a timeline whose clips overlap is refused, and the refusal names the two clips.
- A `.wav` dragged from `ui::FileBrowser` onto lane 3 at bar 9 creates a clip on lane 3 whose
  `start_frame` is `frame_of(9, 1)` with bar snap on.
- The same file dragged from Windows Explorer onto the same pixel creates the same clip.
- A session saved with a tempo, a meter, the metronome enabled and four clips reopens with all of
  them, verified by comparing `RigController::save_to_json` output before and after a round trip.
- Eight lanes of clips playing while the rig runs and the metronome clicks stay inside the block
  deadline at 48 kHz and 256 frames, measured by the benchmark.
