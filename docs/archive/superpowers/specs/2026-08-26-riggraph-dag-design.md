# RigGraph DAG Support — Design Spec

## Summary

`RigGraph` already stores an `Edge` list and computes a topological order, but
`process()` assumes a single linear chain: one input, one output per node.
This spec generalizes it to a true directed acyclic graph — multiple fan-out
(one node feeding several) and fan-in (several nodes feeding one) — via two
new node types, `SplitterNode` and `MixerNode`, plus the `process()`/
`latency_samples()` changes needed to route and sum correctly. This is
core + preset work only; no GUI change (`apps/host_gui` keeps building a
single linear chain and continues to work unmodified — it simply never
constructs a `SplitterNode`/`MixerNode` yet). Visualizing and editing
parallel paths in the GUI is a separate, later spec that depends on this one.

## Goals

- A node may feed more than one downstream node (fan-out).
- A node (specifically `MixerNode`) may accept more than one upstream input
  (fan-in), summing them with a per-input gain.
- `compile()` rejects a cycle with a clear error instead of misbehaving.
- `latency_samples()` reports the correct value for a branching graph (max
  over parallel branches, not a sum).
- JSON preset format serializes an arbitrary DAG, not just a linear chain.

## Non-goals (deferred)

- Any GUI change. `apps/host_gui`'s rack strip, drag-and-drop, and parameter
  panel are untouched by this spec.
- Backward-compatible reading of the old linear preset format. The project is
  pre-v1; the format changes and old preset files are not migrated — this is
  a deliberate scope cut, not an oversight.
- A generic N-ary "pan/crossfade" mixing curve — `MixerNode` does plain
  per-input linear gain and sum. Anything fancier (equal-power crossfade,
  M/S) is future work if a real use case needs it.
- Feedback loops / delay-compensated cycles (e.g. a feedback send). `compile()`
  treats any cycle as an error; supporting intentional feedback would need
  explicit delay compensation design, out of scope here.

## Architecture Overview

Three things change:

