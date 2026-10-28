# Getting started with SushiDSP

SushiDSP is a C++17 library of amplifier and pedal models. Each nonlinear stage is the
circuit's own equations, solved per sample; none is a trained network. The core depends on the
C++17 standard library and SIMD intrinsics and on nothing else. A graph of `AudioNode`s, the
rig, chains the models in any order, the way a pedalboard does.

The root [`README.md`](../../README.md) says which circuits are modelled and how the models are
held in place.

## First run

`sd` is the repository's CLI, the Python package under `cli/`. [`INSTALL.md`](INSTALL.md) gives
the standalone route and the SushiHub route; `cli/pyproject.toml` names the Python and
`sushicore` versions it needs.

```bash
python -m pip install -e ./cli
sd setup --dry-run   # report what is missing and what would be written
sd setup             # provision it and write the tool paths to cli/config.local.toml
sd doctor            # report whether this machine can build
sd build             # configure if needed, then build every target
sd test              # build if needed, then run the unit suite
sd host              # build if needed, then start the standalone host GUI
```

The build needs CMake 3.20 or newer and a C++17 compiler, on Windows or Linux.
`cli/sushistack.deps.toml` lists what `sd setup` provisions.

Every build action goes through `sd`; `cmake` and `ctest` are not called by hand.
[`COMMAND_LINE_INTERFACE.md`](../guides/COMMAND_LINE_INTERFACE.md) describes each command,
and [`cli/README.md`](../../cli/README.md) the package itself.

## The API reference

```bash
sd docs      # writes build/docs/api-site/html/
```

## Where to go next

[`docs/README.md`](../README.md) is the manual index.
[`docs/CONTRIBUTING.md`](../CONTRIBUTING.md) is the page to read before changing anything.
