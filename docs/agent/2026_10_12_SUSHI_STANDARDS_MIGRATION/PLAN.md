# Plan: SushiSkills conformance migration

Each phase has one acceptance criterion and is checked before the next one starts. Work is done
inline in the main checkout, one owner, no commits until the owner asks. No `sd build` or
`sd test` runs without the owner's approval; compile checks use `clang++ -fsyntax-only` on each
translation unit with the flags recorded in `build/compile_commands.json`.

| # | Phase | Acceptance criterion |
| --- | --- | --- |
| 1 | Root configuration: `.clang-format`, `.editorconfig`, `.config/doxygen/Doxyfile` with its `.config/README.md`, `Doxyfile` removed from the root | `Doxyfile` is absent from the root; the `sd` doc command points at the new path; the CLI tests that name the path pass |
| 2 | Core into `modules/foundation/`: moves, `source/`, per-module `CMakeLists.txt`, `cmake/SushiDSPLayers.cmake`, a test that `K_TIER_ORDER` matches it | `check_layering.py` reports 0; every `#include <SushiDSP/...>` resolves to a header of a declared module; every CMake source exists |
| 3 | `apps/` to `applications/` and the `apps/host*` paths in `CMakeLists.txt`, `cli/`, docs | `grep` finds no `apps/` path in tracked files outside `docs/agent/` and `archive/` |
| 4 | Renames: snake_case file names, `I`-prefix removal, `K_UPPER_SNAKE` constants, the generated resampler header and its generator | Each renamed symbol and file has no old name left in tracked text; `clang++ -fsyntax-only` passes for every translation unit |
| 5 | `.clang-format` applied: 100 columns, Allman, 4-space indent | `clang-format --dry-run --Werror` reports nothing for tracked C++ |
| 6 | Checkers: vendor `check_docs_truth.py` and `.docs_truth_allow`; add `check_source_comments.py` to `sd check` | Every checker in `tools/` and the five in `project-tools` run and report what they report, with the output pasted in REPORT.md |
| 7 | CI through `sd`; docs and module READMEs for the new layout; changelog, REMAINING_WORK and KNOWN_ISSUES updated | `check_docs_layout.py`, `check_changelog.py` and `check_docs_truth.py` pass |

Not in this plan: the error model, `[[nodiscard]]`, and the `RigController` and `SlotGraph`
split. They are recorded as open items with the reason they wait for the owner.

## Phase 2 detail

Area to module mapping (one module per area, all in tier `foundation`):

| Old | New module |
| --- | --- |
| `include/SushiDSP/<area>/` and `src/<area>/` | `modules/foundation/<area>/include/SushiDSP/<area>/` and `modules/foundation/<area>/source/` |
| `include/SushiDSP/README.md`, `src/README.md` | `modules/foundation/README.md` (tier) and one README per module |
| `src/device/Sdl2AudioDevice.cpp`, `AsioAudioDevice.cpp` | `modules/foundation/device/source/`, built by targets in the device module, each behind its option |
| `src/math/ResamplerKernel.hpp` | `modules/foundation/math/source/resampler_kernel.hpp` (generated; the generator writes the new path) |

The include path `<SushiDSP/io/Ir.hpp>` stays the same, because each module's `include/` root
holds its own `SushiDSP/<area>/` folder. Only the file locations change.

Tier order, lowest first: `foundation`, `nodes`, `catalog`, `session`. The tier table lives in
`cmake/SushiDSPLayers.cmake`; `check_layering.py` keeps its own copy, and a test compares them.
