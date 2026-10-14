# SushiDSP manual

The manual index. The root [README.md](../README.md) says what SushiDSP is; this page reaches
every document under `docs/` outside `agent/` and `archive/`, and every README kept beside the
code.

## Manual

| Document | Holds |
| --- | --- |
| [Getting started](getting_started/INTRODUCTION.md) | The first-run sequence, from installing `sd` to starting the host |
| [Architecture](architecture/README.md) | Where the two architecture pages begin |
| [Architecture overview](architecture/OVERVIEW.md) | The layers as the tree holds them today |
| [Modules](architecture/MODULES.md) | One link per module to its own README |
| [Command line](guides/COMMAND_LINE_INTERFACE.md) | Every `sd` subcommand, its flags and the compiler search |
| [FAQ](guides/FAQ.md) | The questions a newcomer asks, each answered from this manual |
| [Contributing](CONTRIBUTING.md) | How a change lands and what it must document |
| [Documentation style guide](DOCUMENTATION_STYLE_GUIDE.md) | How a document is written in this repository |
| [API reference landing page](reference/API_REFERENCE.md) | The page Doxygen opens the generated reference with |
| [Changelog](reference/CHANGELOG.md) | What changed, by release |
| [Glossary](reference/GLOSSARY.md) | The words these documents use in a sense the code fixes |
| [Known issues](reference/KNOWN_ISSUES.md) | Defects that are recorded and left open |
| [SPICE reference corpus](reference/spice/README.md) | The LTspice schematics and CSVs the circuit solver is nulled against, one README per block |

`sd docs` writes the generated API reference to `build/docs/api-site/html/`.

## Design

| Document | Holds |
| --- | --- |
| [Design map](design/README.md) | Which design document covers which topic, with each one's status |
| [Remaining work](design/REMAINING_WORK.md) | The single backlog |

## Beside the code

Each module's README is linked from [Modules](architecture/MODULES.md). The other READMEs:

| README | Holds |
| --- | --- |
| [`applications/host_gui`](../applications/host_gui/README.md) | The standalone host: its classes, its files and what it looks for at startup |
| [`applications/plugin`](../applications/plugin/README.md) | The CLAP plugin: the rig adapter, the shell and how its rig is chosen |
| [`cli`](../cli/README.md) | The `sd` package: install, layout, configuration and tests |
| [`assets/hrtf`](../assets/hrtf/README.md) | The bundled HRTF set, its source and its licence |
| [`assets/ir`](../assets/ir/README.md) | What a bundled cabinet impulse response must be |
| [`tools`](../tools/README.md) | The checkers, the license block writer and their tests |
| [`tools/dsp`](../tools/dsp/README.md) | The scripts that generate filter tables, the HRTF asset and the room fixtures |
| [`tools/spice`](../tools/spice/README.md) | The exporter that turns an LTspice run into the corpus CSVs |
| [`golden_compare`](../modules/foundation/golden_compare/README.md) | The golden files and every regeneration of one |
| [`tests/nodes/room/fixtures`, BRAS](../tests/nodes/room/fixtures/built_room_loss/bras_cr2_reference.README.md) | Where `bras_cr2_reference.csv` comes from |
| [`tests/nodes/room/fixtures`, Motus](../tests/nodes/room/fixtures/built_room_loss/motus_reference.README.md) | Where `motus_reference.csv` comes from |
| [`third_party/dr_libs`](../third_party/dr_libs/README.md) | The vendored MP3 decoder and its version |

The [code of conduct](../.github/CODE_OF_CONDUCT.md) and the
[security policy](../.github/SECURITY.md) are in `.github/`, where GitHub reads them.

## Agent work and the archive

Agent work folders are under `agent/`, one per piece of work. Finished material is under
`archive/` and is not edited. `archive/agent/` holds closed work folders; `archive/superpowers/`,
`archive/research/` and `archive/notes/` hold the plans, specs and dossiers written before the
work-folder shape existed.
