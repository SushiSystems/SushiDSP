# tests/circuits

Test-side circuit fixtures. Nothing in this folder is linked into a product, and no product may
include it.

## dk_block_node.hpp

`measure::render`, `frequency_response` and `alias_floor` all drive an `AudioNode`, and a DK
block is not one. `DkBlockNode` wraps a one-in, one-out `DkCircuit` so the helpers in
`tests/measure/` can drive it. The block solves in double, and float appears only at the buffer
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

## spice_block_compare.hpp

Each DK block test finds its CSV, band-limits both signals the same way and reads a magnitude
off a stepped sweep. Without this header three test files would carry three copies of a
fourth-order Butterworth.

`band_limit` applies the same filter shape and cutoff to the reference and to the circuit under
test, so the band-limiting cannot flatter either signal. It removes the top octave, where
LTspice's variable-step integrator and the fixed-step trapezoidal rule of `DkCircuit` are known
to disagree (D6b plan, Risks:
`docs/archive/superpowers/plans/2026-08-26-d6b-dk-circuit-core.md`).
