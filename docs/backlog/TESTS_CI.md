# Tests ci

## Remaining work

| ID | Pri | State | Title | Done when | Blocked by | Source | Opened |
| --- | --- | --- | --- | --- | --- | --- | --- |

## Known issues

| ID | Origin | Symptom | Workaround | Fix | Source | Opened |
| --- | --- | --- | --- | --- | --- | --- |
| KI-30 | own | The test mirror exception is a local edit of a shared checker | restore this repository's version after a copy | RW-34 |  | 2026-10-19 |
| KI-48 | dependency | sd setup installs no SushiLog or SushiProfiler CLI, so sd build says 'cannot run sl' | Run python -m pip install ~/.sushisystems/sources/*/*/cli after sd setup, as ci.yml does |  | `.github/workflows/ci.yml` | 2026-10-30 |
