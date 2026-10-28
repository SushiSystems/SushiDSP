# RW-3 Choose one error model and the host's split

The row of this entry is in `docs/backlog/GENERAL.md`. This file holds what did not fit the row.

- State: decision
- Done when: the owner has chosen one error model, where `[[nodiscard]]` goes, and how `RigController`, `SlotGraph` and `main()` divide the host
- Source: `docs/design/SUSHI_STANDARDS_MIGRATION/README.md`
- Where: `modules/foundation/io/source/wav_file.cpp`, `applications/host_gui/rig_controller.hpp`, `modules/nodes/room/include/SushiDSP/nodes/room/room_node.hpp`

Each changes a public interface; see `docs/agent/2026_10_12_SUSHI_STANDARDS_MIGRATION/SPEC.md`.

Three error models coexist. A caller handles failure three ways; `RigController` forwards
`SlotGraph` one for one and `main()` owns setup and the frame loop. Cause: exceptions, `bool` with
an error string and bare `bool` were each chosen locally.
