# `modules/session/rig_preset`, portable rig presets

Two free functions that convert between a live, compiled `RigGraph` and a JSON string:
`serialize_rig_to_json()` and `deserialize_rig_from_json()`. This is the concrete form of what
[`docs/design/SUSHIDSP.md`](../../../docs/design/SUSHIDSP.md) calls a portable preset, an ordered
list of `{descriptor.id, {param_id → value}}`, so a rig built in the VST loads unchanged in the
standalone host and vice versa. `RigController` in `applications/host_gui` is the module's only caller
today. This file records what the code in this directory does.

## What it owns

| Concern | Function |
| --- | --- |
| Graph → JSON, one node per array entry with its `id`/`type`/`params`/`inputs` | `serialize_rig_to_json()` |
| JSON → graph, `add_node()`/`connect()` calls in file order | `deserialize_rig_from_json()` |
| Looking up a live node's declared parameters by matching `NodeDescriptor::id` | `descriptor_for_type()` (internal) |

Each node's `params` come from the node's own declared `ParameterDescriptor`s in
`node_registry()`, read by string id, this module has no per-product knowledge and adds none
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
  act on it (e.g. install a cabinet), never invoked for a malformed entry, since every entry
  passes deserialization's full shape check in a first pass before any node is added.

## Failure mode

`deserialize_rig_from_json` throws `std::runtime_error` for any malformed entry, a missing or
wrong-shaped `id`/`type`/`params`/`inputs` key, an unregistered `type`, a duplicate `id`, or an
`inputs` entry naming an id no entry declares, and validates every entry before adding any
node, so a malformed preset leaves `graph` unmodified rather than half-built.
`nlohmann::json::parse_error` still escapes unparseable text.

## Dependencies

Links `SushiDSP::sushi_dsp`, `SushiDSP::node_registry` (to resolve a `type` string to a
`NodeDescriptor`) and, privately, `nlohmann_json` for the JSON representation itself
(`modules/session/rig_preset/CMakeLists.txt`).

## Implementation notes

### What serialization writes

`serialize_rig_to_json` writes `{"nodes": [{"id", "type", "params", "inputs"}, ...]}`, where each
node's `inputs` lists the preset-local `id`s of its upstream nodes in `connect()` order. It uses
`node_registry()` to look up each live node's declared parameters by matching
`NodeDescriptor::id`, which is why the graph must have been compiled. `annotate_node` lets the
caller add extra fields to a node's JSON object or overwrite ones already written; null means no
extra fields. The `std::runtime_error` for an unregistered type should not happen for any node
constructed via `make_node()`, but a hand-built `RigGraph` that uses a non-registered
`AudioNode` subclass cannot be serialized.

### What deserialization accepts

`deserialize_rig_from_json` accepts text produced by `serialize_rig_to_json()` or hand-written to
the same schema, and rebuilds it into the graph through `add_node()` and `connect()`. It does not
call `compile()`; the caller compiles afterward once, the same as after any other graph edit.
Each node is constructed with the `PrepareInfo` passed in. A null `on_node_loaded` means no extra
handling. The function throws `std::runtime_error` for a missing or wrong-shaped `id`, `type`,
`params` or `inputs` key, a `type` not found in `node_registry()`, a duplicate `id`, or an
`inputs` entry that references an id no entry declares. Every entry is fully validated in a first
pass before any node is added, so the graph is left unmodified for any malformed entry.
`nlohmann::json::parse_error` still escapes for text that is not valid JSON.

### The validation pass

The first pass of `deserialize_rig_from_json` validates every entry in full: key presence, key
shape, type resolution, duplicate ids and dangling input references. It does so before mutating
the graph at all, so a corrupt or hand-edited preset, or a wrong file entirely (a `host_gui`
preset, for example, has `type` but no `id`), never leaves the caller's live graph half-populated
with orphan nodes that would then break its next `compile()`.

### The dangling-reference sweep

The dangling-reference check needs the complete id set, so it runs as its own sweep. Doing it
there, and not letting a map `.at()` throw during the build pass, keeps the documented
`std::runtime_error` contract and the guarantee that the graph is untouched.

### Parameter order

Parameters are applied in `std::map` order, which is alphabetical by parameter id. Nothing
designed that ordering. Every `set_parameter_for_control_thread()` implementation today is
order-independent; if one ever becomes order-dependent, the function must switch to preserving
descriptor order or JSON-file order.
