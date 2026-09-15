# Sushi Systems Programming Standards Phase 1 Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Vendor canonical checkers into `tools/`, add `sd check` to CLI, standardize root pointer files, and align the `docs/` hierarchy with Sushi Systems Programming standards.

**Architecture:** Copy reference checkers from `sushisystemsprogramming/tools` into `tools/`. Wire `sd check` in the Typer CLI to invoke layout, changelog, and layering checkers. Update root `CLAUDE.md`/`AGENTS.md` to point to `docs/CLAUDE.md`. Reorganize non-compliant `docs/` subdirectories into standard locations (`docs/archive/`, `docs/architecture/`, `docs/reference/`, `docs/design/`), update filenames to `UPPER_SNAKE_CASE.md`, and format `docs/reference/CHANGELOG.md` until checkers pass.

**Tech Stack:** Python 3.11, Typer, Markdown, Git.

**Spec:** `docs/design/SUSHI_STANDARDS_MIGRATION.md`

## Global Constraints

- Never invoke cmake or ctest directly; always use `sd`.
- Root directory files must be strictly minimal; no `Doxyfile`, no unmanaged loose files.
- `docs/` directory entries must strictly match `K_DOCS_ENTRIES`: `README.md`, `CONTRIBUTING.md`, `DOCUMENTATION_STYLE_GUIDE.md`, `CLAUDE.md`, `AGENTS.md`, `getting_started`, `architecture`, `guides`, `reference`, `design`, `agent`, `archive`.
- Every design document in `docs/design/` must be named `UPPER_SNAKE_CASE.md` and start with a title followed by `**Status:** ...`.
- In `docs/reference/CHANGELOG.md`, entries must match `- YYYY-MM-DD — scope: Past-tense verb description (`path`).` with <= 240 chars and single sentences.

---

### Task 1: Vendor Canonical Verification Tooling

**Files:**
- Create: `tools/common/checker.py`
- Create: `tools/documentation/check_source_comments.py`
- Create: `tools/documentation/check_docs_layout.py`
- Create: `tools/documentation/check_changelog.py`
- Create: `tools/layering/check_layering.py`
- Create: `tools/README.md`

**Interfaces:**
- Consumes: Reference files from `D:/Projects/sushisystemsprogramming/tools/`
- Produces: `python tools/documentation/check_docs_layout.py [repo] [--report]`
- Produces: `python tools/documentation/check_changelog.py [repo] [--report]`
- Produces: `python tools/layering/check_layering.py [repo] [--report]`
- Produces: `python tools/documentation/check_source_comments.py [paths] [--report]`

- [ ] **Step 1: Copy checker framework and scripts**

Copy all files from `D:/Projects/sushisystemsprogramming/tools/` to `D:/Projects/sushidsp/tools/`:
- `D:/Projects/sushisystemsprogramming/tools/common/checker.py` -> `tools/common/checker.py`
- `D:/Projects/sushisystemsprogramming/tools/documentation/check_source_comments.py` -> `tools/documentation/check_source_comments.py`
- `D:/Projects/sushisystemsprogramming/tools/documentation/check_docs_layout.py` -> `tools/documentation/check_docs_layout.py`
- `D:/Projects/sushisystemsprogramming/tools/documentation/check_changelog.py` -> `tools/documentation/check_changelog.py`
- `D:/Projects/sushisystemsprogramming/tools/layering/check_layering.py` -> `tools/layering/check_layering.py`
- `D:/Projects/sushisystemsprogramming/tools/README.md` -> `tools/README.md`

- [ ] **Step 2: Verify checkers execute with --report**

Run: `python tools/documentation/check_docs_layout.py --report`
Expected: Exit 0 with issue counts per rule.

- [ ] **Step 3: Commit canonical tools**

```bash
git add tools/common tools/documentation tools/layering tools/README.md
git commit -m "tools: vendor canonical checkers from sushisystemsprogramming"
```

---

### Task 2: Add `sd check` to Developer CLI

**Files:**
- Modify: `cli/sushidsp/cli.py`
- Modify: `cli/sushidsp/services/project.py`

**Interfaces:**
- Consumes: `tools/documentation/check_docs_layout.py`, `tools/documentation/check_changelog.py`, `tools/layering/check_layering.py`
- Produces: CLI subcommand `sd check [--report] [--rule NAME]`

