# Tests ci

## Remaining work

| ID | Pri | State | Title | Done when | Blocked by | Source | Opened |
| --- | --- | --- | --- | --- | --- | --- | --- |
| RW-81 | P2 | open | Run sd test through the shared test options and run_tests of sushicore | sd test takes the shared options, runs small and medium by default and prints a budget line from a real JUnit file |  |  | 2026-11-01 |

## Known issues

| ID | Origin | Symptom | Workaround | Fix | Source | Opened |
| --- | --- | --- | --- | --- | --- | --- |
| KI-30 | own | The test mirror exception is a local edit of a shared checker | restore this repository's version after a copy | RW-34 |  | 2026-10-19 |
| KI-48 | dependency | sd setup installs no SushiLog or SushiProfiler CLI, so sd build says 'cannot run sl' | Run python -m pip install ~/.sushisystems/sources/*/*/cli after sd setup, as ci.yml does |  | `.github/workflows/ci.yml` | 2026-10-30 |
