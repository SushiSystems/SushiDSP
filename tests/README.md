# tests

The test programs of SushiDSP and the notes they share. A unit's tests sit at the path that
mirrors the unit's header; the notes of one owner sit in that owner's README.

`tests/support/` holds the headers every test may include as `"support/<name>.hpp"`.
`tolerance.hpp` is copied from SushiSkills and derives a floating-point tolerance from a rounding
bound, so no comparison types one; `test_tolerance.cpp` beside it checks that the copy keeps its
promises under this repository's compiler flags. `clip_stage_reference.hpp` is a host solve of the
Tube Screamer clip stage's equations, written without the DK solver, that the clip stage's
converged-model null test compares against. With s the source, u the inverting node, h the
voltage across the input capacitor, g the one across the ground leg capacitor and v the one
across the feedback network, and the nullator giving u = s - h, the solve integrates
`dh/dt = (s - h) / (Rin * Cin)`, `dg/dt = (u - g) / (Rg * Cg)` and
`Cf * dv/dt = (u - g) / Rg - v / Rf - 2 * Is * sinh(v / (n * Vt))`, and returns u + v.

# Shards

`cmake/SushiDSPTests.cmake` registers the cases of a program in shards, because a case run in a
process of its own pays the process and runtime start-up, which is most of a small case. Per
program and per size there are N CTest tests named `<program>.<size>_shard_<i>of<N>`, each running
the program with `--gtest_filter=<prefix>*:*/<prefix>*` (without the quarantined cases),
`GTEST_TOTAL_SHARDS=N` and `GTEST_SHARD_INDEX=i`. GoogleTest splits the filtered cases between the
shards, so every case runs in exactly one. N is the case count of the size divided by the cases one
shard holds, rounded up: `SUSHIDSP_SHARD_CASES_SMALL` (300), `_MEDIUM` (25) and `_LARGE` (1). The
count comes from the `TEST`, `TEST_F`, `TEST_P`, `TYPED_TEST` and `TYPED_TEST_P` declarations and
the `INSTANTIATE_*` lines of the program's sources, read when CMake configures, so N is a floor
and a new case needs a configure. A shard's timeout is the size's timeout per case times the cases
it holds. The quarantined cases of a size run apart in `<program>.<size>_quarantined`.

Each shard writes its cases to `test_results/gtest/<shard>.xml` in the build tree, which
`tools/testing/test_budget.py --cases <folder>` reads. A suite name belongs to one fixture in a
program: `TEST` and `TEST_P` cannot share a suite name. The name a parameter generator returns
holds no `-`, which a GoogleTest filter reads as the start of its exclusions;
`tools/testing/check_test_odr.py` checks both.

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
SUSHIDSP_REGENERATE_GOLDEN=1 sd test --label medium
```

That run rewrites the files and fails every golden test, since a regeneration compares nothing;
the next run without the variable is the one that passes.

Say why in the commit message and in `modules/foundation/golden_compare/README.md`.

## Regression output of four products

No cabinet is loaded, so each amplifier's file is independent of anything on disk.

## main.cpp
`main` initialises GoogleTest and runs every registered case. SushiDSP is a portable DSP
library with no dependencies, so the tests exercise the real kernels and the graph directly on
the host. There are no mocks, and there is no device setup to perform here.
