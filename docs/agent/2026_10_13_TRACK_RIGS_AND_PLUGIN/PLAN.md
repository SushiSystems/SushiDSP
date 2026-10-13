# Track rigs, export and the first plugin: plan

**Goal:** Build what [`SPEC.md`](SPEC.md) describes.

**State on 2026-10-13.** Every task is written, built and tested. Nothing was heard, the screen was not checked, and the plugin has not met a DAW. The session writes the DAW inline; one worker, on
`opus` at `low`, writes the plugin and builds nothing.

## Constraints

- SOLID. Each new class answers one question and is tested on its own.
- Everything the device callback reaches allocates nothing, locks nothing, throws nothing.
- No `sd build` and no `sd test` until the code is written; then one Release build with four
  threads, and a rebuild only for what that build and its tests turn up.
- Code, its README, the manual pages it makes false and its changelog line land in one commit.

## Order

| Task | Files | Accepts when |
| --- | --- | --- |
| D1 A controller on a shared timeline | `rig_controller.hpp/.cpp` | A controller built on a timeline reads and writes that timeline; one built without makes its own, and the existing tests pass. |
| D2 One lane alone | `capture/track_player.hpp/.cpp`, its test | A lane rendered alone equals what `process` adds for that lane at unity gain and centre pan. |
| D3 `Session` | new `session.hpp/.cpp`, new test | It starts with one track; a track added has a rig of its own; removing one moves the others down; mute and solo give the gains a mixer gives. |
| D4 `SessionRenderer` | new `session_renderer.hpp/.cpp`, new test | One track with an empty rig renders its lane plus the live input; two tracks sum; a bypassed rig passes its lane; nothing is allocated. |
| D5 The host on the session | `main.cpp`, `ui/arrangement_view`, `ui/transport_bar`, `ui/top_bar` | The rig view shows the selected track's rig; the callback calls the renderer. |
| D6 Dry takes | `session_timeline.cpp`, its test | A stopped recording places the dry take on the armed track. |
| D7 A track's rig and file | new `track_file.hpp/.cpp`, `rig_persistence.cpp`, tests | A track file round-trips its name, strip, rig and clips; a session preset round-trips every track's rig. |
| D8 Export | new `session_bounce.hpp/.cpp`, new `mp3_encoder.hpp/.cpp`, `cmake/SushiDSPLame.cmake`, root `CMakeLists.txt`, `NOTICE.md`, tests | A session of one sine clip bounces to a WAV whose samples are the renderer's; an MP3 decodes back to the same length within a frame. |
| P1 The plugin | `applications/plugin/**`, `tests/functional/unit/test_rig_adapter.cpp` (worker); root `CMakeLists.txt`, `tests/CMakeLists.txt` (session) | The worker's report carries its syntax checks; the session's build links `SushiDSP.clap` and the adapter's tests pass. |

## What needs the owner

- An ear and an eye on the result: eight rigs at once, a track's preset, an export.
- A DAW to open the `.clap` in. The session cannot run one.

## Result of D1 to D8, 2026-10-13

Release tree, four threads. Two invocations of `sd build`: the first stopped at linking LAME,
whose export file names its decoder, so the decoder is now built in; the second was clean.

| Check | Result |
| --- | --- |
| `sd test -s unit` | 1274 of 1274 passed. |
| `sd test -s regression` | 23 of 23 passed. |
| `libmp3lame.dll` | Built and copied beside `sushidsp_host_gui.exe`. |

Not done, and not claimed:

- Nothing was listened to and the screen was not looked at: eight rigs at once, a track's
  preset, a track file and an export are unverified by ear and eye.
- The MP3 writer has no test: the test target is built without LAME, and there the export
  refuses MP3, which is what the test asserts.
- The benchmark was not run; the device callback now goes through `SessionRenderer`.

## Result of P1, 2026-10-13

The worker, on `opus` at `low`, wrote `applications/plugin` and the adapter's test without
building; its report carried the syntax checks of the adapter and the test, and said the CLAP
shell was unchecked, since no CLAP header was on the machine. The session wired CLAP 1.2.2 and
the plugin into the build.

| Check | Result |
| --- | --- |
| `sd build` | `SushiDSP.clap` compiled and linked against the real CLAP headers at the first build. |
| `sd test -s unit` | 1289 of 1289 passed, the adapter's fifteen among them. |

Not done: no CLAP host has loaded the plugin; nothing installs it; `RigAdapter::process` has no
allocation trap.