- [ ] **Step 1: Add check service runner in `cli/sushidsp/services/project.py`**

Add function `check(report: bool = False, rule: Optional[str] = None) -> int`:
It resolves the repository root path, runs `tools/documentation/check_docs_layout.py`, `tools/documentation/check_changelog.py`, and `tools/layering/check_layering.py` sequentially using `sys.executable`, and returns the highest exit code.

- [ ] **Step 2: Add `check` command to `cli/sushidsp/cli.py`**

```python
@app.command("check")
def check(
    report: bool = typer.Option(False, "--report", "-r", help="Report issue counts without failing."),
    rule: Optional[str] = typer.Option(None, "--rule", help="Run only the specified rule."),
):
    """Runs repository layout, changelog, and layering checkers."""
    raise typer.Exit(project_svc.check(report=report, rule=rule))
```

- [ ] **Step 3: Test `sd check` execution**

Run: `sd check --report`
Expected: Outputs checker reports and exits with 0.

- [ ] **Step 4: Commit CLI check command**

```bash
git add cli/sushidsp/cli.py cli/sushidsp/services/project.py
git commit -m "cli: add sd check command for repository checkers"
```

---

### Task 3: Standardize Root Directory and Agent Pointers

**Files:**
- Create: `docs/CLAUDE.md`
- Create: `docs/AGENTS.md`
- Modify: `CLAUDE.md`
- Create: `AGENTS.md`

**Interfaces:**
- Produces: Single-line pointers in `CLAUDE.md`, `AGENTS.md`, and `docs/AGENTS.md` pointing to `docs/CLAUDE.md`.

- [ ] **Step 1: Move existing CLAUDE.md content to docs/CLAUDE.md**

Write `docs/CLAUDE.md` retaining existing developer guidance and referencing Sushi Systems Programming doctrine (`sushisystemsprogramming`).

- [ ] **Step 2: Create docs/AGENTS.md**

Content: `See docs/CLAUDE.md.`

- [ ] **Step 3: Replace root CLAUDE.md and AGENTS.md**

Both `CLAUDE.md` and `AGENTS.md` at the repository root must contain exactly:
```markdown
See docs/CLAUDE.md.
```

- [ ] **Step 4: Verify root files**

Check line count of `CLAUDE.md` and `AGENTS.md`: exactly 1 line each.

- [ ] **Step 5: Commit root pointer changes**

```bash
git add CLAUDE.md AGENTS.md docs/CLAUDE.md docs/AGENTS.md
git commit -m "docs: establish docs/CLAUDE.md and root agent pointers"
```

---

### Task 4: Reorganize Documentation Tree

**Files:**
- Create: `docs/archive/superpowers/plans/`
- Create: `docs/archive/superpowers/specs/`
- Create: `docs/architecture/MODULES.md`
- Create: `docs/reference/API_REFERENCE.md`
- Rename: `docs/design/sushidsp.md` -> `docs/design/SUSHIDSP.md`
- Rename: `docs/design/gui_ux_redesign.md` -> `docs/design/GUI_UX_REDESIGN.md`
- Create: `docs/design/README.md`
- Delete: `docs/modules/`
- Delete: `docs/api/`
- Delete: `docs/superpowers/`

**Interfaces:**
- Consumes: `tools/documentation/check_docs_layout.py`
- Produces: `docs/` hierarchy matching `K_DOCS_ENTRIES`

