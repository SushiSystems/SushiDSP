# The parallel tracks: DS, DT, DE and DR

The parallel tracks run beside the D phases because nothing in them touches the circuit core.

## DS: stereo, tracks and playback

Spec the stereo tracks design spec in the version history, plan
the stereo tracks plan in the version history. It is the graph's channel routing, a
capture module and the host. "The studio path" describes what it built.

| Id | What it builds | State |
| --- | --- | --- |
| DS0 | `RigGraph` routes as many channels per edge as its two nodes declare; `channels_adapt()`, `live_buffer_count()`, `K_MAX_CHANNELS = 2` | built |
| DS1 | `routing::PanNode` on a constant-power law, the same table in `ChannelSelection`, two output channels from both hosts' devices | built |
| DS2 | `modules/nodes/capture`'s recording half, `Take`, `TrackRecorder`, `RecordingBus`, and `io::WavWriter` | built |
| DS3 | `Transport` and `TrackPlayer`: eight tracks, gain, pan, mute, solo, and a loop range packed into one 64-bit word | built |
| DS4 | The host: a stereo output, record and play in the callback, a transport bar, a track panel, the re-amp input switch, the preset's track list and loop range | built |
| DS5 | OGG export through `libvorbis`/`libogg`, inside `applications/host_gui` only; written up once a user asks for it | planned |

## DT: the arrangement view

Spec the arrangement view design spec in the version history, plan
the arrangement view plan in the version history. It continues DS with musical time: a
tempo grid, clips on lanes and a playback speed. "The timeline path" describes what it built.

| Id | What it builds | State |
| --- | --- | --- |
| DT0 | `MusicalClock`, the frame-to-bar-and-beat map in 32.32 | built |
| DT1 | `Metronome`, a click on every beat on the monitor path only | built |
| DT2 | `TakeLibrary`, `TrackTimeline`, `VariRateReader`, `Transport`'s speed, and `TrackPlayer` mixing clips at that speed | built |
| DT3 | `ui::TimelineScale` and the arrangement view: the ruler, the lanes, the clips, the tap tempo | built |
| DT4 | `FileDropQueue` and the two drop routes, both through one `route_file_drop` | built |
| DT5 | The session: clip placement, the armed frame, the tempo and the playback speed | built |
| DT6 | mp3 import: `dr_mp3`, a worker thread, the rig's rate, a cached float32 WAV | built |

## DE: room acoustics

Spec the room acoustics design spec in the version history, plan
the room acoustics plan in the version history. `modules/nodes/room` needs the
graph's stereo routing and nothing from the circuit core. "The room path" describes what it built.

| Id | What it builds | State |
| --- | --- | --- |
| DE0 | `BandResponse`, `Materials`, `AirAbsorption`, `BandResponseFit`, `RoomScene`, `ImageSourceSolver`, `EarlyTapSet` | built |
| DE1 | `EarlyReflectionRenderer`, `RoomRenderSet`, `RoomHost`, and `RoomNode` swapping sets under a raised cosine, registered as `sushidsp.room` | built |
| DE2 | `EyringDecay`, `LateReverbDesign`, `FdnDesign` and the 16-line Hadamard `LateReverb` | built |
| DE3 | `ModalSolver`, `ModalBank`, `CrossoverPair`, the seam at f_s and the level match across it, in both quality modes | built |
| DE4 | `CabinetDirectivity`, `MicrophoneModel` and the `listener` parameter; the SM57 preset is not in it, see `docs/backlog/README.md` | built |
| DE5 | `HrtfSet`, `HrtfSetLoader`, `SphericalHeadHrtf`, `AmbisonicEncoder`, `BinauralDecoder`, `direct_delay` at `compensated`, `tools/dsp/convert_hrtf.py` and `assets/hrtf/README.md`; no measured asset ships | built |
| DE6 | The room panel, `RoomHost` from `RigController`, the scene in the preset, the 30 Hz publish pacing | built |
| DE7 | AVX2 lanes in `EarlyReflectionRenderer` behind `SUSHIDSP_ENABLE_AVX2`. Its gate is the DE1 benchmark passing 2.0 ms with an amplifier in the graph; the JCM800 with a room measures 0.893 ms per 256-frame block | planned |
| DE8 | `max/RayTracer`, `max/LateIrSynthesizer` and the `quality` parameter; the spec's section 9a.3 designs it and no task builds it | planned |

## DR: delay and reverb

Spec the dd3 spx90 design spec in the version history. It runs beside the D phases
because neither unit needs the DK core: no block in either is a nonlinear network wanting a Newton
solve. DR1 built `math/Resampler`, DR2 built the DD-3 (`modules/nodes/dd3/`), DR3 builds the SPX90
engine and the eight programs that calibrate against the Surefyre IR pack, and DR4 adds the
remaining 22 and the graph integration.

| Id | What it builds | State |
| --- | --- | --- |
| DR1 | `math/Resampler` | built |
| DR2 | The Boss DD-3 delay, `modules/nodes/dd3/` | built |
| DR3 | The SPX90 engine and eight programs calibrated against the Surefyre IR pack | planned |
| DR4 | The remaining 22 programs and the graph integration | planned |
