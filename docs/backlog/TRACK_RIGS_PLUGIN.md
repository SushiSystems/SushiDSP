# Track rigs plugin

## Remaining work

| ID | Pri | State | Title | Done when | Blocked by | Source | Opened |
| --- | --- | --- | --- | --- | --- | --- | --- |
| RW-45 | P2 | open | Remove the re-amp leg | `SessionTimeline` and `SlotGraph` carry no re-amp player, track or input-source choice, and their tests go |  | `docs/agent/2026_10_13_TRACK_RIGS_AND_PLUGIN/SPEC.md` | 2026-10-19 |
| RW-46 | P2 | open | Give a track's strip an EQ | the strip has three bands, as AmpliTube 5's does |  |  | 2026-10-19 |
| RW-47 | P2 | open | Edit clips | a clip can be split, copied, pasted, cropped, faded and gain-trimmed, and a punch region exists |  |  | 2026-10-19 |
| RW-48 | P2 | open | Export 24-bit WAV and AIFF | `WavWriter` writes 24-bit and an AIFF writer exists |  |  | 2026-10-19 |
| RW-49 | P2 | open | Export MP3 on Linux | `cmake/SushiDSPLame.cmake` configures LAME on Linux |  |  | 2026-10-19 |
| RW-50 | P2 | open | Keep a removed track's lane out of its neighbour's rig | removing a track moves the lanes and the table of rigs in one step |  |  | 2026-10-19 |
| RW-51 | P2 | open | Skip a rig that has nothing to play | eight idle rigs cost less than eight times one |  |  | 2026-10-19 |
| RW-52 | P2 | open | Make the host's rig sources a library | the tests and the plugin link a library in place of naming 24 `applications/host_gui` sources |  |  | 2026-10-19 |
| RW-53 | P2 | unmeasured | Load the plugin in a DAW and install it | a CLAP host has loaded `SushiDSP.clap` and an install puts it in a CLAP folder |  |  | 2026-10-19 |
| RW-54 | P2 | open | Give the plugin an editor and a VST3 shell | both exist |  |  | 2026-10-19 |
| RW-55 | P2 | open | Give the plugin's rooms the HRTF set | a plugin rig loads the measured set, not the spherical head |  |  | 2026-10-19 |
| RW-56 | P2 | open | Apply a plugin parameter at its event's sample | parameters apply at the event, and `RigController` maps a slot to its node |  |  | 2026-10-19 |
| RW-57 | P2 | open | Trap an allocation in `RigAdapter::process` | the allocation trap of `test_rt_safety.cpp` is shared and covers the adapter |  |  | 2026-10-19 |
| RW-58 | P2 | open | Model the cabinet's drivers | a driver's resonance, its impedance as the power amplifier's load, cone breakup and compression at level are modelled |  | `docs/agent/2026_10_13_CABINET_DRIVERS_AND_FURNITURE/SPEC.md` | 2026-10-19 |

## Known issues

| ID | Origin | Symptom | Workaround | Fix | Source | Opened |
| --- | --- | --- | --- | --- | --- | --- |
