# Contributing to SushiDSP

SushiDSP is currently a personal project developed by Mustafa Garip. To maintain a unified
vision while the architecture is still settling, **external pull requests are not being accepted
at this stage** — but bug reports and design discussion are very welcome, and this document is
the reference for how the project is actually built and how a change should look.

By contributing you agree that your contributions are licensed under the project's
**Apache License, Version 2.0** (see [`LICENSE`](../LICENSE)).

> New to the codebase? Read [`docs/design/SUSHIDSP.md`](design/SUSHIDSP.md) first — it explains
> the rig graph, the `NodeDescriptor` product contract, and the math/circuit layering every
> DSP kernel follows.

---

## 1. Getting set up

```bash
curl -fsSL https://sushisystems.io/install.sh | bash    # Linux/macOS
```

```powershell
irm https://sushisystems.io/install.ps1 | iex            # Windows
```

```bash
hub add sushidsp             # or: hub link sushidsp <path-to-an-existing-checkout>
hub install-cli sushidsp     # puts `sd` / `sushidsp` on PATH
sd build
sd test
```

SushiDSP pins no toolchain, so it also builds standalone: clone it anywhere, `sd build`/`sd test`
work unchanged with the platform compiler. Inside a SushiStack workspace `sd` picks up the
intel/llvm clang++ that `hub install` provisions (then a `clang++` on PATH, then the platform
default); `sd config` reports which one it chose and `sd env` the environment it builds in. The
regression snapshots hold across compilers because FMA contraction is pinned off for every
toolchain (`cmake/SushiDSPCompilerFlags.cmake`).

## 2. Before you start coding

- This is a personal project; if you're not the maintainer, the productive path is an issue — a
  bug report or a design discussion — not a PR.
- **Keep changes focused.** One logical change per commit. A refactor and a behavior change
  together are hard to review and hard to revert.
- **Match the layer boundaries.** The `sushi_dsp` core (`include/SushiDSP/core`, `math`) has zero
  dependency beyond the C++17 standard library and SIMD intrinsics — nothing in it may reach for
  SDL2, a product module, or any host/device concern. A product module depends only on
  `sushi_dsp`; it never references a device or a host. Read spec §7/§8 before adding a new
  target or crossing one of these boundaries.
- **A design decision that narrows scope gets written down, not silently dropped.** If you defer
  something (see the WDF adaptor-tree deferral in the D2 plan for the pattern), say so explicitly
  in the plan/spec, with the reason.

## 3. Code conventions

These are enforced by every reviewer and by the implementation plans; a PR that violates them
gets sent back before anything else is reviewed.

- **C++17 only** — no C++20/23 features.
- **Allman braces** — the opening brace on its own line. Trivial one-line accessors may stay
  compact.
- **Naming** — types `PascalCase`; functions and variables `snake_case`; class members carry a
  trailing underscore (`impl_`); constants `kPascalCase` (`kMaximumNodes`); spelled-out names,
  no abbreviations.
- **License header** — the exact 76-column boxed `/* ... */` template pinned in
  [`docs/design/SUSHIDSP.md` §0a](design/SUSHIDSP.md), on every `.hpp`/`.cpp` file. Copy an
  existing file's header verbatim (e.g. `include/SushiDSP/core/PortLayout.hpp`) and only change
  the filename line — do not retype the box by hand, and do not skip it on a test file (every
  `.hpp`/`.cpp` in the tree carries it, tests included).
- **Doxygen** — `@brief`/`@param`/`@return` on every public function.
- **Comments** — none, unless the *why* is non-obvious. Never explain *what* the code does;
  well-named identifiers already do that.
- **Real-time safety** — `IAudioNode::process()`, and anything reachable from it, must be
  allocation-free, lock-free, syscall-free, and throw-free. Size every scratch buffer in the
  constructor. If you add a class that will eventually sit on the audio path, add a
  `*ProcessPerformsNoHeapAllocation` case to `tests/functional/unit/test_rt_safety.cpp` using its
  existing `operator new` trap — don't leave that as an inspection-only claim.

## 4. Tests

- Every test lives under `tests/functional/unit/test_<snake_case_name>.cpp`, compiled into the
  single `sushidsp_tests` binary via `tests/CMakeLists.txt`.
- Every GTest case uses the suite-name prefix convention: `TEST(Unit_Dsp, SomeCase)` for a unit
  test. `Integration_Dsp`/`Regression_Dsp` join the convention once a phase needs those suites —
  see `tests/CMakeLists.txt`'s comment for how `ctest -L <suite>` selects a sub-suite from the
  prefix.
- A **regression test** pins a fixed input against a fixed, previously-verified expected output
  (a snapshot), guarding against an accidental behavior change in a later refactor — distinct
  from a unit test, which exercises one function's contract in isolation. Add one whenever a
  product's end-to-end output (not just a math kernel) first becomes stable enough to snapshot.
- **Golden files** (`tests/golden/`) are the regression snapshot format: a 1-second render at
  48 kHz and at 96 kHz, stored as a raw `.f32` file, compared against a fresh render at −80 dB
  RMS residual — loose enough to survive a toolchain/compiler change, tight enough to catch a
  real behavior change. A deliberate behavior change regenerates them:
  `SUSHIDSP_REGENERATE_GOLDEN=1 sd test -s regression`, then commit the updated `.f32` files and
  add a row to `tests/golden/README.md` explaining what changed and why — never regenerate to
  silence a failure without knowing the reason for it.
- Golden-vector tests (an independently-derived expected value, not the implementation's own
  output) are preferred over tautological "does it run" tests for anything numerical — see any
  D0–D2 math kernel test for the pattern.
- The alias-floor acceptance cases (`tests/functional/unit/test_measure.cpp`) carry the
  `Regression_Dsp` prefix rather than `Unit_Dsp`: each renders several seconds of audio, and
  keeping them under the regression label leaves `sd test -s unit` a fast edit loop.
- Run `sd test` before every commit. `sd test --suite all` once more than `unit` exists.
- Anything touching the audio path gets measured: `sd test -t release -s benchmark` (the
  `tests/benchmark/` timing harness, registered as a single ctest case under the
  `benchmark` label) before and after, and the numbers go in the commit message or the
  plan. A change that is meant to be pure cost must keep the regression snapshots
  bit-identical; one that moves them is a behaviour change and is reviewed as one.

## 5. SOLID

- **SRP** — every `IAudioNode` does one DSP job; math kernels, the device seam, and each product
  are separate.
- **OCP** — new nodes/products/decoders/devices plug in behind existing interfaces; the rig graph
  never names a concrete node type.
- **LSP** — every node honors "fill exactly `frames`, allocation-free"; any node is substitutable
  in any rig slot.
- **ISP** — narrow interfaces (`IAudioNode`, `IAudioDevice`) — no god-interface.
- **DIP** — the core depends on abstractions; the platform device is injected, never assumed.

See [`docs/design/SUSHIDSP.md` §10](design/SUSHIDSP.md) for the full statement.

## 6. Commit messages

Explain *why*, not *what* — the diff already says what changed. Reference the roadmap phase
(`D0`, `D1`, …) when the change belongs to one.
