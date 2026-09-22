# Plan: room furnishings and finishes

**Status:** open, written 2026-09-22. The design it implements is
[`docs/design/ROOM_FURNISHINGS.md`](../../design/ROOM_FURNISHINGS.md); every section number below
points there. The backlog entries this work creates live in
[`docs/design/REMAINING_WORK.md`](../../design/REMAINING_WORK.md).

Order is hierarchy first: the data brick, then the scene that holds it, then the seam that reads
the scene, then the three layers that read the seam, then the node's acceptance, then the
persistence and the panel that sit above all of it. Each task closes with one acceptance
criterion and one build. A build runs only after the maintainer says so, and never while the GUI
is running. Every task's commit carries its module README lines, its design status and its
changelog line, per `docs/CONTRIBUTING.md`.

## State at the end of 2026-09-22

Written, syntax-checked and, for T1 to T10 and T14 and T15, run green in the maintainer's own
build (1086 of 1086 unit tests). Written after that build and not yet run: T11 (starter scene),
T12 (Motus test and tool), T16 (patch editor), the patch validator tightening in
`RoomScene::is_valid` and the codec's "no furnishings field means none" rule. A full
`sd test -s unit` of that state was started when work stopped; its output is in the session's
temporary directory, not in the tree, so run it again first.

Open, in order:

1. Run `sd test -s unit` (close the GUI first: a running `sushidsp_host_gui.exe` blocks the link).
   Expect to read the printed Motus table of `test_room_motus.cpp` and the two new patch-validator
   tests; the hand predictions were 1.58 to 1.80 times the measured decay in the empty room.
2. T13, the close: the room README (`modules/nodes/room/README.md`: the class table gains
   `Furniture`, `MediumAbsorption`, `StarterScene`, the materials and furniture counts, the three
   consumers, the validator's patch rules), `KNOWN_ISSUES.md` (the empty-room over-prediction, the
   fit continuity at 96 kHz for the heavy curtain, no measured desk, bed, bookcase, kilim),
   `REMAINING_WORK.md` (the decision below, the panel's decay read-out, the HRTF asset),
   `ROOM_FURNISHINGS.md` phase states and the 40 to 70 percent figure (the hand estimate for
   Motus reads 1.58 to 1.80 times), the changelog (one line per change, none written yet), and
   `RoomNode.hpp`'s stale "default scene" comments.
3. The maintainer's decision: a floor of absorption for real rooms (design section 11). Two
   measured empty concrete rooms decay 1.5 to 1.8 times longer in the model than in the room.
4. Look at the panel once (Shell, Finishes, Furnishings, the patch rows): nobody has seen it.
5. Commit in pieces. `apps/host_gui/` also holds another session's unrelated edits (topology,
   timeline, transport bar) and `RigPersistence.cpp`, `test_rig_controller.cpp`,
   `CHANGELOG.md`, `KNOWN_ISSUES.md` carry hunks of both: use `git add -p`.

## Tasks

