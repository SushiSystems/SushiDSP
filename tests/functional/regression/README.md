# Regression tests

Each file here renders one second of a node and compares it with a pinned file under
`tests/golden/`. This page holds the reasoning the source comments carried; the source keeps
one line per point and cites this file. [`tests/golden/README.md`](../../golden/README.md) holds
the file format and the table of pins.

## Golden-file comparison

One second of the node's output is pinned as `tests/golden/<node>_<rate>.f32` and compared by
RMS residual. The bound is -80 dB: it catches any real behaviour change and absorbs float
reassociation between toolchains. Every file tests two rates, 48 kHz and 96 kHz, with the same
rule.

Regenerate deliberately, never to make a red test green:

```text
SUSHIDSP_REGENERATE_GOLDEN=1 sd test -s regression
```

Say why in the commit message and in `tests/golden/README.md`.

## `_USE_MATH_DEFINES`

Every file here defines `_USE_MATH_DEFINES` ahead of its includes, for the reason
[`tests/functional/unit/README.md`](../unit/README.md) gives under the same heading.

## `test_dd3_regression.cpp`

### `render_dd3`

MODE M at D.TIME 0.4 clocks the ring at 46804.2 Hz, so 4096 samples of it are 87.5 ms and the
whole wet path is 89.1 ms, several repeats inside the stimulus. F.BACK 0.45 sits inside the
loop's stable region while still recirculating: a change to either resampler, to the compander
or to the converter moves the file. Every knob is set before `prepare()` so the smoothers snap
and the file holds the settled pedal rather than its ramp.

## `test_diezel_vh4_regression.cpp`, `test_jcm800_regression.cpp`, `test_mesa_mark2c_regression.cpp`, `test_peavey5150_regression.cpp`

No cabinet is loaded, so each amplifier's file is independent of anything on disk.

## `test_tube_screamer_regression.cpp`

The source carried this line above both `TubeScreamerGoldenFile48k` and
`TubeScreamerGoldenFile96k`: "Expected red until Task 8 re-pins against the DK chain at 4x - see
the D6c plan." `tests/golden/README.md` lists a tube_screamer pin by D6c Task 8 dated
2026-09-07, so the note is out of date for both cases. Neither case was run for this page;
`sd test -s regression` shows whether they are green.
