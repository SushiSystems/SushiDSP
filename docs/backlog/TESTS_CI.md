# Tests ci

## Remaining work

| ID | Pri | State | Title | Done when | Blocked by | Source | Opened |
| --- | --- | --- | --- | --- | --- | --- | --- |
| RW-82 | P2 | open | Clear the known findings of the shared checkers | tools/known_findings.toml holds no entry and tools/check_all.py is clean without it |  | `tools/check_all.py` | 2026-11-05 |
| RW-84 | P3 | open | Derive the secondary mean bound of the output transformer stage test | test_output_transformer_stage holds the secondary mean to a bound derived from the stage, not K_MEAN_BOUND 1e-3 |  | `docs/agent/2026_11_05_TEST_REVIEW/REPORT.md` | 2026-11-10 |

## Known issues

| ID | Origin | Symptom | Workaround | Fix | Source | Opened |
| --- | --- | --- | --- | --- | --- | --- |
| KI-30 | own | The test mirror exception is a local edit of a shared checker | restore this repository's version after a copy | RW-34 |  | 2026-10-19 |
| KI-48 | dependency | sd setup installs no SushiLog or SushiProfiler CLI, so sd build says 'cannot run sl' | Run python -m pip install ~/.sushisystems/sources/*/*/cli after sd setup, as ci.yml does |  | `.github/workflows/ci.yml` | 2026-10-30 |
