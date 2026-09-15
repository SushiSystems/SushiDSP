# RigController DAG Support — Design Spec

## Summary

`apps/host_gui`'s `RigController` currently models the rig as a flat, ordered
`std::vector<Slot>` where **chain position doubles as `SushiDSP::NodeHandle`**
— an assumption baked into `set_parameter()`, `reclaim()`, and
`rebuild_and_publish()`. This spec generalizes `RigController` into a real
node-graph model (stable slot identity + explicit edges) so the GUI can
build the branching rigs `RigGraph`/`SplitterNode`/`MixerNode` (added in the
prior "RigGraph DAG Support" work) already support at the core level. It also
retires `apps/host_gui`'s bespoke linear-only JSON preset format in favor of
the existing `modules/rig_preset` DAG-capable serializer, closing the
"two parallel, mutually unaware preset schemas" gap flagged when that module
shipped.

This is backend/controller work only — no visual change. `apps/host_gui`'s
existing plain-ImGui-slider rack strip, gear panel, and drag-and-drop
continue to work exactly as today (still only ever building/editing a linear
chain visually). The follow-up "AmpliTube-style skin + branch visualization"
spec is what actually lets a user *see or draw* a split/mix in the GUI; this
spec only makes the controller layer capable of holding one.

## Goals

- `RigController` supports building, editing, and publishing a rig with
  exactly one Splitter → **two** parallel branches → one Mixer shape (v1
  scope; see Non-goals). The linear-chain case (the only shape today) keeps
  working identically through the same convenience API.
- Slot identity survives insert/remove/reorder: a `SlotId` is never reused
  and never means "position N", unlike today's index-as-handle scheme.
- `apps/host_gui` presets are saved/loaded through `modules/rig_preset`
  (`serialize_rig_to_json`/`deserialize_rig_from_json`), not
  `apps/host_gui/Preset.cpp`'s own linear serializer, which is deleted.
- The preset JSON schema gains an optional `"cabinet_ir"` field per node
  entry (meaningful only for a JCM800-type node), closing the cabinet-loss
  gap noted when `modules/rig_preset` first shipped.

## Non-goals (deferred)

- Any visual change to `apps/host_gui` (rack strip rendering, knobs, gear
  panel, drag-and-drop interaction). Entirely the follow-up GUI spec's job.
- N-way or nested branching (more than one Splitter/Mixer pair, more than
  two parallel branches, a split inside a branch). `RigGraph` itself
  supports up to 8-way fan-in/fan-out; `RigController`'s v1 API only ever
  constructs the single-split/two-branch/single-mix shape. A real use case
  for more is what would justify lifting this limit later.
- Migrating existing `.json` preset files saved by the old
  `apps/host_gui/Preset.cpp` format. That format's `save_preset`/
  `load_preset` functions are deleted outright, not translated — this
  project is pre-v1 and the prior DAG-support spec already established the
  precedent (no backward-compat shim for an evolving preset schema).
- Undo/redo for rig edits. Out of scope for both this and the GUI spec
  unless a future spec calls for it explicitly.

## Architecture Overview

### `SlotId` — stable slot identity

```cpp
using SlotId = std::uint32_t;
```

A monotonically increasing counter owned by `RigController`, unrelated to
`SushiDSP::NodeHandle` (which is assigned fresh by `RigGraph::add_node()` on
every `rebuild_and_publish()` and is rebuild-local, exactly as today).
`RigController` maintains a `std::unordered_map<SlotId, RigGraph::NodeHandle>`
translation table, rebuilt every `rebuild_and_publish()` call, so
`set_parameter(SlotId, ...)` can still resolve to the current live node.

### Internal model

```cpp
struct Slot
{
    std::string                     descriptor_id;
    std::map<std::string, float>    parameters;
    std::string                     cabinet_path;
};

struct Edge { SlotId from; SlotId to; };

std::unordered_map<SlotId, Slot> slots_;
std::vector<Edge>                edges_;
SlotId                            next_slot_id_ = 0;
```

This is the in-memory mirror of `modules/rig_preset`'s JSON shape
(`id`/`type`/`params`/`inputs`), which is exactly why adopting that module
for persistence (see below) requires no adapter layer beyond a thin
translate-to-JSON-shape pass.

### API surface

