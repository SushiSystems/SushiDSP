# The studio path: channels, takes and the transport

The DS phase (spec
the stereo tracks design spec in the version history,
plan the stereo tracks plan in the version history)
made the graph channel-aware, added a capture module beside the products, and gave the standalone
host a transport, a track list and a re-amp path.

The amplifiers stay mono in and mono out. Spec §2 gives the reason: a guitar amplifier is a mono
circuit, and running its chain twice over the same samples costs twice the CPU for two identical
channels. Stereo begins where the channels differ, the pan law, a recorded track placed off
centre, two cabinets, so the graph's job is to carry whatever width its endpoints declare and
never collapse one by accident.

**The channel rule.** `RigGraph` acquires `PortLayout::output_channels` buffers per node instead
of one, and adapts at each edge under a table the graph applies and no node ever sees: one channel
into two is copied to both, two into one is summed and scaled by 0.5, n into n passes channel for
channel. `RigGraph::channels_adapt(from, to)` is that table as a predicate, and `compile()` refuses
any edge it rejects, naming the edge, so an unroutable rig fails at build time. `K_MAX_CHANNELS` is
2. `K_MAX_FAN_IN_OUT` stays 8, now counted in edges. `PortLayout::input_channels` is a slot count in
this tree, `MixerNode` declares `{8, 1}` and reads one edge per slot, so a node's width is
`min(input_channels, output_channels)` and its slot count is `input_channels / width`; edges past
the last slot fold into it, summed per channel. `live_buffer_count()` reports the pool size
`compile()` needed: every node's channels, plus `K_MAX_FAN_IN_OUT * K_MAX_CHANNELS` = 16 adaptation
buffers added once, because a gather's buffers go back to the pool before the next node runs.

**Pan and the device edge.** `routing::PanNode` is `PortLayout{1, 2}` with one `pan` parameter in
[-1, 1] on a constant-power law, smoothed by `SmoothedParameter`, registered under Routing with a
front panel. `ChannelSelection` carries the same table out to the device:
`rig_output_adapts_to_device(rig_channels, device_channels)` answers whether a pair is covered and
`write_rig_output_to_device()` performs the mapping, allocation-free. `applications/host_gui` asks its
device for two output channels and prepares the rig at
`min(device output channels, RigGraph::K_MAX_CHANNELS)`, so a mono rig fills both speakers and a rig
ending in a `PanNode` moves between them. `Sdl2AudioDevice` writes any output width: its
constructor sizes one deinterleaved scratch block, and the callback interleaves into SDL2's buffer;
a one-channel device keeps the older path that writes SDL2's buffer directly.

**Takes, tracks and the transport.** `modules/nodes/capture` is product-independent, it models no
circuit, exposes no `NodeDescriptor` and depends only on `SushiDSP::sushi_dsp`. It holds `Take` (a
recording in memory with its sample rate), `TrackRecorder` (one destination: a `SpscRing<float>`
per channel, a writer thread, a dropped-block counter), `RecordingBus` (the processed/dry pair
armed in one call, which is what makes the two files sample-aligned), `Transport` and
`TrackPlayer` (up to `K_MAX_TRACKS` = 8 tracks, each with a take, gain, pan, mute and solo).
`SushiDSP::WavWriter`, in the core's `io/`, is the streaming write half of `WavFile`: 16-bit PCM or
32-bit float, the float path carrying a `fact` chunk and an 18-byte `fmt `. `TrackRecorder::write`
and `TrackPlayer::process` are the module's two audio-thread entry points and both are trapped in
their own tests, `tests/nodes/capture/test_track_recorder/test_rt_safety.cpp` and
`tests/nodes/capture/test_track_player.cpp`. `Transport` packs the loop range into one 64-bit word,
begin in the high half and length in the low, so a block reads both ends in one load and cannot see
a half-applied range; the cost is that a loop must lie inside the first 2^32 frames, 24.8 hours at
48 kHz. `modules/nodes/capture/README.md` carries the rest.

**Re-amping** needs no node. The host chooses what feeds the rig, `RigInputSource::LiveInput`, a
track, or both summed, and a dry take played into a different amplifier is then an ordinary
playback with the switch thrown. The dry leg records the buffer that actually drove the rig rather
than the raw device input, which departs from spec §5's letter so that the dry file matches the
processed file beside it whatever the input source is.
`RigControllerReampingADryTakeReproducesTheRigsOwnOutput`
(`tests/applications/host_gui/test_rig_controller.cpp`) renders a Tube Screamer rig, writes a float WAV,
re-amps it through an identical rig in the host's own callback order, and asserts every sample
within 1e-6. The preset carries the track list and the transport's loop range.
