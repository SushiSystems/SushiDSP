# Sushi Systems Programming Standards Migration

**Status:** Open — P1 built, P2 under way, P3 not started

## Problem

SushiDSP predates the SushiSkills rules (first named `sushisystemsprogramming`): its layout,
tooling and code style follow no shared standard. The migration has to keep the build stable and
all 969 baseline unit tests and the host applications passing at every step, so it is a phased
rollout.

## Decision

The repository moves onto the SushiSkills rules in three phases: tooling and documentation layout
first, then the module tier restructuring, then code style and comment compliance. Each phase
lands with its tests green.

## Design

### Canonical verification tooling

The canonical checkers of SushiSkills are vendored into `tools/`:

- `tools/checking/checker.py`: shared checker framework and finding reporter.
- `tools/documentation/check_source_comments.py`: source comment and Doxygen block checker.
- `tools/documentation/check_docs_layout.py`: documentation tree and naming validator.
- `tools/documentation/check_changelog.py`: changelog entry formatting and line validator.
- `tools/layering/check_layering.py`: layer dependency validator.
- `tools/README.md`: guide describing checker execution, options and contract.

The `sd` developer CLI (`cli/sushidsp/cli.py`) has a `check` subcommand:

```
sd check [--report] [--rule NAME]
```

It runs the shared checkers under `tools/` and returns a non-zero exit code if any reports
violations.

### Root directory and agent entry points

The root directory conforms to `repository-layout`: `CLAUDE.md` and `AGENTS.md` at the repository
root hold the guidelines, architecture rules and the Sushi doctrine. Developer-local artifacts at
the root (`*.png`, `*.wav`, `imgui.ini`) are handled by the repository owner and stay ignored in
`.gitignore`.

### Documentation tree

```
docs/
    README.md
    CONTRIBUTING.md
    DOCUMENTATION_STYLE_GUIDE.md
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
        SUSHIDSP/
        GUI_UX_REDESIGN.md
        SUSHI_STANDARDS_MIGRATION.md
    agent/
    archive/
        superpowers/
```

- `docs/archive/superpowers/plans/` and `docs/archive/superpowers/specs/` live under
  `docs/archive/superpowers/`.
- The module index is `docs/architecture/MODULES.md`; the Doxygen main page is
  `docs/reference/API_REFERENCE.md`.
- Design documents are `UPPER_SNAKE_CASE.md` with a status line, and `docs/design/README.md`
  maps each topic to its document.
- `docs/reference/CHANGELOG.md` complies with `check_changelog.py`: the heading reads
  `## Unreleased`, and each entry follows `- YYYY-MM-DD, scope: Past-tense verb description
  (`path`).` with fewer than 240 characters.

### Module tier restructuring

- Reorganise `src/` and `include/SushiDSP/` into `modules/<tier>/<module>/`.
- Configure `K_TIER_ORDER` in `tools/layering/check_layering.py` and `cmake/`. The checker's half
  has the `nodes`, `catalog` and `session` tiers.

### Code style and comment compliance

- Audit and refactor class and interface names (for example remove the `I` prefix).
- Ensure Allman braces and 100-character line width across all C++ translation units.
- Populate license and `@file` blocks matching `source-comments`.

The work folder of phase 1 is archived at
`docs/archive/agent/2026_09_15_SUSHI_STANDARDS_PHASE_1/`.

## Phases

| Id | Phase | State |
| --- | --- | --- |
| P1 | Foundational tooling and documentation layout | built |
| P2 | Module tier restructuring: the checker's half has landed, `src/` and `include/SushiDSP/` have not moved | building |
| P3 | Code style and comment compliance | planned |

## Open questions

None.
