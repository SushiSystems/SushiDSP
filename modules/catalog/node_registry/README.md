# `modules/catalog/node_registry`, the product catalog

One function, `node_registry()`, returning the static list every `NodeDescriptor`-exposing
product is registered under, plus `make_node(id, info)`, the id-to-instance factory both hosts
and the plugin wrapper use to build a node without knowing its concrete type. Adding a product
to the rig means adding one entry to `node_registry.cpp`'s `node_registry()`, nothing else in
`applications/host_gui` or `applications/host_asio` changes, which is the contract
[`docs/design/SUSHIDSP/README.md`](../../../docs/design/SUSHIDSP/README.md) describes for `NodeDescriptor`, the
self-describing factory. This file records what the code in this directory does.

## What it owns

| Concern | Function |
| --- | --- |
| The catalog: descriptor + gear-panel category per product | `node_registry()` |
| Constructing a node by its `NodeDescriptor::id` | `make_node(id, info)` |

`NodeCategory` is a single `const char*` label ("Distortion", "Amp", "Delay", "Routing",
"Room") that only the gear panel reads; the registry attaches it to each entry so no product
module has to know about panel grouping.

## What is registered today

| Id | Category | Product module |
| --- | --- | --- |
| `sushidsp.tubescreamer` | Distortion | `tubescreamer` |
| `sushidsp.jcm800` | Amp | `jcm800` |
| `sushidsp.peavey5150` | Amp | `peavey5150` |
| `sushidsp.mesa_mark2c` | Amp | `mesa_mark2c` |
| `sushidsp.diezel_vh4` | Amp | `diezel_vh4` |
| `sushidsp.dd3` | Delay | `dd3` |
| `sushidsp.splitter`, `sushidsp.mixer`, `sushidsp.pan`, `sushidsp.channel_strip`, `sushidsp.bus` | Routing | `routing` |
| `sushidsp.room` | Room | `room` |

## Dependencies

`node_registry` links `SushiDSP::sushi_dsp` plus every product library it registers
(`tubescreamer`, `jcm800`, `peavey5150`, `mesa_mark2c`, `diezel_vh4`, `routing`, `dd3`, `room` , 
see `modules/catalog/node_registry/CMakeLists.txt`). It is the one place in the tree allowed to depend
on all of them at once; every product module itself depends only on `sushi_dsp`, so this module
is where that fan-in happens, and nowhere else in the codebase needs to list every product by
name.

## Failure mode

`make_node` returns `nullptr` for an id with no matching entry rather than throwing;
`modules/session/rig_preset`'s deserializer is the caller that turns a lookup miss into a diagnosed
error, since it has the JSON context needed to name the offending node.

## Designs

<!-- designs:begin -->
- [SUSHIDSP](../../../docs/design/SUSHIDSP/README.md): The standalone amp and pedal simulation framework: the rig graph, the DSP math library, the amplifiers and the studio, timeline and room paths.
<!-- designs:end -->
