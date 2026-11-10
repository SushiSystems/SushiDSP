# Host

## Remaining work

| ID | Pri | State | Title | Done when | Blocked by | Source | Opened |
| --- | --- | --- | --- | --- | --- | --- | --- |
| RW-27 | P2 | open | Shift the pitch of track playback | the bottom bar's Pitch field shifts track playback by semitones and leaves speed alone, in a plan of its own (owner decision of 2026-09-14) |  | `docs/design/GUI_UX_REDESIGN/README.md` §12 | 2026-10-19 |
| RW-28 | P2 | unmeasured | Run the phase 2 manual acceptance | the seven checks of task 14 of the host gui phase2 plan in the version history have been run in the GUI |  |  | 2026-10-19 |
| RW-29 | P2 | open | Remove `SlotGraph::add_branch` | neither `SlotGraph` nor `RigController` carries it; it has no caller since the splitter picker went |  |  | 2026-10-19 |
| RW-30 | P2 | open | Carry surviving nodes across a rebuild | `SlotGraph::publish()` keeps the nodes a structural edit leaves in place, so the room's tail survives the edit and the node set is built once per edit |  |  | 2026-10-19 |
| RW-31 | P2 | open | Keep a stereo branch stereo through a mixer | a `PortLayout` can say eight slots of two channels and `MixerNode` keeps its inputs' width |  |  | 2026-10-19 |
| RW-32 | P2 | open | Export OGG, DS5 | a take leaves the host as OGG; a five-minute stereo take is 115 MB of float today |  |  | 2026-10-19 |
| RW-33 | P2 | open | Test the 39 units without a test file | `tools/testing/untested_units.txt` is empty: 31 units in `applications/host_gui`, 6 in `applications/plugin`, and the ASIO and SDL2 device backends |  |  | 2026-10-19 |
| RW-34 | P2 | blocked | Take the untested-unit exception from SushiSkills | the shared `check_test_mirror.py` matches SushiSkills', the list is read from `tools/settings.py`, and `tools/testing/untested_units.txt` is gone | sushiskills RW-20 |  | 2026-10-19 |
| RW-35 | P2 | open | Read a take's own sample rate | a clip carries its take's rate over the rig's into `VariRateReader` |  |  | 2026-10-19 |
| RW-36 | P2 | open | Cap the imported-audio cache | `cache/imported_audio/` has a size cap and evicts by last use |  |  | 2026-10-19 |
| RW-37 | P2 | open | Start a rebuilt strip where its level is | a structural edit leaves no 20 ms glide from a parameter's default to its value |  |  | 2026-10-19 |
| RW-38 | P2 | open | Give the post stage a stereo input | an effect can declare a stereo input, or the post stage runs one instance per channel |  |  | 2026-10-19 |
| RW-72 | P2 | open | Write `imgui.ini` outside the working directory | the host sets ImGui's ini path to a folder of its own |  |  | 2026-10-19 |
| RW-73 | P2 | open | Let the host find products without naming them | adding a product edits no file under `applications/host_gui/` |  |  | 2026-10-19 |

## Known issues

| ID | Origin | Symptom | Workaround | Fix | Source | Opened |
| --- | --- | --- | --- | --- | --- | --- |
