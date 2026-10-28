# Host sessions

## Remaining work

| ID | Pri | State | Title | Done when | Blocked by | Source | Opened |
| --- | --- | --- | --- | --- | --- | --- | --- |

## Known issues

| ID | Origin | Symptom | Workaround | Fix | Source | Opened |
| --- | --- | --- | --- | --- | --- | --- |
| KI-19 | own | Removing a track can double one block of audio | remove tracks while the transport is stopped | RW-10 |  | 2026-10-19 |
| KI-23 | own | The 2-into-1 downmix has no live caller | none needed | none, the branch completes the adaptation table for a rig whose width is fixed |  | 2026-10-19 |
| KI-24 | own | `Transport::seek` racing `advance` can be lost | serialise the two, as the host and `seek`'s `@pre` do | none, no caller needs a generation counter |  | 2026-10-19 |
| KI-25 | own | A take's own sample rate is never read | convert a WAV to the rig's rate before dropping it | RW-35 |  | 2026-10-19 |
| KI-26 | own | The imported-audio cache is never pruned | delete the folder between sessions | RW-36 |  | 2026-10-19 |
| KI-27 | own | A take leaves the host as WAV only | convert outside the host | RW-32 |  | 2026-10-19 |
| KI-28 | own | The host writes `imgui.ini` into the working directory | start the host from its own folder | RW-72 |  | 2026-10-19 |
