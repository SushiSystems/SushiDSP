# Notices

SushiDSP is Copyright (c) 2026-present Mustafa Garip & Sushi Systems and licensed under the PolyForm Noncommercial
License 1.0.0; see `LICENSE`. Commercial use needs a licence from Sushi Systems; see
`COMMERCIAL.md`. The material below keeps its own licence.

## Third-party components

Fetched by CMake `FetchContent` at configure time. None is tracked in this repository. Each
licence was read from the licence file in the fetched source.

| Component | Where | Source | Version | Licence |
| --- | --- | --- | --- | --- |
| SDL | fetched by `CMakeLists.txt`; linked by `applications/host` | https://github.com/libsdl-org/SDL | release-2.30.0 | zlib, Copyright (C) 1997-2024 Sam Lantinga |
| nlohmann/json | fetched by `CMakeLists.txt` | https://github.com/nlohmann/json | v3.11.3 | MIT, Copyright (c) 2013-2022 Niels Lohmann |
| GLFW | fetched by `CMakeLists.txt`; linked by `applications/host_gui` | https://github.com/glfw/glfw | 3.4 | zlib, Copyright (c) 2002-2006 Marcus Geelnard, Copyright (c) 2006-2019 Camilla Löwy |
| Dear ImGui | fetched by `CMakeLists.txt`; linked by `applications/host_gui` | https://github.com/ocornut/imgui | v1.90.9 | MIT, Copyright (c) 2014-2024 Omar Cornut |
| GoogleTest | fetched by `tests/CMakeLists.txt` | https://github.com/google/googletest | v1.14.0 | BSD-3-Clause, Copyright 2008, Google Inc. |

GoogleTest is fetched for the test suite and is not part of a shipped build.

Vendored in the tree:

| Component | Where | Source | Version | Licence |
| --- | --- | --- | --- | --- |
| dr_libs (`dr_mp3.h`) | `third_party/dr_libs/` | https://github.com/mackron/dr_libs | mp3-0.7.3 | Unlicense or MIT No Attribution, Copyright 2023 David Reid; derived from minimp3 (https://github.com/lieff/minimp3), CC0; text in `third_party/dr_libs/LICENSE.txt` |

Installed for the command line and the tools, not tracked and not linked into a build:

| Component | Where | Source | Version | Licence |
| --- | --- | --- | --- | --- |
| sushicore | dependency in `cli/pyproject.toml` | https://github.com/SushiSystems/SushiCore | 0.7.0 or later | A Sushi Systems repository, under its own licence |
| typer | dependency in `cli/pyproject.toml` | https://pypi.org/project/typer/ | 0.12 or later | MIT, from the package metadata |
| rich | dependency in `cli/pyproject.toml` | https://pypi.org/project/rich/ | 13.0 or later | MIT, from the package metadata |
| tomli | dependency in `cli/pyproject.toml`, Python below 3.11 | https://pypi.org/project/tomli/ | 2.0 or later | MIT, from the package metadata |
| numpy | imported by `tools/dsp/` | https://pypi.org/project/numpy/ | not pinned | BSD-3-Clause AND 0BSD AND MIT AND Zlib AND CC0-1.0, from the package metadata |
| scipy | imported by `tools/dsp/` | https://pypi.org/project/scipy/ | not pinned | BSD-3-Clause, from the package metadata |
| pytest | runs `cli/tests/` | https://pypi.org/project/pytest/ | not pinned | MIT, from the package metadata |
| intel/llvm | optional compiler in `cli/sushistack.deps.toml` | https://github.com/intel/llvm | not pinned | unverified; the upstream licence file was not read |

## Steinberg ASIO SDK

The ASIO SDK is not tracked and not distributed with this repository; `.gitignore` excludes
`third_party/asio/`. A developer who places the SDK there (ASIO SDK 2.3, from
www.steinberg.net/en/company/developers.html) gets a Windows build that compiles three of its files
into the `sushi_dsp_asio` library, which `applications/host_asio` and `applications/host_gui` link.

The SDK is licensed by Steinberg Media Technologies GmbH under the proprietary Steinberg ASIO
License or, alternatively, the GNU General Public License version 3. A build with ASIO falls
under Steinberg's terms. No binary built with ASIO is distributed until a licence agreement
signed by Steinberg exists.

ASIO is a registered trademark of Steinberg Media Technologies GmbH.

## Ported files

None recorded. No source file in the tree carries an upstream copyright line.

## Redistributed binaries

None. The repository tracks no executable or library.

## Third-party data

| Data | Where | Source | Version | Licence |
| --- | --- | --- | --- | --- |
| SADIE II head-related transfer functions, converted by `tools/dsp/convert_hrtf.py` | `assets/hrtf/sadie2_ku100_48k.hrtf` | https://www.york.ac.uk/sadie-project/database.html | subject D1 (Neumann KU100), 48 kHz, 256-tap SOFA | Apache-2.0, Copyright 2018, University of York; text in `third_party/licenses/Apache-2.0.txt`; notice and citation in `assets/hrtf/README.md` |
| T30 figures computed from the Benchmark for Room Acoustical Simulation (BRAS), room CR2 | `tests/fixtures/bras_cr2_reference.csv` | doi 10.14279/depositonce-6726.3 | version 3 of the deposit | CC-BY-SA-4.0, as `tests/fixtures/bras_cr2_reference.README.md` states it |
| Decay figures computed from the Motus dataset | `tests/fixtures/motus_reference.csv` | doi 10.5281/zenodo.4923187 | Zenodo record 4923187 | CC-BY-4.0, as `tests/fixtures/motus_reference.README.md` states it |

The converted HRTF file and the two fixtures stay under the licences in the table.

## Schematic sources

| Work | Where | Source | Licence of the source |
| --- | --- | --- | --- |
| TS808 tone and level stage, traced | `docs/reference/spice/ts_tone/ts_tone.asc` | ElectroSmash, TS808 tone/volume schematic; R.G. Keen, "The Technology of the Tube Screamer" (geofex.com) | unverified |
| Mark IIC+ component values | `modules/nodes/mesa_mark2c/`, `modules/foundation/circuits/include/SushiDSP/circuits/power_amp_feedback.hpp` | jrb32's trace of a chassis; the 1983 Mesa/Boogie factory drawing | unverified |

## Trademarks

SushiDSP models the behaviour of guitar amplifiers and effects that other companies designed
and sell. Every manufacturer and product name in this repository, in its source, its
documentation and its user interface, names the equipment a model was written against and
nothing else.

Mesa/Boogie, Mark IIC+ and Simul-Class are trademarks of Mesa/Boogie Ltd. Marshall and JCM800
are trademarks of Marshall Amplification plc. Peavey and 5150 are trademarks of Peavey
Electronics Corporation. Diezel and VH4 are trademarks of Diezel Amplification GmbH. Ibanez and
Tube Screamer are trademarks of Hoshino Gakki Co. Ltd. Boss and DD-3 are trademarks of Roland
Corporation. Yamaha and SPX90 are trademarks of Yamaha Corporation.

None of these companies endorses, sponsors or is affiliated with SushiDSP, and no claim is made
to any of their marks. The names appear because a person reading the code has to know which
circuit each file describes.
