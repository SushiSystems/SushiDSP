# Cabinet drivers and placed furniture

**Status:** Open — built and tested; RW-12 and RW-58 are left. Section 8 lists where the tree departs from the text.

## 1. What the owner asked for

After seeing the cabinet section on screen, the owner asked for two things:

1. A cabinet must stop being a point. A microphone 5 cm from a 4x12 is treated today as if it
   stood 5 cm from a single point at the baffle's centre, between the four cones, so what it
   picks up is wrong in level, tone and phase.
2. Furniture must stand in the room as objects with a place, not as a count in a list.

Three choices were put to the owner and answered:

| Question | Answer |
| --- | --- |
| What a driver simulation covers | The drivers in space: cones on the baffle that a microphone can be placed against. The cabinet's own voice still comes from `CabinetNode`. An electromechanical driver model is not part of this work. |
| What placed furniture does to the sound | It absorbs where it stands. It does not shadow the direct sound and it reflects nothing. |
| Order, given the failed gate | The early taps are made cheaper first, the gate is measured again, then the two features are written. |

## 2. Where the tree stands

- `room::ImageSourceSolver::solve` reduces every image to one `room::EarlyTap`. The cabinet
  enters as `room::CabinetDirectivity::gain_for`, a far-field pattern of four pistons, applied
  to the line of sight and to every reflection alike.
- `room::CabinetGeometry` holds a cone radius, one spacing, a baffle width and a breakup
  frequency. `K_CABINET2X12` and `K_CABINET4X12` are the same numbers.
- The geometry is one field of the shell, `room::RoomScene::cabinet`, so every cabinet in a room
  is the same cabinet.
- `room::RoomScene::Furnishing` is a furniture row and a quantity. The furnishings enter through
  `room::MediumAbsorption` as one loss per metre, the same along every reflected path and for
  every mode.
- `room::EarlyReflectionRenderer::render` runs the taps one after another. Each tap is a ring
  read, two biquads in double precision and an accumulate. The biquads are a serial chain per
  sample, and they are most of the 1.25 ms the 24 microphone pairs cost in the benchmark.

## 3. Phase A: cheaper early taps

The gate leaves the room 0.71 ms beside three JCM800s, and the room at its limits takes 1.80 ms.
The backlog's two items, one ring per cabinet and one shell solve per arrangement, save memory
and control-thread time. Neither shortens a block on the audio thread. The one change that does
is to filter several taps at once.

### A1. Four taps per pass

`room::EarlyReflectionRenderer` filters its taps in groups of four. The four chains are
independent, so the group runs as four lanes of one loop: the ring reads fill four scratch
blocks, one loop advances four pairs of biquads, and the accumulate runs per lane. Each lane
does the arithmetic the scalar loop does, in the same order, so a tap's output does not change.
The state layout per tap, `K_STATE_PER_TAP`, and `carry_tap` stay as they are. A set whose count
is not a multiple of four pads the last group with silent lanes.

The loop is written over fixed arrays of four, with no intrinsics, and the optimised build's
`-mavx2 -mfma` turns it into vector code. The benchmark, not the listing, is the acceptance.

### A2. One ring per cabinet

`room::EarlyReflectionRenderer` stops owning a ring. It reads the `room::DelayRing` it is
handed, as `room::DirectPath` does, and `Room::SourceStage` owns the one ring every pair of its
cabinet reads. `Room::PairStage::write_input` goes away. This removes 27 ring copies per block
and the memory behind them.

### A3. One shell solve per arrangement

`room::LateReverbDesign` and `room::ModalSolver` each split into a part that reads the shell
alone and a part that reads a pair's poses. `Room::EnvironmentBuilder` runs the shell part once
per build and the pair part per pair. A drag that moves one microphone then designs no tail and
no resonator. Phases B and C both add work to a solve, so this comes before them.

### A4. The gate, measured again

After A1 to A3 the benchmark runs and the three rows of the last plan are recorded again. The
gate stays at 2.67 ms until the owner moves it. If it still fails, the number is reported and
the work goes on to phase B, because the owner chose that order knowing the gate was open.

## 4. Phase B: drivers in space

### B1. Geometry

`room::CabinetGeometry` describes the box and its cones:

| Field | Meaning |
| --- | --- |
| `cone_radius_m`, `breakup_frequency_hz` | As today. |
| `width_m`, `height_m`, `depth_m` | The box. `width_m` takes the place of `baffle_width_m`. |
| `driver_count`, `drivers` | Up to four cone centres on the baffle, each an offset to the left and upward from the baffle's centre, in metres. |

A source's position is the baffle's centre. The three shipped geometries carry their real
layouts: one cone, two cones side by side, four cones in a square. `room::CabinetDirectivity`
takes its array factor from the cone centres, so a 2x12 stops narrowing vertically as a 4x12
does. The 4x12's far-field pattern does not change.

The geometry moves from the shell to the cabinet: `Room::SourcePlacement` gains a `cabinet`
field and `Room::Arrangement::pair_scene` copies it into the pair's scene. A preset stores it
per source as `"cabinet"`, one of `"1x12"`, `"2x12"`, `"4x12"`; a source without the key is a
4x12.

### B2. The near field

A new class, `room::DriverField`, answers one question on the control thread: what reaches a
point from the cones of one cabinet along the line of sight. It returns one radiating element
per tap.

Each cone is seven elements: its centre and six points on a ring at 0.7 of the cone's radius.
An element is a small source on the baffle with a delay and a 1/r of its own. Its level per band
follows the model the far field already uses: above the breakup frequency the cone's effective
radius shrinks, so the ring's share falls and the centre's share rises. The shares of one cone
sum to one in every band, and the cones share the cabinet's level equally, so far from the
cabinet the elements add up to the pattern `room::CabinetDirectivity` gives.

