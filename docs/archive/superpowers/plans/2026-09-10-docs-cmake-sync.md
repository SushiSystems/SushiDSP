# Docs and CMake Sync Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Close the documentation-architecture and CMake-module gap between sushidsp and its sibling `sushiengine`, without importing machinery sushidsp has no use for.

**Architecture:** Two independent waves. Wave 1 adds the six missing `docs/` sections (`DOCUMENTATION_STYLE_GUIDE.md`, `getting_started/`, `architecture/`, `modules/`, `guides/`, `archive/`), each a standalone file/directory with no cross-task dependency except the final index update. Wave 2 adds the three missing CMake concerns (`CompilerWarnings.cmake`, `Sanitizers.cmake`, `CMakePresets.json`) as new files wired into the existing `CMakeLists.txt`; the existing `SushiDSPCompilerFlags.cmake` is left untouched — it is already correctly scoped and does not need splitting. Wave 3 is the single task that updates `docs/README.md`'s index to point at everything Wave 1 created.

**Tech Stack:** Markdown (docs), CMake (build config). No C++ changes.

**Spec:** This plan implements the gap list from the 2026-09-10 audit (two research passes recorded in this session, not yet written to a spec file — the gap itself is the spec: sushidsp's `docs/` and `cmake/` should carry the same architecture CLAUDE.md prescribes and `sushiengine` already has, sized to what sushidsp actually needs).

## Global Constraints

- Every prose file goes through the `humanizer` skill before being finalized (per the user's global CLAUDE.md).
- English for all files (per global CLAUDE.md language rule).
- Never invoke `cmake`/`ctest` directly; CMake changes are verified by asking the user to run `sd build` at the end (per project CLAUDE.md — never build without asking).
- Do not clone `sushiengine`'s 13-file CMake layout wholesale — `Module.cmake`, `EngineLayers.cmake`, `Runtime.cmake`, `SyclTarget.cmake`, `NativeExecutionTarget.cmake`, `Shaders.cmake`, `PrecompiledHeaders.cmake`, `InstallContent.cmake`, `Toolchain.cmake`, `Vcpkg.cmake`, `Platform.cmake`, `Install.cmake` have no sushidsp equivalent (no module-tier system, no runtime subproject, no shaders, compiler already selected via `cli/config.toml`) and must not be added.
- Every new manual page ends up linked from `docs/README.md` — an orphaned doc is a plan defect.
- A changelog entry (one line, past tense, `docs/reference/CHANGELOG.md`) is added for this work as its own step, per global CLAUDE.md's changelog rule.

---

## Wave 1 — Documentation skeleton (5 tasks, run in parallel; disjoint files)

### Task 1: `docs/DOCUMENTATION_STYLE_GUIDE.md`

**Files:**
- Create: `docs/DOCUMENTATION_STYLE_GUIDE.md`

**Interfaces:**
- Consumes: the prose conventions already visible in `docs/README.md` and `docs/CONTRIBUTING.md` (voice, heading style, link style) — read both before writing so the new file describes what this repo already does, not a generic template.
- Produces: nothing later tasks call, but Wave 3 links to it from `docs/README.md`.

- [ ] **Step 1:** Read `sushiengine`'s `docs/DOCUMENTATION_STYLE_GUIDE.md` in full for section shape (do not copy sushiengine-specific content — e.g. its module-tier or shader conventions do not apply here).
- [ ] **Step 2:** Read `D:\Projects\sushidsp\docs\README.md` and `docs/CONTRIBUTING.md` fully to extract sushidsp's actual conventions: sentence-per-changelog-line rule, `**Bold lead-in** — description` list style, design-doc status lines, the "never describes the tree as it stands" caveat for `docs/design/`, doxygen comment rules from the root `CLAUDE.md`.
- [ ] **Step 3:** Write `docs/DOCUMENTATION_STYLE_GUIDE.md` covering: heading levels, the `**[Title](path)** — description` index-entry format used throughout `docs/README.md`, the changelog one-line/240-char rule, the design-doc status-line convention, the doxygen comment shape (`@file`/`@brief`/`@author`, 8-line block cap, no `@date`), and where prose review (`humanizer`) applies.
- [ ] **Step 4:** Run the file's prose through the `humanizer` skill.
- [ ] **Step 5:** Commit.

```bash
git add docs/DOCUMENTATION_STYLE_GUIDE.md
git commit -m "docs: add documentation style guide"
```

### Task 2: `docs/getting_started/INTRODUCTION.md`

**Files:**
- Create: `docs/getting_started/INTRODUCTION.md`

**Interfaces:**
- Consumes: the root `README.md` and `docs/CONTRIBUTING.md`'s build section (the `sd` CLI usage).
- Produces: nothing later tasks call.

- [ ] **Step 1:** Read `sushiengine`'s `docs/getting_started/INTRODUCTION.md` for section shape only.
- [ ] **Step 2:** Read the root `D:\Projects\sushidsp\README.md` and `docs/CONTRIBUTING.md`'s build/`sd` sections.
- [ ] **Step 3:** Write `docs/getting_started/INTRODUCTION.md`: what sushidsp is (one paragraph, matching the root README's framing), how to get a workspace running (`sd build`, `sd test`, `sd doxygen` — one line each, pointing to `docs/CONTRIBUTING.md` for the full rules rather than restating them), and where to go next (link to `docs/README.md`).
- [ ] **Step 4:** Run the file's prose through the `humanizer` skill.
- [ ] **Step 5:** Commit.

```bash
git add docs/getting_started/INTRODUCTION.md
git commit -m "docs: add getting-started introduction"
```

### Task 3: `docs/architecture/OVERVIEW.md` and `docs/architecture/README.md`

**Files:**
- Create: `docs/architecture/README.md`
- Create: `docs/architecture/OVERVIEW.md`

**Interfaces:**
- Consumes: `docs/design/sushidsp.md` (the authoritative spec) for the actual module map — do not invent architecture, extract it from the spec's existing section headings.
- Produces: nothing later tasks call.

- [ ] **Step 1:** Read `sushiengine`'s `docs/architecture/README.md` and one domain file for section shape only.
- [ ] **Step 2:** Read `docs/design/sushidsp.md`'s table of contents / top-level section headings to identify the real module boundaries (DK circuit core, node graph, modules/, math/, device layer).
- [ ] **Step 3:** Write `docs/architecture/README.md` as a one-paragraph index pointing to `OVERVIEW.md` and to the per-module `README.md`s already linked from `docs/README.md` (do not duplicate those links — say "see docs/README.md's module list").
- [ ] **Step 4:** Write `docs/architecture/OVERVIEW.md`: the layering as it exists today (device layer → node graph → circuit/math core), stated as fact about the current tree, distinct from `docs/design/sushidsp.md` which states intent. Cite `docs/design/sushidsp.md` by section for anything speculative rather than restating it.
- [ ] **Step 5:** Run both files' prose through the `humanizer` skill.
- [ ] **Step 6:** Commit.

```bash
git add docs/architecture/README.md docs/architecture/OVERVIEW.md
git commit -m "docs: add architecture overview"
```

### Task 4: `docs/modules/README.md`

**Files:**
- Create: `docs/modules/README.md`

**Interfaces:**
- Consumes: the existing `modules/*/README.md` files (already written, per-module facts) — this is an index over them, not a rewrite.
- Produces: nothing later tasks call.

- [ ] **Step 1:** List `D:\Projects\sushidsp\modules\*` and confirm each has its own `README.md` (per `docs/README.md`'s existing links: tubescreamer, routing, node_registry, rig_preset, dd3, peavey5150, mesa_mark2c, diezel_vh4, capture, room).
- [ ] **Step 2:** Write `docs/modules/README.md` as a one-paragraph explanation of what a "module" is in this tree (a product or product-shaped feature under `modules/`, each with its own `README.md` beside its code) plus a link list to each one, matching the `**[Title](path)** — description` format from `docs/README.md`.
- [ ] **Step 3:** Run the file's prose through the `humanizer` skill.
- [ ] **Step 4:** Commit.

```bash
git add docs/modules/README.md
git commit -m "docs: add modules index"
```

### Task 5: `docs/guides/COMMAND_LINE_INTERFACE.md` and `docs/archive/README.md`

**Files:**
- Create: `docs/guides/COMMAND_LINE_INTERFACE.md`
- Create: `docs/archive/README.md`

**Interfaces:**
- Consumes: `cli/config.toml` and the `sd` subcommands referenced in `docs/CONTRIBUTING.md` (`sd build`, `sd test`, `sd doxygen`).
- Produces: nothing later tasks call.

- [ ] **Step 1:** Read `cli/config.toml` and `docs/CONTRIBUTING.md`'s `sd` references to confirm the full current subcommand list and the compiler-resolution order (explicit config → SushiStack-provisioned intel/llvm clang++ → PATH clang++ → CMake default).
- [ ] **Step 2:** Write `docs/guides/COMMAND_LINE_INTERFACE.md`: one section per `sd` subcommand actually in use (`build`, `test`, `doxygen`, any others found in Step 1), each with its flags and what it wraps, plus a section documenting the compiler-resolution order so "which compiler is actually building this" is answered in the manual instead of requiring a `CMakeCache.txt` read.
- [ ] **Step 3:** Write `docs/archive/README.md`: one paragraph stating this directory is frozen (per global CLAUDE.md: "added to, never edited") and is currently empty because sushidsp has no closed-out design documents yet; do not backfill it with content that belongs in `docs/design/`.
- [ ] **Step 4:** Run both files' prose through the `humanizer` skill.
- [ ] **Step 5:** Commit.

```bash
git add docs/guides/COMMAND_LINE_INTERFACE.md docs/archive/README.md
git commit -m "docs: add CLI guide and archive placeholder"
```

---

## Wave 2 — CMake gap (3 tasks, run in parallel; disjoint files)

### Task 6: `cmake/CompilerWarnings.cmake`

**Files:**
- Create: `cmake/CompilerWarnings.cmake`
- Modify: `CMakeLists.txt` (add one `include()` line near the existing `include(cmake/SushiDSPCompilerFlags.cmake)` at line 12)

**Interfaces:**
- Consumes: `CMAKE_CXX_COMPILER_ID` (already branched on in `SushiDSPCompilerFlags.cmake` — follow the same Clang/MSVC/GNU branching pattern).
- Produces: a function or variable applying warning flags to the main target — match whatever mechanism `SushiDSPCompilerFlags.cmake` uses today (inspect it before writing; if it sets flags on a target via `target_compile_options`, do the same here rather than inventing a second mechanism).

- [ ] **Step 1:** Read `cmake/SushiDSPCompilerFlags.cmake` in full to copy its exact pattern for branching on `CMAKE_CXX_COMPILER_ID` and applying flags (target-scoped vs global).
- [ ] **Step 2:** Write `cmake/CompilerWarnings.cmake`: for Clang, `-Wall -Wextra -Wpedantic -Wshadow -Wconversion -Wsign-conversion`; for MSVC, `/W4`. Apply with the same mechanism found in Step 1. Do not enable `-Werror` — that is a separate policy decision the plan does not make; leave a one-line comment citing this plan file if a rationale must be recorded, or omit the comment if the flag list is self-explanatory (per global CLAUDE.md's comment rules, no comment unless the WHY is non-obvious).
- [ ] **Step 3:** Add `include(cmake/CompilerWarnings.cmake)` to `CMakeLists.txt` immediately after the existing `include(cmake/SushiDSPCompilerFlags.cmake)` line.
- [ ] **Step 4:** Ask the user to run `sd build` to confirm the new warning flags compile clean or to see what they surface (per project CLAUDE.md: never build without asking).
- [ ] **Step 5:** Commit (after the user confirms the build result — if warnings surface in existing code, that is a separate follow-up task, not part of this one).

```bash
git add cmake/CompilerWarnings.cmake CMakeLists.txt
git commit -m "build: add compiler warnings module"
```

### Task 7: `cmake/Sanitizers.cmake`

**Files:**
- Create: `cmake/Sanitizers.cmake`
- Modify: `CMakeLists.txt` (add one `include()` line)

**Interfaces:**
- Consumes: a build-type or option variable to gate sanitizers behind (e.g. only enable when `CMAKE_BUILD_TYPE` is `Debug` or a dedicated `SUSHIDSP_ENABLE_SANITIZERS` option — check whether `sd` already exposes a sanitizer flag in `cli/config.toml` before inventing a new one).
- Produces: `-fsanitize=address,undefined` (Clang) added to compile/link flags for the main target, opt-in only — never applied to the RT-safety trap tests' Release-mode benchmarks, since ASan changes timing and could itself allocate, defeating the RT-safety intent.

- [ ] **Step 1:** Read `cli/config.toml` and `docs/CONTRIBUTING.md` for any existing sanitizer convention or flag before adding a new one.
- [ ] **Step 2:** Write `cmake/Sanitizers.cmake`: an `option(SUSHIDSP_ENABLE_SANITIZERS "Build with ASan+UBSan" OFF)`, applied only when Clang and only when the option is on, via `target_compile_options`/`target_link_options` on the same target `SushiDSPCompilerFlags.cmake` targets.
- [ ] **Step 3:** Add `include(cmake/Sanitizers.cmake)` to `CMakeLists.txt` after `cmake/CompilerWarnings.cmake`.
- [ ] **Step 4:** Ask the user to run `sd build -DSUSHIDSP_ENABLE_SANITIZERS=ON` (or the `sd`-equivalent flag-passing syntax from `docs/CONTRIBUTING.md`) to confirm it configures and builds.
- [ ] **Step 5:** Commit.

```bash
git add cmake/Sanitizers.cmake CMakeLists.txt
git commit -m "build: add opt-in sanitizer module"
```

### Task 8: `CMakePresets.json`

**Files:**
- Create: `CMakePresets.json`

**Interfaces:**
- Consumes: the existing `build/CMakeCache.txt` configuration (compiler path, generator) confirmed in the earlier audit, and `sushiengine`'s `CMakePresets.json` for the schema/version shape only — not its preset names, which are engine-specific.
- Produces: named presets (`debug`, `release`) that `sd` or a raw `cmake --preset` invocation can select; this does not replace `cli/config.toml`'s compiler-resolution order, it just gives `cmake --preset` users the same defaults `sd` already picks.

- [ ] **Step 1:** Read `sushiengine`'s `CMakePresets.json` for `"version"` field and preset schema shape only.
- [ ] **Step 2:** Read `build/CMakeCache.txt` for the exact `CMAKE_CXX_COMPILER` path already in use, and `CMakeLists.txt` for the project's minimum CMake version (to set a compatible `"version"`/`"cmakeMinimumRequired"`).
- [ ] **Step 3:** Write `CMakePresets.json` with two configurePresets, `debug` and `release`, each setting `CMAKE_BUILD_TYPE` and pointing `CMAKE_CXX_COMPILER` at the same intel/llvm clang++ path `sd` already resolves to, so a plain `cmake --preset debug` gets the identical toolchain `sd build` would pick.
- [ ] **Step 4:** Ask the user to run `cmake --list-presets` (not a build — just listing) to confirm the file parses, or `sd build` if they'd rather verify end-to-end.
- [ ] **Step 5:** Commit.

```bash
git add CMakePresets.json
git commit -m "build: add CMake presets matching sd's default toolchain"
```

---

## Wave 3 — Index and changelog (1 task, depends on all of Wave 1)

### Task 9: Update `docs/README.md` index and `docs/reference/CHANGELOG.md`

**Files:**
- Modify: `docs/README.md`
- Modify: `docs/reference/CHANGELOG.md`

**Interfaces:**
- Consumes: every file path created in Wave 1 (Tasks 1–5) — this task cannot start until all five have landed.
- Produces: nothing later tasks call; this is the plan's terminal task.

- [ ] **Step 1:** Add one `**[Title](path)** — description` line to `docs/README.md`'s "Where to go" section for each of: `DOCUMENTATION_STYLE_GUIDE.md`, `getting_started/INTRODUCTION.md`, `architecture/README.md`, `modules/README.md`, `guides/COMMAND_LINE_INTERFACE.md`, `archive/README.md` — matching the existing bullet format exactly (see `docs/README.md:8-25` for the pattern).
- [ ] **Step 2:** Add one changelog line to `docs/reference/CHANGELOG.md`: `- 2026-09-10 — Added the missing documentation skeleton and CMake warnings/sanitizer/presets modules (\`docs/architecture/\`, \`docs/getting_started/\`, \`docs/guides/\`, \`docs/modules/\`, \`docs/archive/\`, \`docs/DOCUMENTATION_STYLE_GUIDE.md\`, \`cmake/CompilerWarnings.cmake\`, \`cmake/Sanitizers.cmake\`, \`CMakePresets.json\`).` — split into two lines if it exceeds 240 characters per the changelog rule; verify with a character count before committing.
- [ ] **Step 3:** Click through every link added in Step 1 (open each target file) to confirm none are broken.
- [ ] **Step 4:** Run the diff's prose through the `humanizer` skill.
- [ ] **Step 5:** Commit.

```bash
git add docs/README.md docs/reference/CHANGELOG.md
git commit -m "docs: index the new documentation sections"
```

---

## Self-Review

- **Spec coverage:** All six missing docs sections (Task 1–5) and all three missing CMake concerns (Task 6–8) from the audit are covered. Index update (Task 9) closes the loop so nothing is orphaned.
- **Placeholder scan:** No TBD/TODO; every step names the exact file and exact content source to read from.
- **Deliberately out of scope:** splitting `SushiDSPCompilerFlags.cmake` (already well-scoped, per the second research pass), and any of sushiengine's module-tier/runtime/shader CMake files (no sushidsp equivalent) — both stated explicitly in Global Constraints so no executor re-adds them.
