# General

## Remaining work

| ID | Pri | State | Title | Done when | Blocked by | Source | Opened |
| --- | --- | --- | --- | --- | --- | --- | --- |
| RW-1 | P2 | decision | Tag v0.1.0 or fold its heading back into Unreleased | the `## v0.1.0` heading of the changelog has a tag behind it, or is folded into `## Unreleased` |  |  | 2026-10-19 |
| RW-3 | P2 | decision | Choose one error model and the host's split | the owner has chosen one error model, where `[[nodiscard]]` goes, and how `RigController`, `SlotGraph` and `main()` divide the host |  | `docs/design/SUSHI_STANDARDS_MIGRATION/README.md` | 2026-10-19 |
| RW-4 | P2 | decision | Choose how the alias floor closes | the owner has chosen 16× oversampling, a band limit between the stages that lets 8× stay, or a higher-order antiderivative, and the amplifiers meet their bounds |  |  | 2026-10-19 |
| RW-5 | P2 | decision | Choose how the IIC+ and the 5150 fit the smallest buffer | the owner has chosen 4× for these models or a cheaper triode solve |  |  | 2026-10-19 |
| RW-7 | P2 | decision | Rule a shared rig-width rule for the two hosts | the owner has ruled it (spec section 12) |  | `docs/agent/2026_10_13_AMPLITUBE5_CHANNELS/SPEC.md` | 2026-10-19 |
| RW-9 | P2 | decision | Move the cabinet section's gate, or make the amplifier cheaper | three JCM800 chains into one room with eight microphones meet the gate, or the owner has moved the gate from half the block |  |  | 2026-10-19 |
| RW-10 | P2 | decision | Choose how a track removal stays atomic | a generation counter the callback reads once per block, or a refusal of the removal while the transport runs, is chosen and built |  |  | 2026-10-19 |
| RW-76 | P2 | decision | Decide whether `Wdf` stays | `Wdf` has a production user, or it leaves the library |  |  | 2026-10-19 |
| RW-80 | P2 | open | Declare nlohmann/json as a dependency and find it before fetching | cli/sushistack.deps.toml lists nlohmann-json and CMake finds the installed package first |  | `CMakeLists.txt` | 2026-10-30 |

## Known issues

| ID | Origin | Symptom | Workaround | Fix | Source | Opened |
| --- | --- | --- | --- | --- | --- | --- |
