# golden_compare

Internal module: the stimulus and the residual comparison that the golden-file regression tests
use. Its headers are never installed, and no product links it.

The reference `.f32` files and their record stay in `tests/golden/` until the regression tests move
to their owners; see `docs/agent/2026_10_14_TEST_TREE/PLAN.md`.

## golden_file.hpp

`stimulus(sample_rate)` is the one-second input every golden file was rendered from.
`compare_or_regenerate(directory, name, rendered)` compares `rendered` with `<directory>/<name>.f32`
and returns the RMS residual in dB. The caller passes the folder that holds the file. Setting
`SUSHIDSP_REGENERATE_GOLDEN` rewrites the file instead.
