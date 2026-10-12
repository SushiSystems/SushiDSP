# `modules/nodes/cabinet`, the standalone cabinet node

One `AudioNode`, mono in and mono out, registered as `sushidsp.cabinet`. `CabinetNode` loads
an impulse response and convolves it into whatever feeds its input; until `set_cabinet()`
installs one, it passes audio through unchanged.

## Why it exists

Every amp module (`modules/nodes/jcm800`, `modules/nodes/peavey5150`, `modules/nodes/diezel_vh4`,
`modules/nodes/mesa_mark2c`) used to implement `CabinetHost` itself and run its own
`StreamingConvolution` after the decimator. `CabinetNode` replaces that: it is the only class
in the tree that implements `CabinetHost` now, and none of the four amp classes reference
`CabinetHost`, `set_cabinet` or `StreamingConvolution` any more. A rig that wants a cabinet
places `sushidsp.cabinet` after its amp explicitly, there is no amp with one built in, and no
rig can convolve the same signal twice by accident. The decision and its rationale are recorded
in [`docs/design/GUI_UX_REDESIGN.md`](../../../docs/design/GUI_UX_REDESIGN.md) §8.

## The blocks and who owns each

| Block | Class |
| --- | --- |
| The `AudioNode` / `CabinetHost` contract | `CabinetNode` |
| Impulse-response loading | `Ir`, `IrLoader` (`modules/foundation/io/include/SushiDSP/io/`) |
| The convolution itself | `StreamingConvolution` (`modules/foundation/math/include/SushiDSP/math/`) |

`CabinetNode::set_cabinet`, `::has_cabinet` and `::reclaim_cabinets` match `CabinetHost`'s
signatures exactly, so `RigController` reaches this node through the same `CabinetHost*`
pointer the four amp classes used to answer.

## Dependencies

Links only `SushiDSP::sushi_dsp` (`modules/nodes/cabinet/CMakeLists.txt`), no amp module, no device
backend, no host.