1. **`core/RigGraph`**: `process()` gains multi-input routing (an upstream
   node's output buffer is handed to every downstream edge, not just one);
   `compile()` gains cycle detection; `latency_samples()` gains a
   max-over-branches walk.
2. **New module `modules/routing`** (depends only on `core`): `SplitterNode`
   and `MixerNode`, registered in `modules/node_registry` like any other
   node (category `"Routing"`).
3. **New module `modules/rig_preset`** (depends on `core` and
   `node_registry`): `serialize_rig_to_json()` /
   `deserialize_rig_from_json()`, a standalone DAG preset schema —
   an explicit node-list-with-inputs serialization rather than an ordered
   chain. `apps/host_gui`'s existing `Preset.hpp`/`.cpp` is untouched and
   unaffected by this spec; it keeps its own linear-chain schema.

   > **Amendment (post-implementation).** This item originally said the DAG
   > schema would land as a change to `apps/host_gui`'s `Preset.hpp`/`.cpp`.
   > During implementation that was superseded by the cleaner non-invasive
   > approach above: a new standalone module, with the GUI's preset code left
   > alone. The text here describes what was actually built.

## Core Changes

### `SplitterNode` (`modules/routing`)

```cpp
class SplitterNode : public IAudioNode
{
public:
    static constexpr std::size_t kMaxFanOut = 8;
    // ports(): 1 input, kMaxFanOut outputs declared; RigGraph only wires the
    // outputs actually connect()-ed to something.
    // process(): copies inputs[0] to every one of its declared outputs.
    // No parameters. latency_samples() == 0.
};
```

### `MixerNode` (`modules/routing`)

```cpp
class MixerNode : public IAudioNode
{
public:
    static constexpr std::size_t kMaxFanIn = 8;
    // ports(): kMaxFanIn inputs declared; RigGraph only wires the inputs
    // actually connect()-ed from something. Unconnected inputs are treated
    // as silence.
    // Parameters: "gain_0".."gain_7", range [0, 2], default 1.0 — one per
    // input slot, always present in the NodeDescriptor regardless of how
    // many inputs end up connected (matches the existing ParameterDescriptor
    // "declared statically" convention used by every other node).
    // process(): out[n] = sum_i( input_i[n] * gain_i ), scalar multiply-add,
    // allocation-free. latency_samples() == 0.
};
```

Both are fixed-max-fan-out/fan-in by design — no `std::vector` resize inside
`process()`, matching the RT-safety bar. `kMaxFanOut`/`kMaxFanIn` of 8 is
generous for any rig a human will build by hand; `RigGraph::connect()`
throws `std::runtime_error` if a caller tries to exceed it (checked at
connect-time, off the audio thread).

### `RigGraph::process()` — multi-fan-in/fan-out routing

Today, each node's `process()` is called with a single upstream output
buffer as its one input. The change: for each node in topological order,
`RigGraph` gathers **all** buffers produced by its upstream edges (in
`connect()` call order) into a small on-stack `const float*[kMaxFanIn]`
array and passes that as `context.inputs` — a node with a single input
(the common case) simply reads `inputs[0]` as it always has; only
`MixerNode` reads more than one slot. This requires no `BufferPool` API
change: fan-out is "the same buffer pointer handed to two downstream
edges" (no copy needed, since every consumer only reads), and fan-in is
"multiple pointers gathered before the call" (also no copy — summation
happens inside `MixerNode::process()`, not in `RigGraph`). `RigGraph`
itself never sums audio — that responsibility stays inside `MixerNode`,
per SRP: `RigGraph` only routes buffer pointers.

### `RigGraph::compile()` — cycle detection

The existing topological sort (Kahn's algorithm or equivalent — read the
current implementation before touching it) already detects "some node was
never scheduled" as a byproduct of nodes with unsatisfied in-degree
remaining after the queue empties. Today that path is presumably unreached
(the graph has always been acyclic by construction, since only linear
chains were ever built). This spec makes it a checked, explicit error:
if any node remains unscheduled after the sort, `compile()` throws
`std::runtime_error("RigGraph::compile: cycle detected")` instead of
silently producing a partial/wrong order.

### `RigGraph::latency_samples()`

Changes from `sum(latency_samples() over topological_order_)` to a
max-over-paths walk: process nodes in topological order, track
`std::vector<std::uint32_t> latency_at[handle]` where
`latency_at[node] = node->latency_samples() + max(latency_at[upstream] for
each upstream edge)` (0 for a node with no upstream). The graph's total
latency is `max(latency_at[node] for every node with no downstream edge)`.
This is the standard longest-path-in-a-DAG computation, done once per
`compile()` call (not real-time; `latency_samples()` itself stays O(1) by
caching the result at compile time rather than re-walking on every call).

## Preset Format (JSON) — DAG serialization

```json
{
  "nodes": [
    { "id": 0, "type": "tube_screamer", "params": { "drive": 0.6 }, "inputs": [] },
    { "id": 1, "type": "splitter",      "params": {},               "inputs": [0] },
    { "id": 2, "type": "jcm800",        "params": { "master": 0.5 }, "inputs": [1] },
    { "id": 3, "type": "some_other_fx", "params": {},               "inputs": [1] },
    { "id": 4, "type": "mixer",         "params": { "gain_0": 1.0, "gain_1": 0.8 }, "inputs": [2, 3] }
  ]
}
```

- `nodes` is an unordered list; `id` is a preset-local integer used only to
  express `inputs` edges within this file (not the same as a live
  `NodeHandle`, which is assigned on load).
- `inputs` lists the `id`s of upstream nodes, in connect order — this order
  is what determines which `MixerNode` input slot (and therefore which
  `gain_N` parameter) each upstream lands on.
- **Save**: iterate `RigGraph::nodes()`, assign each a preset-local `id` in
  handle order, write its edges by translating `RigGraph`'s internal `Edge`
  list into each node's `inputs` array.
- **Load**: parse JSON, `add_node()` every entry (order doesn't matter,
  `node_registry::make_node` looks up by `type`), then `connect()` every
  edge from `inputs`, `compile()`, publish via `RtGraphSlot` — same publish
  path as today, no change there.
- This is a NEW, separate schema, implemented in `modules/rig_preset`. It
  has no relationship to `apps/host_gui`'s existing linear-chain preset
  format, which is left exactly as it is — nothing is migrated, and no
  existing `.json` preset stops loading. Nothing is wired to the new schema
  yet; `modules/rig_preset` has no consumer. The two schemas coexist
  independently, and unifying them (or retiring the old one) is future work
  for the GUI follow-up spec.

  > **Amendment (post-implementation).** This bullet originally framed the
  > DAG schema as a breaking change to the GUI's preset format, with
  > existing presets failing to load. That intent was superseded during
  > implementation by the standalone-module approach; the text above
  > describes what was actually built.

## Testing

- `SplitterNode`/`MixerNode` unit tests: `get_parameter`/`set_parameter`
  round-trip (`MixerNode`'s `gain_N`), RT-safety trap test
  (`*ProcessPerformsNoHeapAllocation`) for both.
- `RigGraph` fan-out test: one source into two sinks, confirm both sinks
  receive an identical copy of the source's output.
- `RigGraph` fan-in test: two sources into one `MixerNode`, confirm the
  summed output matches `source_a * gain_0 + source_b * gain_1` for a known
  input signal and gain values.
- `compile()` cycle-detection test: build A→B→A, confirm `compile()` throws.
- `latency_samples()` branching test: two branches of different
  `latency_samples()` feeding one `MixerNode`, confirm the graph reports the
  longer branch's total, not the sum of both.
- Preset DAG round-trip test: build a branching graph (source → splitter →
  two effects → mixer), set distinctive parameter values including
  `gain_N`, save to JSON, load into a fresh `RigGraph`, assert the same
  topology and every parameter matches.
- `RigGraph::connect()` over-fan-in/fan-out test: connecting a 9th edge to a
  `MixerNode`'s inputs (or from a `SplitterNode`'s outputs) throws
  `std::runtime_error`.

## Global Constraints

- Everything reachable from `IAudioNode::process()` — `SplitterNode`,
  `MixerNode`, and `RigGraph::process()`'s multi-input gather — remains
  allocation-free, lock-free, syscall-free, throw-free. The
  `const float*[kMaxFanIn]` gather array is a fixed-size stack array, sized
  by the existing `max_live_buffers` contract already used for
  `BufferPool`; no new per-block allocation.
- `RigGraph` structural methods (`add_node`, `remove_node`, `connect`,
  `disconnect`, `compile`) remain prepare-time-only; never called from the
  audio thread — unchanged contract, just now also cycle-checked.
- New CMake target `SushiDSP::routing` follows the existing
  `SushiDSP::<module>` alias convention (mirrors `modules/tubescreamer`,
  `modules/jcm800`).
- `apps/host_gui`'s existing behavior (rack strip, drag-and-drop reorder,
  parameter sliders, device selection) is unchanged by this spec — it
  continues to build and edit a linear chain; the DAG capability exists in
  `core` and is exercised by tests and the preset format, but no GUI affordance
  to create a split/mix exists until the follow-up GUI spec.
