# SushiDSP documentation

The manual index. Start at the root [README.md](../README.md) for a general overview; this page indexes every document in the repository.

## Manual index

- **[Getting started](getting_started/INTRODUCTION.md)**: Overview of SushiDSP, the rig graph, and workspace setup.
- **[Design spec](design/SUSHIDSP.md)**: Architecture, circuit equations, SOLID boundaries, and the roadmap.
- **[Architecture](architecture/README.md)**: Current system layer layout and module boundaries.
- **[GUI/UX redesign](design/GUI_UX_REDESIGN.md)**: Main window layout, rig routing view, and gear browser grid.
- **[Room furnishings](design/ROOM_FURNISHINGS.md)**: Furniture as equivalent absorption in the room module, the layered room and how it is checked against measured rooms.
- **[Contributing](CONTRIBUTING.md)**: Engineering conventions: C++17, Allman braces, headers, and test requirements.
- **[Documentation style guide](DOCUMENTATION_STYLE_GUIDE.md)**: Documentation standards, layout, and document structure.
- **[Command line interface](guides/COMMAND_LINE_INTERFACE.md)**: Developer CLI (`sd`) subcommands, compilers, and presets.
- **[Remaining work](design/REMAINING_WORK.md)**: The single live backlog of open implementation items.
- **[Changelog](reference/CHANGELOG.md)**: Change history organized by roadmap milestone and release.
- **[Known issues](reference/KNOWN_ISSUES.md)**: Documented defect tracking, reproduction steps, and planned resolutions.
- **[Glossary](reference/GLOSSARY.md)**: Domain vocabulary definitions for timeline units, audio buffers, and rig slots.
- **[Code of conduct](../.github/CODE_OF_CONDUCT.md)** and **[Security policy](../.github/SECURITY.md)**: in `.github/`, which is where GitHub looks for them.
- **[Archive](archive/README.md)**: Historical plans, specs, and research dossiers.
- **[Modules index](architecture/MODULES.md)**: Inventory of project modules and their responsibilities.
- **[`docs/reference/spice/`](reference/spice/)**: Netlists, subcircuit definitions, and SPICE reference data.
- **[`include/SushiDSP/math/dk/`](../include/SushiDSP/math/dk/)**: Damped Newton-Raphson solver and Netlist builder.
- **[`include/SushiDSP/math/Resampler.hpp`](../include/SushiDSP/math/Resampler.hpp)**: Polyphase arbitrary-ratio resampler.
- **[`include/SushiDSP/circuits/models/`](../include/SushiDSP/circuits/models/)**: Device laws for triodes, pentodes, and clipping diodes.
- **[`modules/nodes/tubescreamer/`](../modules/nodes/tubescreamer/)**: Ibanez Tube Screamer pedal model.
- **[`modules/nodes/routing/`](../modules/nodes/routing/)**: Splitter, mixer, and pan nodes for rig graph routing.
- **[`modules/nodes/cabinet/`](../modules/nodes/cabinet/)**: Cabinet impulse-response convolution node.
- **[`modules/catalog/node_registry/`](../modules/catalog/node_registry/)**: Catalog registering available audio nodes.
- **[`modules/session/rig_preset/`](../modules/session/rig_preset/)**: JSON serialization and deserialization for rig graphs.
- **[`modules/nodes/dd3/`](../modules/nodes/dd3/)**: Boss DD-3 digital delay pedal model.
- **[`modules/nodes/peavey5150/`](../modules/nodes/peavey5150/)**, **[`modules/nodes/mesa_mark2c/`](../modules/nodes/mesa_mark2c/)**, and **[`modules/nodes/diezel_vh4/`](../modules/nodes/diezel_vh4/)**: High-gain amplifier models built from schematics in `docs/archive/research/`.
- **[`modules/nodes/capture/`](../modules/nodes/capture/)**: Synchronized audio capture and playback module.
- **[`modules/nodes/room/`](../modules/nodes/room/)**: Room acoustics simulation, early reflections, and modal resonance.
- **[`assets/hrtf/`](../assets/hrtf/)**: Head-related transfer function assets and converters.
- **[`tools/`](../tools/)**: Repository checkers and DSP filter design scripts.
- **[`tests/measure/`](../tests/measure/)**: Audio measurement library (THD, alias floor, frequency response).

## Generated API reference

Run `sd doxygen` to generate the HTML API documentation in `build/docs/api-site/html/` from [`docs/reference/API_REFERENCE.md`](reference/API_REFERENCE.md) and header comments.
