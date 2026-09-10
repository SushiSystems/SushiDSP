# `modules/routing` — the graph's own routing nodes

Three plain `IAudioNode`s a user places directly in the rig graph, registered under the
"Routing" gear-panel category: `SplitterNode`, `MixerNode`, `PanNode`.
[`docs/design/sushidsp.md`](../../docs/design/sushidsp.md) §3a describes `RigGraph`'s own
edge-width adaptation (one-into-two copied, two-into-one summed and scaled); these three nodes
are the user-visible complement to that — things a rig can name and place, not things the graph
does silently at an edge. This file records what the code in this directory does.

## The nodes

| Node | `type_id()` | Ports | What it does |
| --- | --- | --- | --- |
| `SplitterNode` | `sushidsp.splitter` | 1 in, 1 out | Copies input to output; exists so a rig can name an explicit split point, since fan-out itself is already `RigGraph`'s job (one output buffer read by more than one downstream edge) |
| `MixerNode` | `sushidsp.mixer` | up to `kMaxFanIn` in, 1 out | Sums up to `RigGraph::kMaxFanInOut` inputs, each scaled by its own `gain_N` parameter (default 1.0); a plain multiply-add, no crossfade curve |
| `PanNode` | `sushidsp.pan` | 1 in, 2 out | Splits one channel into two on a constant-power law, one `pan` parameter in [-1, 1], smoothed by `SmoothedParameter` |

`PanNode` is the one design-spec-documented node here: §3a specifies it as `PortLayout{1, 2}`
with one `pan` parameter on a constant-power law, and notes that `ChannelSelection` carries the
same adaptation table out to the device edge that `RigGraph` applies internally.

## Why they're plain nodes and not graph primitives

`SplitterNode` does no work its `process()` couldn't skip — it exists only so a rig can name an
explicit split point in the gear panel or preset graph, not because it duplicates the buffer
itself. Keeping it as an ordinary `IAudioNode` rather than a `RigGraph` special case means the
preset format, the gear panel and the parameter plumbing all treat it like any other product,
with no special-casing anywhere else in the tree.

## Dependencies

Links only `SushiDSP::sushi_dsp` (`modules/routing/CMakeLists.txt`) — no product module, no
device backend. `PanNode` additionally uses `SmoothedParameter` from the core's `math` library.
