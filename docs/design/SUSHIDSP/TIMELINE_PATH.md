# The timeline path: bars, clips and playback speed

The DT phase (spec
the arrangement view design spec in the version history,
plan the arrangement view plan in the version history)
put musical time under the studio path. Before it, a take started at transport frame 0 and the
only tempo in the tree was the guitarist's. After it, a lane holds clips at frames, the host
draws a bar grid, and a recording re-amps at the bar it was played at.

**Musical time is a map, not a clock.** `MusicalClock` holds a tempo, a meter and nothing else:
`position_of(frame)` gives a `BarBeat` and `frame_of(bar, beat)` gives a frame back, both in
32.32 fixed point, and the clock is handed a position rather than keeping one. `Transport` stays
position-and-speed. Either can be replaced without the other noticing, which is the reason the
tempo does not live on the transport where a first draft would put it. `frame_of` floors, so a
bar line is at most one sample early and `frame_of(position_of(f)) <= f` holds, a bar's frame
never lands after the bar it names.

**Clips, and who owns a take.** `TrackTimeline` is one lane's clips: up to `K_MAX_CLIPS` = 16 of
them, sorted at construction, immutable once published, each naming a take by a raw
`const Take*` and carrying its own start frame, take offset and length. `TakeLibrary` owns every
loaded take on the control thread and counts the borrows; a lane publishes a timeline through an
`RtSwapSlot<TrackTimeline>`, and the order that keeps the audio thread safe is fixed: reclaim
every lane slot first, then `collect()` once. No `shared_ptr` reaches the audio thread.
`TrackTimeline::edge_gain` is the 5 ms de-click ramp, and `TrackPlayer` asks for it only at an
edge that cuts into the take, so a whole-take clip is still bit-for-bit its file.

**Speed carries pitch.** `Transport` holds a speed as a 32.32 word between `K_MIN_SPEED` 0.5 and
`K_MAX_SPEED` 2.0, saved in the session, and `advance()` moves the position by that many frames per
output frame while carrying the fraction, so 100000 blocks of 256 at 1.3 land on frame 33280000
rather than near it. `VariRateReader` reads a take at that speed through `Resampler`, one
converter per channel, and the pitch falls with the speed the way a tape machine's does. Half
speed is an octave down; nothing here stretches time, which is spec §5a's stated decision.
Speed 1.0 never enters the reader, `TrackPlayer` copies the take, so the golden files compare
bits rather than a filter's output. Against sample-domain linear interpolation at half speed the
sinc measures 112.5 dB of spurious-free dynamic range at 1 kHz against 47.3.

**The click is in no file.** `Metronome` is not an `AudioNode` and holds no place in the
`RigGraph`. The host calls it after `RecordingBus::write`, which is what keeps the click on the
monitor path and out of both recorded files; there is no click track written beside a take.

**The host.** `applications/host_gui` draws the arrangement full width beneath the rig panels:
`ui::TimelineScale` maps frames to pixels and snaps to a bar, a beat or nothing,
`ui::ArrangementLayout` holds the grid arithmetic the drawing units then only draw, and
`ui::TapTempo` and `ui::MusicalTimeText` serve the transport bar's two rows. A file reaches a
lane by two routes that share one `route_file_drop`: an ImGui drag from the takes browser
(`ui::FileBrowser`, `ui::DirectoryListing`) and a Windows Explorer drop, queued by
`FileDropQueue` from the GLFW callback and drained on the next frame in framebuffer pixels.
`RigController` stamps the transport frame a recording was armed at, places the dry file as a
clip there, and mirrors a lane's clips into the re-amp leg, which is what closes spec §5's
alignment: a take recorded at bar 5 drives the rig at bar 5
(`RigControllerReAmpsARecordedTakeFromTheBarItWasRecordedAt`).

**An mp3 becomes a WAV on the way in.** `dr_mp3` is vendored under `third_party/dr_libs` and
named only inside `applications/host_gui`: `Mp3Decoder` decodes on a worker thread, `ChannelResampler`
converts to the rig's rate through the tree's own `Resampler`, and `AudioImportCache` writes the
result as a float32 WAV under the executable's `cache/imported_audio/` before the clip points at
it. The session stores a clip as a path, so an in-memory decode would leave an empty clip on the
next launch. Float32 rather than 16-bit PCM because resampling a real song raised its peak to
1.0763. A second drop of the same file costs 0 ms against the first's 3128 ms. Nothing prunes
the cache and nothing reads a plain WAV's own sample rate; both are in
[`docs/reference/KNOWN_ISSUES.md`](../../reference/KNOWN_ISSUES.md).

`modules/nodes/capture/README.md` carries the rest, and
[`docs/reference/GLOSSARY.md`](../../reference/GLOSSARY.md) defines bar, beat, clip, take and
playback speed as this tree uses them.
