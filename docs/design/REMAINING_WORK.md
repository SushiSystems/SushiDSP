# Remaining work

**Status:** open, last revised 2026-09-10.

The single backlog. An entry here names work that is not started or not finished; a defect that
is measured, understood and deliberately left alone lives in
[`docs/reference/KNOWN_ISSUES.md`](../reference/KNOWN_ISSUES.md) instead, and the entries below
link to it rather than repeat its figures.

Order inside a section is the order the work should be taken in, not a priority ranking.

## Waiting on the maintainer's ears

The room's mix control has not been played since the tail's handover moved. Before
2026-09-09 the first tail energy arrived 5198 frames after the direct sound, 108 ms at 48 kHz,
because the input delay carried the handover time on top of the shortest line; the shortest line
is the handover now. The maintainer played the old build at `mix = 0.09` with +6 dB of make-up
and called anything above that "gaipten geliyor". Two questions close this: whether the gap is
gone, and whether `mix = 1.0` is usable.

If it is still unusable, the level is not the suspect. The direct path measures within air
absorption of `1/r` and `DiffuseField` reproduces the classical critical-distance formula
algebraically, both anchored against hand arithmetic. What is left is the shape of the control:
`mix` blends dry against a wet path that carries the room's own direct sound, so the honest
control may be the listener's distance to the cabinet rather than a dry/wet ratio. No work
starts on that without the verdict.

## Room, opened by the DE phase

- **Bound the output.** Nothing between `RoomNode` and the device limits what the room can
  produce; `AsioSampleConverter::clamp_unit` is the only clamp in the path. The decision number
  used to be the corner-cube's +36 dB, but `RoomScene`'s absorption gate refuses that scene now,
  and the loudest a still-valid scene reaches has not been measured. Measure it first, then
  decide whether a guard belongs in the node, the rig or neither.
- **Ship a measured HRTF set**, and weight the converter's diffuse average by solid angle. Two
  entries in KNOWN_ISSUES, one licence question and one arithmetic fix; the second is worthless
  until the first lands, because no rig loads a converted file today.
- **Run the tail's output cascade twice under a fade**, so a listener or microphone move stops
  stepping the filter on the network's stored energy.
- **Give `BandResponseFit` an analytic Jacobian.** It stops at its eight-step cap on about 60 %
  of fits and costs 4.33 ms for 150 fits, against the 30 Hz republish budget.
- **Draw patches in `RoomPanel`.** The garage reference scene is the one every room measurement
  in the spec is quoted against, and it cannot be built from the user interface.
- **Rebuild the room's spec section 6.2.** Its line-allocation sentence sizes the delay lines for
  an RT60 of 3 s, and `FdnDesign::kMaxLineSeconds` is the 0.525 s that follows from it, while
  `RoomScene::kMaxDecaySeconds` admits scenes up to 12 s. The lines do not lengthen with the
  gate, so a scene between the two decays at the echo density 0.525 s lines give. Whether that
  thins audibly is not measured.

## Host

- **Gear-browser grid.** `apps/host_gui/ui/GearBrowser.cpp` still draws a vertical list, one
  full-width card per row (`GearBrowser.cpp:92`); the grid of draggable amp/pedal/cabinet
  components described in `docs/design/gui_ux_redesign.md` §3 was never built — only the
  panel's position moved. See `docs/design/gui_ux_redesign.md`.
- **`RigController::publish()` rebuilds every node.** One structural edit builds the node set
  three times, which is why "No bundled HRTF set" prints three times on a graph change and why a
  room edit through the graph empties the tail (KNOWN_ISSUES, "A structural rig edit empties the
  room's tail"). Carrying surviving nodes across a rebuild closes both and touches no module.
- **Render the rig in stereo on the ASIO host**, which also gives
  `write_rig_output_to_device`'s 2-into-1 downmix its first live caller and stops a stereo branch
  collapsing at a `MixerNode`. Three KNOWN_ISSUES entries, one change.
- **DS5, the OGG export.** A five-minute stereo take is 115 MB of float on disk today.

## Products

- **Read the Mesa Mark IIC+'s effects send/return network off a schematic.** Three tests are
  skipped for it and the amplifier has no clean end on its knob.
- **Find a Diezel VH4 power-amp schematic.** Its phase inverter and power section are the
  JCM800's, borrowed whole.
- **Close the alias floor.** The maintainer's call between 16×, a band limit between the stages,
  or a higher-order antiderivative; part 4's measurements for all three are in KNOWN_ISSUES.
- **Transcribe the remaining eight material rows**, and measure the SM57's curve.
