# Backlog migration: inventory

**Status:** Shipped — 2026-10-19. The owner approved the inventory as proposed.

The migration of this repository to the backlog format of SushiSkills'
`skills/documentation/BACKLOG_FORMAT.md`, step 2 of section 8 of SushiSkills'
`docs/agent/2026_10_19_BACKLOG_FORMAT/SPEC.md`.

## Shared tools

`copy_shared_tools.py` brought the shared files up to SushiSkills, except
`tools/testing/check_test_mirror.py`: this repository holds a local edit of it (`KI-30`), so the
file was put back as it was after the copy. `sushiskills RW-20` asks SushiSkills to carry the
exception; `RW-34` here takes it when that lands.

## Fact check

Both files were last written at `01fd75b` (2026-10-17); the commits since change no code the
entries name. What changed:

| Entry | Verdict |
| --- | --- |
| Run the workflows once on GitHub | Closed for CI and Nightly, which run; the release workflow has not run (`RW-64`). CI fails on the small tests under ThreadSanitizer since 2026-10-18 (`KI-31`, new) |
| Write the READMEs of `applications/host/`, `applications/host_asio/`, `signal_measure`, `tests/foundation/core/` | Closed: each exists; `include/SushiDSP/` is gone |
| CI calls `cmake` and `ctest` directly, runs no checker, has no CLI test job | Closed: CI runs `tools/check_all.py` and the CLI tests |
| One workflow file; no release workflow | Closed: `ci.yml`, `nightly.yml`, `release.yml` |
| `*.f32` not marked binary | Closed: `.gitattributes` marks it; the CSVs in the manual remain (`KI-39`) |
| Eight remote-tracking refs under `refs/remotes/local/` | Closed: none left |
| Comments citing module paths from before the tiers | Closed: the paths they cite are this tree's |
| One header lacks `#pragma once` | Closed |
| No function is `[[nodiscard]]` | Stale: seven headers use it; the error models remain (`KI-43`) |
| Em dashes as punctuation in three pages | Closed: one dash left in each of two design pages |
| The v0.1.0 report records the schematic, not the release | Not a defect: a work folder records its time |
| Tests not grouped as unit, integration, regression | Closed by the test tree work of 2026-10-15 and the rules that followed; the 24 host sources in the test binary remain (`KI-32`) |
| Loop FX sends and the VIRTM view | Not work: the specification's section 3 records why they stay out |

## Shape

- Each of the 29 prose known issues became a field entry. Bodies were cut to the figures a reader
  needs to recognise the issue; the alias floor keeps its measurements for all three closing
  options, since the owner chooses between them (`RW-4`).
- Three known issues record behaviour that is the design's (`KI-10`, `KI-11`, `KI-23`) or a
  rule callers follow (`KI-24`); each says so in `Fix: none, <reason>`. "DS0, DS2 and DS3 are
  committed under each other's names" left: `git log` records it and nothing bites.
- The audit table's rows became entries, or closed as above.
- The order sections ("Opened 2026-10-13 ...") became areas.

The backlog holds `RW-1` to `RW-77` less `RW-24`, `RW-39` and `RW-61`, ten of them decisions;
the known issues hold `KI-1` to `KI-45`.

## Changes in sibling repositories

| Repository | Change |
| --- | --- |
| sushiskills | `RW-20`: let the test mirror checker skip units listed in `tools/settings.py` |

## Status lines

Ten work folder files took the four forms. Open: `V0_1_0_RELEASE`,
`SUSHI_STANDARDS_MIGRATION`, `AMPLITUBE5_CHANNELS`, `CABINET_DRIVERS_AND_FURNITURE`,
`TRACK_RIGS_AND_PLUGIN`. The rest shipped. `docs/.docs_truth_allow` lost three entries the
rewritten known issues no longer need and gained the probe folders `tools/README.md` names.

## Outcome

The owner approved the inventory on 2026-10-19.

```
$ python tools/check_all.py .
clean: 10 checkers, tests
```