What this produces, and what the single point could not:

- Against the dust cap the centre element is nearest and carries the highs: the bright position.
- Toward the cone's edge the centre moves away and the ring's nearer points take over: darker.
- Between two cones the two arrivals differ in delay, and the sum combs.
- The proximity effect is evaluated per element, at the element's own distance.

A receiver behind the baffle plane takes the rear shelf the far-field model already has.

### B3. Who uses it

A microphone's line of sight is always the element sum: 7, 14 or 28 taps in place of one. There
is no distance at which the model switches, so nothing jumps while a microphone is dragged. The
element taps come first in the set and are never the ones dropped when the set is full.

Reflections keep the far-field pattern. An image is metres away even when the microphone is
against the grille.

The listener keeps the far-field pattern on the line of sight too. Its direct sound is one
HRIR-filtered path, and at listening distance the elements differ by less than the pattern
already says. This is recorded in the room README as an approximation.

`room::EarlyTapSet` gains `direct_count`, the number of leading taps that belong to the line of
sight, so `Room::PairStage::take_direct_image` and the delay compensation read a count where
they assume one today.

### B4. On screen

The scene draws each cabinet as its box with its cones on the baffle. The inspector of a
selected cabinet offers the three geometries. A selected microphone shows its distance to the
nearest cone's centre and how far off that cone's axis it stands, since those two numbers are
what an engineer places a microphone by.

## 5. Phase C: furniture with a place

### C1. Data

`room::FurnitureItem` gains a nominal box per row: width, depth and height in metres.
`room::RoomScene::Furnishing` gains a position on the floor and a yaw. The quantity stays and
scales the row's absorption as it does today. A furnishing must stand inside the shoebox; it may
overlap a cabinet or another furnishing, since nothing collides.

A preset stores `"position"` and `"yaw"` per furnishing. A furnishing without them is placed by
a fixed rule, along the walls in list order, so an old preset loads to the same room every time.

### C2. What a place changes

The total absorption does not change, so the reverberation time and the fit against the Motus
and BRAS CR2 rooms stand. What changes is who pays it. A new class, `room::FurnishingField`,
holds each furnishing as a density around its box that integrates to one over the room, and
answers two questions:

- **A reflected path.** The loss along a path is the furnishing's loss per metre weighted by the
  density the path runs through. The solver samples the unfolded path, folds each sample back
  into the room and reads the density there. A reflection that passes the sofa loses more than
  one that crosses the empty half of the room. Averaged over all paths the weight is one.
- **A mode.** A furnishing damps a mode by the mode's pressure squared at the furnishing, taken
  over the density, relative to the mode's mean. A bed at a pressure antinode of the first axial
  mode damps that mode more than the same bed at its node.

The tail and Eyring's formula keep the room's total, as today. The line of sight is not thinned,
as today.

The yaw turns the box on screen. The density is round in plan, so the yaw does not change the
sound. This is recorded as an approximation.

### C3. On screen

The scene draws each furnishing as its box, selectable and draggable across the floor. The
furnishing list adds a row at the room's centre. The inspector of a selected furnishing shows
its row, its quantity and its yaw.

## 6. What changes for someone who has the tree today

- The room's regression goldens move: the starter scene has furniture, and its reflections now
  lose by position. The owner ear-checked that room on 2026-09-23, so the new goldens are
  written only after the owner has listened again.
- A microphone pair's output changes whenever the microphone is near its cabinet. That is the
  point of the work.
- `room::RoomScene::cabinet` stays as the field a pair's scene carries, but the arrangement
  fills it per source.
- Public names removed: `CabinetGeometry::baffle_width_m`, `CabinetGeometry::driver_spacing_m`,
  `Room::PairStage::write_input`.

## 7. What this work does not do

- No electromechanical driver: no Thiele-Small resonance, no impedance curve, no compression.
- No shadowing and no reflection from furniture.
- No change to the listener's direct path.
- No per-cone voicing: every cone of a cabinet is the same cone.
- No microphone inside the box or behind the grille cloth as a separate case.

## 8. As built

The tree departs from sections 3 to 5 in these places.

| Section | Text | Tree |
| --- | --- | --- |
| A1 | The lanes are written without intrinsics. | The loop is written with AVX intrinsics, with a lane-after-lane loop for a build without AVX. Left to the vectoriser the four-lane loop took 10 ns a frame against the scalar loop's 8; the intrinsic loop takes 3. |
| A4 | The gate is measured after phase A alone. | The owner chose one build at the end, so the gate was measured once, with the driver taps in it. |
| B1 | `width_m` replaces `baffle_width_m`; the cones are a list of offsets. | `baffle_width_m` and `driver_spacing_m` stay. `driver_columns` and `driver_rows` say how many cones stand each way, and `height_m` and `depth_m` are new. No public name was removed from the geometry. |
| B2 | Six points on a ring at 0.7 of the radius. | 0.72, the centroid of the annulus the ring stands for. |
| B4 | The inspector shows the microphone's distance and its offset from the axis. | As written, plus which cabinet and cone, and whether the capsule is behind the baffle. |
| C1 | A furnishing without a place is placed by a fixed rule along the walls. | A furnishing has a `placed` flag. Without it the furnishing absorbs evenly, exactly as before this work, so an old preset and the starter room render as they did. The room panel has an "In the room" switch per row, and a row added there starts at the room's centre. |
| 6 | The regression goldens move. | They did not: the starter room places nothing, and the listener's path is unchanged. |

One thing no section foresaw: a single scene's cabinet geometry now goes to the first cabinet
through `RoomNode::set_scene` and the host's `set_room_scene`, and comes back through `scene()`,
since the geometry left the shell.
