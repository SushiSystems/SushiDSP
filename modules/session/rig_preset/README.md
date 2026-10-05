# `modules/session/rig_preset` — portable rig presets

Two free functions that convert between a live, compiled `RigGraph` and a JSON string:
`serialize_rig_to_json()` and `deserialize_rig_from_json()`. This is the concrete form of what
[`docs/design/SUSHIDSP.md`](../../../docs/design/SUSHIDSP.md) calls a portable preset — an ordered
list of `{descriptor.id, {param_id → value}}`, so a rig built in the VST loads unchanged in the
standalone host and vice versa. `RigController` in `apps/host_gui` is the module's only caller
today. This file records what the code in this directory does.

## What it owns

| Concern | Function |
| --- | --- |
| Graph → JSON, one node per array entry with its `id`/`type`/`params`/`inputs` | `serialize_rig_to_json()` |
| JSON → graph, `add_node()`/`connect()` calls in file order | `deserialize_rig_from_json()` |
| Looking up a live node's declared parameters by matching `NodeDescriptor::id` | `descriptor_for_type()` (internal) |

Each node's `params` come from the node's own declared `ParameterDescriptor`s in
`node_registry()`, read by string id — this module has no per-product knowledge and adds none
when a new product registers. `inputs` records, per node, the preset-local ids of its upstream
`connect()` edges, in the order they were connected.

## Extension points

Two callbacks let a caller that does have per-product knowledge extend the schema without this
module knowing about it:

- `AnnotateNodeFn`, called once per node during serialization after the `id`/`type`/`params`/
  `inputs` fields are written, so `RigController` can add fields this module has no opinion
  about (e.g. a `Jcm800`'s cabinet IR path) or overwrite `params` with its own stored values.
- `OnNodeLoadedFn`, called once per node during deserialization, after `add_node()` but before
  its params/edges are applied, so a caller can read a field its own `AnnotateNodeFn` wrote and
  act on it (e.g. install a cabinet) — never invoked for a malformed entry, since every entry
  passes deserialization's full shape check in a first pass before any node is added.

## Failure mode

`deserialize_rig_from_json` throws `std::runtime_error` for any malformed entry — a missing or
wrong-shaped `id`/`type`/`params`/`inputs` key, an unregistered `type`, a duplicate `id`, or an
`inputs` entry naming an id no entry declares — and validates every entry before adding any
node, so a malformed preset leaves `graph` unmodified rather than half-built.
`nlohmann::json::parse_error` still escapes unparseable text.

## Dependencies

Links `SushiDSP::sushi_dsp`, `SushiDSP::node_registry` (to resolve a `type` string to a
`NodeDescriptor`) and, privately, `nlohmann_json` for the JSON representation itself
(`modules/session/rig_preset/CMakeLists.txt`).