**New primitives** (mirror `RigGraph`'s own prepare-time API one level up):

```cpp
SlotId add_node(const std::string& descriptor_id);   // unconnected
void   connect(SlotId from, SlotId to);
void   disconnect(SlotId from, SlotId to);
void   remove(SlotId);                               // drops slot + touching edges
```

All four republish (call `rebuild_and_publish()`) immediately, matching
every existing structural method's contract.

**Retained convenience API**, reimplemented on top of the primitives, for
the linear-chain UI path that still exists unchanged in `apps/host_gui`:

```cpp
SlotId append(const std::string& descriptor_id);           // was: void, index-based
SlotId insert_after(SlotId anchor, const std::string& id); // was: insert(size_t index, ...)
void   move_after(SlotId moved, SlotId anchor);             // was: move(size_t from, size_t to)
```

`append`/`insert_after` internally call `add_node()` then `connect()` the
new slot into the linear chain at the right point (disconnecting/
reconnecting the one edge that must move, same as today's insert-in-middle
behavior). `apps/host_gui/main.cpp`'s call sites change from passing a
`std::size_t` chain index to threading a `SlotId` instead — a mechanical
rename at every call site, not a UX change, since the GUI still only lets a
user work with the visually-ordered rack strip today.

**Renamed to take `SlotId`** (mechanical signature change, same behavior):
`set_parameter`, `get_parameter`, `set_cabinet`.

**Removed:** `chain_ids()` / index-based accessors that assumed a single
linear order. Replaced by `nodes() -> std::vector<SlotId>` (arbitrary but
stable order — insertion order via the map's... no: `std::unordered_map`
has no stable order, so `RigController` keeps a separate
`std::vector<SlotId> slot_order_` purely for enumeration stability, updated
alongside `slots_`) and `upstream_of(SlotId) -> std::vector<SlotId>`
(mirrors `RigGraph::upstream_of()`, needed by the GUI spec to know which
slot feeds which for rendering).

### `rebuild_and_publish()`

Generalizes from "add every slot to the graph in vector order, connect
`i -> i+1` for all `i`" to: add every slot (in `slot_order_`, order doesn't
matter to `RigGraph` but keeps iteration deterministic for debugging), then
`connect()` every `Edge` in `edges_`, translating each `SlotId` through the
freshly-rebuilt `SlotId -> NodeHandle` map. A `SlotId` whose `add_node()`
call returned no live node (unknown `descriptor_id`, exactly today's
"corrupt preset" dropped-slot behavior) is dropped from `slot_order_` and
`edges_` touching it are dropped too — same "never construct a partial/dead
edge" invariant `RigGraph` itself already enforces via its own
`remove_node()`.

`RigGraph::compile()` may now throw (cycle, multi-sink, zero-sink — all
from the prior DAG-support work). `rebuild_and_publish()` is only ever
called from GUI-driven edits, which only ever construct the restricted
single-split/single-mix or linear shape via the primitives above — so a
throw here indicates a `RigController` bug, not a reachable user action.
State this explicitly rather than silently swallowing it: let the exception
propagate (matching today's behavior — `rebuild_and_publish()` does not
currently catch anything from `compile()` either).

## Preset Integration

### Delete `apps/host_gui/Preset.hpp`/`.cpp`

Their `save_preset`/`load_preset` free functions and the linear
`{"nodes": [{"type", "params", "cabinet_ir"}]}` schema are removed. Callers
in `apps/host_gui/main.cpp` switch to:

```cpp
std::string json_text = SushiDSP::serialize_rig_to_json(*rig_controller.active_graph());
// write json_text to file

std::string json_text = /* read file */;
SushiDSP::deserialize_rig_from_json(*fresh_graph, json_text, info);
// then adopt fresh_graph's topology into RigController's slots_/edges_ —
// see "Loading into RigController" below.
```

### Schema addition: `"cabinet_ir"`

`modules/rig_preset`'s JSON schema gains one optional field per node entry,
meaningful only when `type == "sushidsp.jcm800"`:

```json
{ "id": 2, "type": "sushidsp.jcm800", "params": { "master": 0.5 }, "inputs": [1], "cabinet_ir": "assets/ir/default_4x12.wav" }
```

