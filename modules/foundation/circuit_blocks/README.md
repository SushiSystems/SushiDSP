# circuit_blocks

Internal module: the circuit blocks that the block tests, the RT-safety traps and the benchmark
build. Its headers are never installed, and no product may include it.

It depends on `core`, `math` and `circuits`. The same-tier edges are listed in
`cmake/SushiDSPLayers.cmake`.

## Entry points

| File | What it holds |
| --- | --- |
| `dk_block_node.hpp` | Adapter that wraps a one-in, one-out `DkCircuit` as an `AudioNode` |
| `jcm_v1a_netlist.hpp` | The JCM800 2203 V1a stage as a `DkNetlist`, for every test-side consumer |

## dk_block_node.hpp

`measure::render`, `frequency_response` and `alias_floor` all drive an `AudioNode`, and a DK
block is not one. `DkBlockNode` wraps a one-in, one-out `DkCircuit` so the helpers in
`signal_measure` can drive it. The block solves in double, and float appears only at the buffer
boundary, which is the rule every product node follows.

## jcm_v1a_netlist.hpp

Three consumers build the JCM800 2203 V1a block: the D6b stage test, the RT-safety traps and the
benchmark. The header describes it once, with the component values named instead of spelled as
literals at each call site.

`build_jcm_v1a` builds two shapes of the block. Without the tail it has six reactive states,
three discrete capacitors plus the triode's three interelectrode capacitances, and probes the
stage output at node 6. With the tail it has seven states and the probe moves to the tail's
load, node 9. The nonlinear branches, the sources and the potentiometer are identical either
way, so the two shapes differ only in what the plan calls S.
