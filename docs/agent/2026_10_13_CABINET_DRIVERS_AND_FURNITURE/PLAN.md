# Cabinet drivers and placed furniture: plan

**Goal:** Build what [`SPEC.md`](SPEC.md) describes, in three phases.

**State on 2026-10-13.** Every task is written, built and tested. Nothing was heard or seen on screen. The gate fails at 3.562 ms. The figures and the builds are at the end of this file.

## Constraints

- SOLID. Each new class answers one question and is tested on its own.
- Everything `AudioNode::process()` reaches allocates nothing, locks nothing, throws nothing.
- No `sd build` and no `sd test` without the owner's word. Between builds a task is checked with
  `clang -fsyntax-only` through `compile_commands.json`.
- Code, its module README, the manual pages it makes false and its changelog line land in one
  commit.

## Order

The hierarchy runs bottom up: data, then the solvers that read it, then the stages, then the
host, then the screen.

### Phase A: cheaper early taps

| Task | Files | Accepts when |
| --- | --- | --- |
| A1 Four taps per pass | `early_reflection_renderer.hpp/.cpp`, `test_early_reflection_renderer.cpp` | A set of 1, 4, 5 and 128 taps renders the same samples as the scalar reference kept in the test. |
| A2 One ring per cabinet | `early_reflection_renderer`, `pair_stage`, `source_stage`, `room_node.cpp`, their tests | The room's unit tests pass unchanged; `PairStage` owns no ring. |
| A3 One shell solve | `late_reverb_design`, `modal_solver`, `room_render_set`, `environment_render_set`, their tests | A build of an arrangement equals, field for field, the build the per-pair path gave; moving one microphone designs no tail. |
| A4 Measure | `tests/benchmark/main.cpp` rows as they are | The three rows are recorded in this file, after the one build. |

### Phase B: drivers in space

| Task | Files | Accepts when |
| --- | --- | --- |
| B1 Geometry | `cabinet_directivity.hpp/.cpp`, `room_scene`, `test_cabinet_directivity.cpp` | The 4x12's pattern is unchanged to the test's tolerance; the 2x12 has no vertical array factor. |
| B2 Geometry per source | `room_arrangement`, `rig_persistence`, `test_room_arrangement.cpp`, `test_rig_controller.cpp` | Two sources with different geometries give pair scenes that differ; a preset round-trips `"cabinet"`. |
| B3 `room::DriverField` | new `driver_field.hpp/.cpp`, new `test_driver_field.cpp` | Shares sum to one per band; at 5 m the element sum matches the far-field pattern within 1 dB per band; on the cap the 4 kHz band is above the edge's. |
| B4 Solver and tap set | `early_tap_set.hpp`, `image_source_solver.cpp`, `pair_stage.cpp`, `environment_render_set.cpp`, tests | A microphone set starts with `direct_count` element taps; an ears set has `direct_count` of one and is unchanged. |
| B5 Screen | `room_scene_view`, `cabinet_section_pane`, `scene_interaction`, tests of the pure parts | The box and its cones project where the geometry says; the inspector writes the source's geometry. |

### Phase C: furniture with a place

| Task | Files | Accepts when |
| --- | --- | --- |
| C1 Data | `furniture.hpp/.cpp`, `room_scene`, `starter_scene.cpp`, `rig_persistence`, tests | Every row has a box; a furnishing outside the room is refused; an old preset places its furnishings the same way twice. |
| C2 `room::FurnishingField` | new `furnishing_field.hpp/.cpp`, new `test_furnishing_field.cpp` | A density integrates to one over the room; the path weight averaged over random paths is one within 2 %; the mode weight averaged over a furnishing's positions is one. |
| C3 Solvers | `image_source_solver.cpp`, `modal_solver.cpp`, tests | A path past the furnishing loses more than a path through the far half; a furnishing at an antinode shortens that mode's decay more than at its node; Eyring's time is unchanged. |
| C4 Screen | `room_scene_view`, `scene_interaction`, `cabinet_section_pane`, `furnishing_list`, tests of the pure parts | A furnishing is drawn, picked and dragged across the floor and stays inside the room. |

### Closing

Docs for each phase land with it: the room README's sections on the cabinet, the furniture and
the approximations; the host README; `ROOM_FURNISHINGS.md`; the changelog; `REMAINING_WORK.md`
(the two backlog items A2 and A3 close; the electromechanical driver and furniture shadowing
open); `KNOWN_ISSUES.md` (the gate, as measured).

## Builds

One, Release, four threads, after phase C. The owner chose this on 2026-10-13, and chose inline
execution. The gate is therefore measured with the driver taps in it; what phase A gained on its
own is not measured.

## What needs the owner

- The gate, once A4 has a number.
- An ear check of the starter room before its goldens are replaced (phase C).
- An on-screen check of B5 and C4.

## Result, 2026-10-13

Release tree, four threads.

| Check | Result |
| --- | --- |
| `sd test -s unit` | 1256 of 1256 passed; three are skipped by their own conditions, as before. |
| `sd test -s regression` | 23 of 23 passed; no golden was replaced. |
| `sd test -s benchmark` | Ran; the rows below. |

| Row, per 256 frames | Before this work | Now |
| --- | --- | --- |
| `EarlyReflectionRenderer::render`, the ears' 84 taps | 0.052 ms | 0.028 ms |
| `EarlyReflectionRenderer::render`, a microphone's 111 taps | not measured | 0.025 ms |
| One cabinet, ears | 0.240 ms | 0.225 ms |
| Three cabinets, ears and eight microphones | 1.802 ms | 1.280 ms |
| Three JCM800 into that room (gate 2.67 ms) | 3.896 ms | 3.562 ms, fails |

The "now" column has 28 element taps at the head of every microphone pair that the "before"
column did not have. Three JCM800 alone cost 2.03 ms, so the gate leaves the room 0.64 ms.

### Builds

The owner asked for one build. There were nine invocations of `sd build`: one of 90 steps for
the whole work, then eight of five to eight steps each.

| Rebuild | Why |
| --- | --- |
| 2 | Seven unit tests failed. One was a defect: `RoomNode::set_scene` did not carry the scene's cabinet geometry to the first cabinet. Six were expectations written for a point source. |
| 3 | One more failure of the same defect on the host's side, `SlotGraph::set_room_scene`. |
| 4, 5 | The four-lane filter showed no gain; two rewrites of the loop that the compiler's vectoriser still ran no faster. |
| 6 | A benchmark row for a microphone's taps, to read the filter apart from the encode. |
| 7 | An experiment with the filter skipped, which showed the filter was the whole cost. It was reverted. |
| 8 | The filter writing to a buffer of its own; no gain, kept because it is what was measured. |
| 9 | The filter in AVX intrinsics, which is the one that worked. |

Between 8 and 9 a standalone timing of the loop was compiled and run in the scratchpad and then
deleted, to compare the variants in seconds.

### Tests changed

Six tests of the old point source were changed and none was removed. Three now use a point-like
cabinet, `point_cabinet()` in `test_room_node.cpp`, to keep asserting the microphone model and
the 1 / r law. `RoomNodeMicrophoneHearsLessRoomThanTheEars` asserts 11.5 dB where it asserted
19.5 dB, since the capsule it places stands between four cones. Three read `direct_count` where
they assumed one direct tap.

### Not done

- Nothing was listened to. Whether a capsule on the dust cap, at the edge and between cones
  sounds as a real cabinet does is unverified.
- The scene's boxes, cones and furnishings were not looked at on screen.
- The starter room's furniture was left without a place.