- [ ] **Step 1: Move superpowers plans and specs to docs/archive/superpowers/**

Move all files from `docs/superpowers/plans/` and `docs/superpowers/specs/` into `docs/archive/superpowers/plans/` and `docs/archive/superpowers/specs/`. Remove `docs/superpowers/`.

- [ ] **Step 2: Relocate docs/modules/README.md and docs/api/mainpage.md**

- Move `docs/modules/README.md` to `docs/architecture/MODULES.md`. Remove `docs/modules/`.
- Move `docs/api/mainpage.md` to `docs/reference/API_REFERENCE.md`. Remove `docs/api/`.

- [ ] **Step 3: Rename and add status lines to design docs**

- Rename `docs/design/sushidsp.md` to `docs/design/SUSHIDSP.md`. Ensure line 2 is: `**Status:** Open — phase 2 of 4`.
- Rename `docs/design/gui_ux_redesign.md` to `docs/design/GUI_UX_REDESIGN.md`. Ensure line 2 is: `**Status:** Open — phase 2 of 2`.

- [ ] **Step 4: Create docs/design/README.md**

Create `docs/design/README.md` mapping topics:
```markdown
# Design Documents

Topic to design document map for live intent.

| Topic | Document | Status |
| --- | --- | --- |
| Core DSP & Architecture | [SUSHIDSP.md](SUSHIDSP.md) | Open — phase 2 of 4 |
| Host GUI/UX Redesign | [GUI_UX_REDESIGN.md](GUI_UX_REDESIGN.md) | Open — phase 2 of 2 |
| Sushi Standards Migration | [SUSHI_STANDARDS_MIGRATION.md](SUSHI_STANDARDS_MIGRATION.md) | Open — phase 1 of 3 |
| Remaining Work Backlog | [REMAINING_WORK.md](REMAINING_WORK.md) | Single live backlog |
```

- [ ] **Step 5: Verify docs entries using check_docs_layout.py**

Run: `python tools/documentation/check_docs_layout.py --rule rule_docs_entries`
Expected: 0 issues for `rule_docs_entries`.

- [ ] **Step 6: Commit docs tree reorganization**

```bash
git add docs/
git commit -m "docs: reorganize tree to match documentation standard"
```

---

### Task 5: Standardize `docs/reference/CHANGELOG.md`

**Files:**
- Modify: `docs/reference/CHANGELOG.md`

**Interfaces:**
- Consumes: `tools/documentation/check_changelog.py`
- Produces: Compliant CHANGELOG with zero findings.

- [ ] **Step 1: Check existing changelog violations**

Run: `python tools/documentation/check_changelog.py .`
Identify specific shape, section, length, and sentence issues.

- [ ] **Step 2: Reformat sections and bullet points**

Ensure:
- Headings: `## Unreleased`, followed by latest release `## v0.3.0 — 2026-09-01`.
- Older releases (v0.2.0, etc.) are moved to `docs/archive/changelog/` if they exceed `K_LIVE_RELEASE_CEILING` (1 live release).
- Every entry starts with `- YYYY-MM-DD — scope: Past-tense-verb description (`file.ext`).`.
- No bullet exceeds 240 characters.
- Each bullet is exactly one sentence.
- No nested bullets.

- [ ] **Step 3: Verify check_changelog.py passes**

Run: `python tools/documentation/check_changelog.py .`
Expected: Exit code 0, 0 issues.

- [ ] **Step 4: Commit changelog changes**

```bash
git add docs/reference/CHANGELOG.md docs/archive/
git commit -m "docs: format CHANGELOG.md to match documentation skill"
```

---

### Task 6: Resolve Internal Links and End-to-End Verification

**Files:**
- Modify: `docs/README.md`
- Modify: other live docs with links pointing to old paths

**Interfaces:**
- Consumes: `tools/documentation/check_docs_layout.py`, `tools/documentation/check_changelog.py`
- Produces: `sd check` clean pass with exit 0.

- [ ] **Step 1: Run check_docs_layout.py to find broken links**

Run: `python tools/documentation/check_docs_layout.py --rule rule_links`
Inspect broken links pointing to old paths (`superpowers/`, `modules/README.md`, `design/sushidsp.md`, etc.).

- [ ] **Step 2: Update broken links in docs/README.md and other live docs**

Update links:
- `superpowers/specs/...` -> `archive/superpowers/specs/...`
- `modules/README.md` -> `architecture/MODULES.md`
- `design/sushidsp.md` -> `design/SUSHIDSP.md`
- `design/gui_ux_redesign.md` -> `design/GUI_UX_REDESIGN.md`
- `api/mainpage.md` -> `reference/API_REFERENCE.md`

- [ ] **Step 3: Run full check_docs_layout.py**

Run: `python tools/documentation/check_docs_layout.py .`
Expected: Clean pass, exit 0.

- [ ] **Step 4: Run full `sd check`**

Run: `sd check`
Expected: Exit 0, all layout and changelog rules passing.

- [ ] **Step 5: Run `sd test` to verify build and test safety**

Run: `sd test`
Expected: All existing tests pass.

- [ ] **Step 6: Commit link fixes and end-to-end verification**

```bash
git add docs/
git commit -m "docs: update relative links and verify with sd check"
```
