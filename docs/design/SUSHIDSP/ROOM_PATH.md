# The room path: one cabinet in one room

`modules/nodes/room` is the DE phase's module: one `AudioNode`, `sushidsp.room`, mono in and two
channels out, declaring `PortLayout{1, 2}` on the routing DS0 built. It puts the cabinet in a
shoebox room and hands back what a head or a microphone hears there. Spec
[`docs/archive/superpowers/specs/2026-09-08-room-acoustics-design.md`](../../archive/superpowers/specs/2026-09-08-room-acoustics-design.md),
plan
[`docs/archive/superpowers/plans/2026-09-08-room-acoustics.md`](../../archive/superpowers/plans/2026-09-08-room-acoustics.md).

The control thread solves and the audio thread renders, with one `RtSwapSlot` between them. A
scene, six faces with a material each, the cabinet's pose, the listener's, the microphone's , 
goes into `build_render_set`, which mirrors the source to order 4 (`ImageSourceSolver`), fits each
tap's six-band target to two biquads (`BandResponseFit`), designs the 16-line Hadamard tail from
Eyring's decay (`EyringDecay`, `LateReverbDesign`), and solves the room's eigenmodes below the
Schroeder frequency (`ModalSolver`). The audio thread reads the finished `RoomRenderSet`: the
early taps into sixteen Ambisonic channels, the tail, the modal bank under a power-complementary
fourth-order Butterworth seam, and a `BinauralDecoder` into two ears. A publish arrives as a
second bank and crossfades under a raised cosine, so a slider drag never steps the filters.

Three things a reader should know before changing anything here. Both layers are scaled to one
reference, `DiffuseField`, so the modal octave and the tail meet at the same level across the
seam. No measured HRTF asset ships, so every rig runs the spherical-head fallback of
`SphericalHeadHrtf`, which has no pinna cues. And a structural rig edit rebuilds the node, which
empties the tail.

Measured at 48 kHz in 256-frame blocks on a Release build, the node costs 0.238 ms per block for
the ears and 0.113 ms for the microphone, 4.47 % and 2.11 % of one core; the JCM800 with a room
after it costs 0.893 ms. [`modules/nodes/room/README.md`](../../../modules/nodes/room/README.md) carries the
class-by-class record, the thread rules and every approximation named as such.
