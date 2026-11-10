# Tests ci

## Remaining work

| ID | Pri | State | Title | Done when | Blocked by | Source | Opened |
| --- | --- | --- | --- | --- | --- | --- | --- |
| RW-82 | P2 | open | Clear the known findings of the shared checkers | tools/known_findings.toml holds no entry and tools/check_all.py is clean without it |  | `tools/check_all.py` | 2026-11-05 |
| RW-84 | P3 | open | Derive the secondary mean bound of the output transformer stage test | test_output_transformer_stage holds the secondary mean to a bound derived from the stage, not K_MEAN_BOUND 1e-3 |  | `tests/foundation/circuits/stages/test_output_transformer_stage.cpp` | 2026-11-10 |
| RW-85 | P3 | open | Derive the amplitude band of the half-speed track player tone test | TrackPlayerAtHalfSpeedKeepsTheToneAmplitude holds a derived bound, not the 0.01 band |  | `tests/nodes/capture/test_track_player.cpp` | 2026-11-10 |
| RW-86 | P3 | open | Derive the image rejection floor of the half-speed vari-rate reader test | VariRateReaderAtHalfSpeedKeepsTheSourceToneImageDown holds a derived floor, not 80 dB |  | `tests/nodes/capture/test_vari_rate_reader.cpp` | 2026-11-10 |
| RW-90 | P2 | open | Run the test shards and confirm every case runs once | A full sd test run executes each case in exactly one shard and test_budget.py reads the shard XML. |  | `cmake/SushiDSPTests.cmake` | 2026-11-10 |

## Known issues

| ID | Origin | Symptom | Workaround | Fix | Source | Opened |
| --- | --- | --- | --- | --- | --- | --- |
| KI-30 | own | The test mirror exception is a local edit of a shared checker | restore this repository's version after a copy | RW-34 |  | 2026-10-19 |
| KI-48 | dependency | sd setup installs no SushiLog or SushiProfiler CLI, so sd build says 'cannot run sl' | Run python -m pip install ~/.sushisystems/sources/*/*/cli after sd setup, as ci.yml does |  | `.github/workflows/ci.yml` | 2026-10-30 |
