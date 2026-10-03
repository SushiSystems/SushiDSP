# Sushi Systems Programming Standards Migration

**Status:** Open — phase 1 of 3

## Overview

This document specifies the migration plan to align SushiDSP with Sushi Systems Programming
standards. The migration follows a phased rollout to preserve continuous build stability and test
verification across all 969 baseline unit tests and host applications.

## Phase 1: Foundational Tooling and Documentation Layout

Phase 1 establishes the canonical verification checkers, root pointer files, and standard
`docs/` directory structure.

### 1. Canonical Verification Tooling

The canonical checkers from `sushisystemsprogramming/tools` are vendored into `tools/`:
- `tools/common/checker.py`: Shared checker framework and finding reporter.
- `tools/documentation/check_source_comments.py`: Source comment and Doxygen block checker.
- `tools/documentation/check_docs_layout.py`: Documentation tree and naming validator.
- `tools/documentation/check_changelog.py`: CHANGELOG entry formatting and line validator.
- `tools/layering/check_layering.py`: Layer dependency validator.
- `tools/README.md`: Guide describing checker execution, options, and contract.

The `sd` developer CLI (`cli/sushidsp/cli.py`) adds a `check` subcommand:
```
sd check [--report] [--rule NAME]
```
Running `sd check` executes `check_docs_layout.py`, `check_changelog.py`, and `check_layering.py`,
returning a non-zero exit code if any checker reports violations.

### 2. Root Directory and Agent Entry Points

The root directory conforms to `repository-layout` specifications:
- `docs/CLAUDE.md`: Created to house agent guidelines, architecture rules, and the Sushi doctrine.
- `docs/AGENTS.md`: Created as a single-line pointer: `See docs/CLAUDE.md.`
- `CLAUDE.md`: Updated to a single-line pointer: `See docs/CLAUDE.md.`
- `AGENTS.md`: Created at the repository root as a single-line pointer: `See docs/CLAUDE.md.`
- Developer-local artifacts at the repository root (`*.png`, `*.wav`, `imgui.ini`) are handled by
  the repository owner and remain ignored in `.gitignore`.

Commit `cfcb43f` later moved the two instruction files out of `docs/` to the repository root, so
`CLAUDE.md` and `AGENTS.md` now hold the guidelines rather than pointing at a copy under `docs/`.
The four bullets above record what phase 1 did, not where those files are today.

### 3. Documentation Tree Realignment

The `docs/` tree aligns strictly with the documentation skill:
```
docs/
    README.md
    CONTRIBUTING.md
    DOCUMENTATION_STYLE_GUIDE.md
    CLAUDE.md
    AGENTS.md
    getting_started/
    architecture/
        MODULES.md
    guides/
    reference/
        API_REFERENCE.md
        CHANGELOG.md
        GLOSSARY.md
        KNOWN_ISSUES.md
    design/
        README.md
        REMAINING_WORK.md
        SUSHIDSP.md
        GUI_UX_REDESIGN.md
        SUSHI_STANDARDS_MIGRATION.md
    agent/
    archive/
        superpowers/
            plans/
            specs/
```

Key transformations:
1. `docs/archive/superpowers/plans/` and `docs/archive/superpowers/specs/` move to `docs/archive/superpowers/`.
2. The module index moves to `docs/architecture/MODULES.md`. Done.
3. `docs/api/mainpage.md` moves to `docs/reference/API_REFERENCE.md`.
4. `docs/design/sushidsp.md` is renamed to `docs/design/SUSHIDSP.md` with status `**Status:** Open — phase 2 of 4`.
5. `docs/design/gui_ux_redesign.md` is renamed to `docs/design/GUI_UX_REDESIGN.md` with status `**Status:** Open — phase 2 of 2`.
6. `docs/design/README.md` is added to provide a direct topic-to-design document map.
7. `docs/reference/CHANGELOG.md` is formatted to comply with `check_changelog.py` rules:
   - Heading reads `## Unreleased`.
   - Each entry follows `- YYYY-MM-DD — scope: Past-tense verb description (`path`).` with fewer than 240 characters.
8. `docs/README.md` internal hyperlinks are updated to resolve correctly.

## Phase 2: Module Tier Restructuring (Future)

- Reorganise `src/` and `include/SushiDSP/` into `modules/<tier>/<module>/`.
- Move `apps/` to `applications/`.
- Configure `K_TIER_ORDER` in `tools/layering/check_layering.py` and `cmake/`.

## Phase 3: Code Style and Comment Compliance (Future)

- Audit and refactor class and interface names (e.g. remove `I` prefix).
- Ensure Allman braces and 100-character line width across all C++ translation units.
- Populate license and `@file` blocks matching `source-comments`.