`Jcm800::set_cabinet()` takes an already-loaded `Ir` object, not a path, so
`Jcm800` itself never knows the cabinet's source path — only
`RigController`'s own `Slot::cabinet_path` remembers that, at the app
layer. `modules/rig_preset` must therefore stay ignorant of `Jcm800`
specifically (it is a generic module; `RigController` is the only layer
that knows about cabinet paths), so the field is populated through an
extension seam rather than hardcoded per-type knowledge inside
`modules/rig_preset`:

- Add an optional callback parameter to
  `serialize_rig_to_json`: `std::function<void(NodeHandle, nlohmann::json&)> annotate_node = nullptr`,
  called (if non-null) once per serialized node with the JSON object being
  built for it, letting the caller add extra fields. `RigController`'s
  preset-save call site passes a lambda that looks up the node's chain
  position's `Slot::cabinet_path` and adds `"cabinet_ir"` if non-empty.
  Symmetrically, `deserialize_rig_from_json` gains an optional
  `std::function<void(NodeHandle, const nlohmann::json&)> on_node_loaded = nullptr`,
  called once per constructed node with its full JSON entry, letting
  `RigController`'s load call site read `"cabinet_ir"` back and call
  `Jcm800::set_cabinet()` (via `IrLoader::load`) itself. This keeps
  `modules/rig_preset` generic (no `Jcm800`/`IrLoader` dependency added to
  that module) while still letting the schema carry the field — the
  callback signature is the extension seam, not a hardcoded `"cabinet_ir"`
  key inside `modules/rig_preset` itself. `RigPreset.hpp`'s Doxygen
  documents both new parameters and that a null callback means "no
  per-node extension".

### Loading into `RigController`

`RigController` gains:

```cpp
void load_from_json(const std::string& json_text);
std::string save_to_json() const;
```

`load_from_json` builds a scratch `RigGraph` via
`deserialize_rig_from_json` (with the cabinet-reading callback above),
then walks the scratch graph's `nodes()`/`upstream_of()`/`get_parameter()`
to repopulate `RigController`'s own `slots_`/`edges_`/`slot_order_` (fresh
`SlotId`s assigned in the scratch graph's node order — preset-local
continuity across save/load is not preserved, matching that
`SlotId` was never part of the JSON schema to begin with, only
`RigGraph::NodeHandle`-equivalent preset-local ids are), then calls
`rebuild_and_publish()` once. `save_to_json` calls `serialize_rig_to_json`
on `active_graph()` with the cabinet-writing callback.

## Testing

- `RigController` unit tests (extending the existing
  `tests/functional/unit/test_rig_controller.cpp`): `add_node`/`connect`/
  `disconnect`/`remove` primitives; `append`/`insert_after`/`move_after`
  convenience wrappers preserve today's linear-chain test coverage
  end-to-end (same assertions, `SlotId`-typed instead of index-typed);
  building the single-split/two-branch/single-mix shape and confirming
  `active_graph()->process()` produces the expected mixed signal (mirrors
  the core-level `RigGraphSplitterAndMixerRoundTripThroughTwoEffectBranches`
  test one layer up).
- `save_to_json`/`load_from_json` round-trip test, including a JCM800 with
  a cabinet installed, confirming the cabinet survives.
- `modules/rig_preset`'s new callback parameters get their own focused
  tests in `tests/functional/unit/test_rig_preset.cpp`: a serialize call
  with a non-null `annotate_node` confirms the extra field appears; a
  deserialize call with a non-null `on_node_loaded` confirms it fires once
  per node with the right JSON entry.
- `apps/host_gui/Preset.cpp`'s existing tests
  (`tests/functional/unit/test_preset.cpp`) are deleted along with the
  file they test.

## Global Constraints

- Everything reachable from `IAudioNode::process()` is unaffected by this
  spec — all changes are GUI/controller-layer, non-RT.
- `RigController`'s structural methods remain GUI/main-thread-only, matching
  the existing contract.
- `modules/rig_preset` stays free of any `Jcm800`/`IrLoader`/cabinet-specific
  dependency; cabinet handling is threaded through via the new generic
  callback parameters, not hardcoded knowledge of a JCM800 field.
- No change to `apps/host` or `apps/host_asio` (both already never touch
  `RigController`, `Preset.cpp`, or `modules/rig_preset`).
- Test files and license headers follow the project's existing conventions
  (see CONTRIBUTING.md), unchanged from the prior DAG-support spec's
  statement of the same constraints.