| Id | Task | Files | Acceptance | Build |
| --- | --- | --- | --- | --- |
| T0 | Source and verify the rows: bed, desk, wardrobe, curtains, sofa, and the finishes (plaster on masonry, painted brick, tile, parquet). Measured first, derived second, each with its kind and citation. No code. | `REPORT.md` of this folder | Every row has six values, a unit, a kind (measured, tabulated, derived) and a citation that resolves; each derived row shows its formula | none |
| T1 | `Furniture.hpp/.cpp`: `FurnitureId`, the row struct with unit, area per band, citation, the lookup. Section 4. | `Furniture.hpp`, `Furniture.cpp`, `modules/nodes/room/CMakeLists.txt`, `tests/CMakeLists.txt`, `test_room_furniture.cpp` | One row per id, every citation non-empty, every area non-negative and finite, the lookup returns the row asked for | `sd test -s unit` |
| T2 | `RoomScene::furnishings`, `furnishing_count`, `kMaxFurnishings`, `furnishing_absorption_area()`, and the validator's refusals and the effective-absorption bound. Sections 4 and 5. | `RoomScene.hpp`, `RoomScene.cpp`, `test_room_scene.cpp` | The sum equals hand arithmetic for two items; a scene with none returns the scene of today (`mean_absorption`, `is_valid` unchanged); a non-positive quantity and a seventeenth entry are refused; a furnished room the bare bound refused is accepted | `sd test -s unit` |
| T3 | `MediumAbsorption`: the air's coefficient plus `A_f / 4V` per band, and `over_path` for reflected taps. Section 3. | `MediumAbsorption.hpp`, `MediumAbsorption.cpp`, room `CMakeLists.txt`, `tests/CMakeLists.txt`, `test_medium_absorption.cpp` | With no furnishings the coefficient equals `AirAbsorption`'s bit for bit; with items it equals `A_f / 4V` added; `over_path` equals `exp(-m d / 2)` | `sd test -s unit` |
| T4 | `EyringDecay` takes the seam. | `EyringDecay.cpp`, `test_eyring_decay.cpp` | RT60 of a furnished scene equals the hand formula with `+ A_f`; an unfurnished scene equals today's figures exactly | `sd test -s unit` |
| T5 | `ImageSourceSolver` folds `over_path` on reflected taps; the `(0, 0, 0)` image is left alone. | `ImageSourceSolver.cpp`, `test_image_source_solver.cpp` | A reflected tap's gain drops by `exp(-m_f d / 2)`; the direct tap is unchanged; an unfurnished set equals today's bit for bit | `sd test -s unit` |
| T6 | `ModalSolver` adds `c m_f / 2` to each mode's damping, `m_f` held at the 125 Hz band below it. | `ModalSolver.cpp`, `test_modal_solver.cpp` | A mode's decay time in a furnished scene equals the hand figure; an unfurnished design is unchanged | `sd test -s unit` |
| T7 | Node acceptance in a furnished room. | `test_room_node.cpp` | `RoomNodeIsContinuousAcrossTheSchroederFrequency` holds within 2 dB in a furnished scene; a furnished 60 m3 room's 500 Hz to 1 kHz decay lies inside the band section 2 gives | `sd test -s unit` |
| T8 | A `key` in every material and furniture row; `RigPersistence` derives its words from the tables and reads and writes `furnishings`. Section 7. | `Materials.hpp/.cpp`, `Furniture.hpp/.cpp`, `RigPersistence.cpp`, `test_rig_controller.cpp` | A furnished preset round-trips; a preset without the field loads with none; a material or furniture row added to a table needs no edit in the codec | `sd test -s unit` |
| T9 | `RoomPanel` groups its controls under Shell, Finishes and Furnishings; Furnishings is an editable list. Section 7. | `RoomPanel.hpp/.cpp` | Adding, changing and removing an item publishes a scene the node adopts; the list refuses an eighteenth item | `sd build`, then the maintainer looks at the panel |
| T10 | The finish rows from T0. | `Materials.cpp`, `test_room_materials.cpp` | Each new row passes the table's existing sanity cases and the fit bound of 1 dB at every band | `sd test -s unit` |
| T14 | Coverings are materials: move the curtain rows from `Furniture` to `Materials`, drop `FurnitureUnit` (every row is per item), and update every test, the panel list and the codec that named a per-square-metre row. Design section 3. | `Furniture.hpp/.cpp`, `Materials.hpp/.cpp`, `FurnishingList.hpp/.cpp`, `RoomPanel.cpp`, and the tests that name `CurtainHeavyVelour`, `CurtainLight` or `FurnitureUnit` | No `Furniture` row carries a unit; a curtain is a `Materials` row usable as a patch; a scene with a curtain patch over the window has a lower `A` than the same scene with the curtain counted as furniture would have | `sd test -s unit` |
| T15 | Review fixes: a `@pre` on the seam and the three consumers, a bound test that separates the exact form from the linear one, the missing boundary and ordering tests, single-line comments, `EXPECT_EQ` where the claim is exactness. | `MediumAbsorption.hpp`, `EyringDecay.hpp`, `RoomScene.cpp`, `test_room_scene.cpp`, `test_medium_absorption.cpp`, `test_eyring_decay.cpp`, `test_image_source_solver.cpp`, `test_modal_solver.cpp` | Each review finding of the T1 to T4 review is closed or named in the report as declined with its reason | `sd test -s unit` |
| T16 | The panel's patch editor for the shell layer: a face, a material and an area per patch, up to `kMaxPatches`; the codec already persists patches. | `RoomPanel.cpp`, a pure list unit beside `FurnishingList` and its test | Adding, changing and removing a patch publishes a scene the node adopts; an area over the face is refused with the validator's reason | `sd build`, then the maintainer looks at the panel |
| T11 | The maintainer's room as `RoomScene{}`. Waits for the room's details (design section 11). | `RoomScene.hpp`, room README, test fixtures that read the default | The default scene's mid-band decay lies inside the band section 2 gives; the goldens that move are regenerated with a row in `tests/golden/README.md` saying why | `sd test -s all` |
| T12 | Tier 2: a tool that renders the node's impulse response for a Motus scene and prints RT60, EDT and DRR beside the measured figures. Section 8. | `tools/dsp/`, a small checked-in table | The empty room's RT60 lies within 10 % of the measured figure at 500 Hz and 1 kHz; the differences at the other bands and for the furnished cases are recorded, not bounded | none, the tool runs through `sd` |
| T13 | Close: the room README, `KNOWN_ISSUES.md`, `REMAINING_WORK.md`, the design status, the changelog. | the listed documents | Every link and cited path resolves; `check_docs_layout.py`, `check_changelog.py` and `check_source_comments.py` pass | none |

## Waves

Two agents never touch one file inside a wave.

| Wave | Tasks | Waits on | Build after |
| --- | --- | --- | --- |
| A | T0, T1 | nothing | `sd test -s unit` |
| B | T2 | T1 | `sd test -s unit` |
| C | T3 | T2 | `sd test -s unit` |
| D | T4, T5, T6 | T3 | `sd test -s unit` |
| F | T8, T9 | T2 for T9; T1 for T8 | `sd build`, `sd test -s unit` |
| G | T10, T15 | T8 for T10; T4, T5, T6 for T15 | `sd test -s unit` |
| G2 | T14, T7 | T10 and T15 for T14 | `sd test -s unit` |
| G3 | T16, T12 | T14 for T16; T7 for T12 | `sd build`, `sd test -s unit` |
| H | T11 | T14, T16 | `sd test -s all` |
| I | T13 | everything above | none |

Wave E moved into G2: T7 waits for the review fixes.

Inline runs the same tasks in the same order in one session, T0 through T13.

## Model per dispatch

An implementer runs on `sonnet`. T2, T3 and T5 fix a public struct, a seam and the choice of
which tap the loss skips, so a reviewer on `opus` reads each before the wave's build. Nothing
runs on `fable`.

## What this plan does not cover

The panel's per-band decay read-out, which needs a read on `IRoomHost`, and the patch editor for
the shell layer. Both are backlog items, not part of any task above.
