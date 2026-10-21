# Host GUI redesign: the record

**Status:** Shipped — 2026-10-21

Which plan built which part of [the host GUI redesign](../../design/GUI_UX_REDESIGN.md), which
took the record out when it took the design format.

| Part | Built by |
| --- | --- |
| Audio-device panel (centered; `DrawDeviceSettings` computes the display center) | Task 1 of `docs/archive/superpowers/plans/2026-09-10-host-gui-redesign.md` |
| Main window layout | Tasks 3 and 4 of the same plan |
| Splitter and mixer automation | Task 5 of the same plan |
| Transport bar | Tasks 2 and 4 of the same plan |
| Mini-DAW panel | Task 6 of the same plan |
| File browser removal | Task 3 of the same plan |
| Cabinet node | `docs/archive/superpowers/plans/2026-09-10-cabinet-node.md`, all 8 tasks |
| Chain types (single path, 2-ways and 3-ways splitter) | 2026-09-22, against AmpliTube 5's user manual chapter 3 |
| Phase 2 | `docs/archive/superpowers/plans/2026-09-14-host-gui-phase2.md`, except the Pitch field |

The first-draft assumptions that code inspection dropped, on 2026-09-10: a hardcoded device name
(`Roland Rubix`; `applications/host_gui/main.cpp:211-217` enumerates the drivers and
`applications/host_gui/ui/device_settings.cpp` renders them) and a user-facing track count
control (`SushiDSP::TrackPlayer::K_MAX_TRACKS = 8` in
`modules/nodes/capture/include/SushiDSP/nodes/capture/track_player.hpp:51`). The owner decided the
cabinet path on 2026-09-10.
