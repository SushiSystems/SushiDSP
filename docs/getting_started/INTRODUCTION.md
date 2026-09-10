# Getting started with SushiDSP

SushiDSP is a portable C++17 amp/pedal DSP framework — the shared engineering behind a
standalone rig of white-box amp and pedal models. Every nonlinear stage is a physically derived
circuit model, a Wave Digital Filter or a Newton–Raphson-solved diode/triode equation, never a
trained network, and the DSP core depends on nothing beyond the C++17 standard library and SIMD
intrinsics — no plugin format, game engine, or GPU runtime. A standalone `IAudioNode` graph, the
**rig**, chains any number of pedal/amp/cabinet models in any order, the way a real pedalboard
works.

This is a DSP library you drive from C++, not a finished pedal you plug in this afternoon — see
the root [`README.md`](../../README.md) for what is built today and what is still only designed.

## Building a workspace

Every build action goes through the `sd` command line tool, never `cmake`/`ctest` directly. The
full setup, requirements, and conventions live in
[`docs/CONTRIBUTING.md`](../CONTRIBUTING.md#1-getting-set-up); this page only names the three
commands you will reach for most:

```bash
sd build      # configures and builds the tree
sd test       # runs the test suite
sd doxygen    # generates the API reference under docs/api-site/html/
```

## Where to go next

[`docs/README.md`](../README.md) is the manual's index — the design spec, the changelog, the
per-module references, and everything else this tree carries.
