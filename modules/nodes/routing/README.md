# `modules/nodes/routing`, the graph's own routing nodes

Five plain `AudioNode`s a user places directly in the rig graph, registered under the
"Routing" gear-panel category: `SplitterNode`, `MixerNode`, `PanNode`, `ChannelStripNode` and
`BusNode`.
[`docs/design/SUSHIDSP/STUDIO_PATH.md`](../../../docs/design/SUSHIDSP/STUDIO_PATH.md) describes `RigGraph`'s own
edge-width adaptation (one-into-two copied, two-into-one summed and scaled); these nodes are the
user-visible complement to that, things a rig can name and place, not things the graph does
silently at an edge. This file records what the code in this directory does.

## The nodes

| Node | `type_id()` | Ports | What it does |
| --- | --- | --- | --- |
| `SplitterNode` | `sushidsp.splitter` | 1 in, 1 out | Copies input to output; exists so a rig can name an explicit split point, since fan-out itself is already `RigGraph`'s job (one output buffer read by more than one downstream edge) |
| `MixerNode` | `sushidsp.mixer` | up to `K_MAX_FAN_IN` in, 1 out | Sums up to `RigGraph::K_MAX_FAN_IN_OUT` inputs, each scaled by its own `gain_N` parameter (default 1.0); a plain multiply-add, no crossfade curve |
| `PanNode` | `sushidsp.pan` | 1 in, 2 out | Splits one channel into two on a constant-power law, one `pan` parameter in [-1, 1], smoothed by `SmoothedParameter` |
| `ChannelStripNode` | `sushidsp.channel_strip` | 1 in, 2 out | One mono source's channel: `level` (dB, -60 to 12), `pan` (constant power, as `PanNode`), `polarity` (Boolean) and `delay` (ms, 0 to 50), the delay through a `SampleDelayLine`. Level and pan are smoothed over 20 ms; the delay is not, so a delay change takes effect at once |
| `StereoStripNode` | `sushidsp.stereo_strip` | 2 in, 2 out | One stereo source's strip: `level` (dB, -60 to 12), `balance` (-1 to 1) and `width` (0 to 1), all smoothed over 20 ms. Balance leaves the centre at unity on both sides and turns the far side down; width scales the side signal, 0 being the mono sum |
| `BusNode` | `sushidsp.bus` | 8 stereo inputs, 2 out | Gates and sums the mixer's inputs. `mute_0`..`mute_7` and `solo_0`..`solo_7` are Boolean. A muted input is silent whatever its solo; while any connected input is soloed, only soloed inputs sound. Each gate moves over 5 ms, so a toggle does not click; the first block after construction or `reset()` starts each gate at its target, so a bus built with an input muted never lets it through |

The bus holds solo because only the bus sees every input; a strip cannot silence its neighbours.
`MixerNode` stays as the plain summing node the splitter topologies use. Width belongs to
`StereoStripNode`, the strip of a source that is already stereo: the room's listener pair, the
sum of the room microphones and the master.

`PanNode` is the one design-spec-documented node here: §3a specifies it as `PortLayout{1, 2}`
with one `pan` parameter on a constant-power law, and notes that `ChannelSelection` carries the
same adaptation table out to the device edge that `RigGraph` applies internally.

## Why they're plain nodes and not graph primitives

`SplitterNode` does no work its `process()` couldn't skip, it exists only so a rig can name an
explicit split point in the gear panel or preset graph, not because it duplicates the buffer
itself. Keeping it as an ordinary `AudioNode` rather than a `RigGraph` special case means the
preset format, the gear panel and the parameter plumbing all treat it like any other product,
with no special-casing anywhere else in the tree.

## Dependencies

Links only `SushiDSP::sushi_dsp` (`modules/nodes/routing/CMakeLists.txt`), no product module, no
device backend. `PanNode` additionally uses `SmoothedParameter` from the core's `math` library.

## Designs

<!-- designs:begin -->
- [SUSHIDSP](../../../docs/design/SUSHIDSP/README.md): The standalone amp and pedal simulation framework: the rig graph, the DSP math library, the amplifiers and the studio, timeline and room paths.
<!-- designs:end -->
