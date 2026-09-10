# `modules/cabinet` — the standalone cabinet node

One `IAudioNode`, mono in and mono out, registered as `sushidsp.cabinet`. `CabinetNode` loads
an impulse response and convolves it into whatever feeds its input; until `set_cabinet()`
installs one, it passes audio through unchanged.

## Why it exists

Every amp module (`modules/jcm800`, `modules/peavey5150`, `modules/diezel_vh4`,
`modules/mesa_mark2c`) used to implement `ICabinetHost` itself and run its own
`StreamingConvolution` after the decimator. `CabinetNode` replaces that: it is the only class
in the tree that implements `ICabinetHost` now, and none of the four amp classes reference
`ICabinetHost`, `set_cabinet` or `StreamingConvolution` any more. A rig that wants a cabinet
places `sushidsp.cabinet` after its amp explicitly — there is no amp with one built in, and no
rig can convolve the same signal twice by accident. The decision and its rationale are recorded
in [`docs/design/gui_ux_redesign.md`](../../docs/design/gui_ux_redesign.md) §8.

## The blocks and who owns each

| Block | Class |
| --- | --- |
| The `IAudioNode` / `ICabinetHost` contract | `CabinetNode` |
| Impulse-response loading | `Ir`, `IrLoader` (`include/SushiDSP/io/`) |
| The convolution itself | `StreamingConvolution` (`include/SushiDSP/math/`) |

`CabinetNode::set_cabinet`, `::has_cabinet` and `::reclaim_cabinets` match `ICabinetHost`'s
signatures exactly, so `RigController` reaches this node through the same `ICabinetHost*`
pointer the four amp classes used to answer.

## Dependencies

Links only `SushiDSP::sushi_dsp` (`modules/cabinet/CMakeLists.txt`) — no amp module, no device
backend, no host.
