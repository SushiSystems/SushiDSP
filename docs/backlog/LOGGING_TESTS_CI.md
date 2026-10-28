# Logging tests ci

## Remaining work

| ID | Pri | State | Title | Done when | Blocked by | Source | Opened |
| --- | --- | --- | --- | --- | --- | --- | --- |
| RW-59 | P2 | open | Finish the logger's tests and lines | tests provoke the writer-thread start, the append failure in `TrackRecorder` and the `TopologyApplier` and `SlotGraph` sites; `list_directory` fails on… |  |  | 2026-10-19 |
| RW-60 | P2 | unmeasured | State the test count with its sizes in the README | one `sd test` run gives the count and `README.md` states it |  |  | 2026-10-19 |
| RW-64 | P2 | unmeasured | Run the release workflow once | `.github/workflows/release.yml` has run on GitHub; CI and Nightly have |  |  | 2026-10-19 |
| RW-65 | P2 | open | Make the small tests pass under ThreadSanitizer | the CI job "Small tests under thread (Linux, Clang)" passes |  |  | 2026-10-19 |
| RW-66 | P2 | open | Compile the forward-sink example | the example in `modules/foundation/log/README.md` comes from a file this tree compiles, or is cut to what it compiles; `SushiRuntime::log_forward_sink()`… |  |  | 2026-10-19 |
| RW-77 | P2 | open | Test the scripts under `tools/dsp/` and `tools/spice/` | each script has tests |  |  | 2026-10-19 |

## Known issues

| ID | Origin | Symptom | Workaround | Fix | Source | Opened |
| --- | --- | --- | --- | --- | --- | --- |
