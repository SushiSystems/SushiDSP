# Documentation layout

## Remaining work

| ID | Pri | State | Title | Done when | Blocked by | Source | Opened |
| --- | --- | --- | --- | --- | --- | --- | --- |
| RW-62 | P2 | open | Correct the changelog's places | the five placeless entries of 2026-09-05 and 2026-09-07 have places, and the entries citing a changelog folder that does not exist and three removed files… |  |  | 2026-10-19 |
| RW-63 | P2 | open | Read the pages the em dash pass changed | the CLI guide, the design spec's opening block and the pages the 2026-10-12 pass changed read in the `humanizer` register |  |  | 2026-10-19 |
| RW-67 | P2 | open | Move the SPICE transient CSVs out of the manual | `docs/reference/spice/*/*_tran.csv` move to a fixture folder with the tests and `tools/spice/export.py` that read and write them |  |  | 2026-10-19 |
| RW-68 | P2 | open | Correct the impulse response pages | `assets/ir/README.md` and `applications/host_gui/README.md` name what ships, and the admission rule names this repository's licence |  |  | 2026-10-19 |
| RW-69 | P2 | open | Generate the version | a generated header carries the version, `cli/pyproject.toml` reads it, and `sushi-module.toml` states it |  |  | 2026-10-19 |
| RW-70 | P2 | open | Move the root build file's logic into `cmake/` | the root `CMakeLists.txt` defines no target and fetches no dependency itself |  |  | 2026-10-19 |
| RW-71 | P2 | open | Clean the ignore file | `.gitignore` names nothing nobody writes, says `sd docs`, and carries rules, not paragraphs |  |  | 2026-10-19 |
| RW-74 | P2 | open | Keep `nlohmann::json` out of the preset module's interface | `rig_preset.hpp` includes no JSON header |  |  | 2026-10-19 |
| RW-75 | P2 | open | Give the four amplifier modules their shared helpers once | the helpers and constants `jcm800.cpp`, `peavey5150.cpp`, `diezel_vh4.cpp` and `mesa_mark2c.cpp` repeat live in one module |  |  | 2026-10-19 |

## Known issues

| ID | Origin | Symptom | Workaround | Fix | Source | Opened |
| --- | --- | --- | --- | --- | --- | --- |
| KI-34 | own | Changelog entries cite places that do not exist or none | read `git log` | RW-62 |  | 2026-10-19 |
| KI-37 | own | Two READMEs cite an impulse response that does not ship | give the host an impulse response of your own | RW-68 |  | 2026-10-19 |
| KI-38 | own | No document states the test count | run `sd test` | RW-60 |  | 2026-10-19 |
