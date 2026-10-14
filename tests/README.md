# tests

The test programs of SushiDSP and the notes they share. A unit's tests sit at the path that
mirrors the unit's header; the notes of one owner sit in that owner's README.

# Unit tests

The reasoning behind the cases in this folder: why a tolerance has its value, where a golden
number comes from, what a case was written to catch. The source keeps one line per point and
cites this file.

There is one section per test file and one subsection per test case or helper. Each entry
opens with the comment line the source carries, so a search for that line lands on its
reasoning. Figures are quoted as they were measured, with the date where the source gave one.

# Regression tests

Each file here renders one second of a node and compares it with a pinned file under
the golden folder of its unit. This page holds the reasoning the source comments carried; the source keeps
one line per point and cites this file. [`modules/foundation/golden_compare/README.md`](../../modules/foundation/golden_compare/README.md) holds
the file format and the table of pins.

## `_USE_MATH_DEFINES`

The test files that use `M_PI` define `_USE_MATH_DEFINES` ahead
of their includes. MSVC's `<cmath>` declares `M_PI` only when the macro is already defined, and
`<cmath>` may be pulled in transitively by any earlier include, so the definition has to stay
the first thing in the translation unit. GCC and Clang, through libstdc++ and libc++, declare
`M_PI` unconditionally.

## Golden-file comparison

One second of the node's output is pinned as `goldens/<unit>/<node>_<rate>.f32` beside the unit's tests and compared by
RMS residual. The bound is -80 dB: it catches any real behaviour change and absorbs float
reassociation between toolchains. Every file tests two rates, 48 kHz and 96 kHz, with the same
rule.

Regenerate deliberately, never to make a red test green:

```text
SUSHIDSP_REGENERATE_GOLDEN=1 sd test -s regression
```

Say why in the commit message and in `modules/foundation/golden_compare/README.md`.

## Regression output of four products

No cabinet is loaded, so each amplifier's file is independent of anything on disk.

## main.cpp
`main` initialises GoogleTest and runs every registered case. SushiDSP is a portable DSP
library with no dependencies, so the tests exercise the real kernels and the graph directly on
the host. There are no mocks, and there is no device setup to perform here.
