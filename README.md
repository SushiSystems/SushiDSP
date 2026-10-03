# SushiDSP

SushiDSP models guitar amplifiers and effects by solving their circuits, sample by sample, in real
time. A model here is not a filter shaped until it sounds close: it is the schematic's own nodal
equations, with the triode and pentode laws in the loop, solved at 4x or 8x the host rate.

Six circuits are modelled. The Marshall JCM800 2203, the Mesa/Boogie Mark IIC+ in lead mode, the
Peavey 5150 and the Diezel VH4 on the amplifier side; the Ibanez Tube Screamer and the Boss DD-3
on the pedal side. Cabinet convolution, room acoustics and the routing nodes that wire a rig
together sit beside them.

## What "white box" commits us to

Every constant in a model comes from somewhere, and each module's README says where. A figure read
off a schematic is marked as read; a figure derived from it shows the arithmetic; a figure nobody
has published is listed in that module's stand-in table, with what would settle it.

`modules/nodes/mesa_mark2c/README.md` is the longest of these, because the Mark IIC+ is the most
carefully traced model in the repository. It records, among other things, that the output
transformer's primary impedance appears on no sheet and in no published source, that five of the
model's constants therefore stand in for it, and that one of them currently decides whether the
global feedback loop is stable. None of that is hidden in a comment. A model whose gaps are
written down can be corrected by the next person who measures a real chassis.

## Build and run

Everything goes through `sd`, the repository's own CLI. It sets the runtime search path, so a
binary started any other way will not find its libraries. `sd` is the Python package in `cli/`
and needs sushicore 0.6.0, which is not on PyPI yet; install sushicore from its own checkout
first, then `python -m pip install -e ./cli`.

```bash
sd build      # configure if needed, then build every target
sd test       # build if needed, then run the suite
sd check      # documentation layout, changelog shape and layer boundaries
sd run        # the standalone host GUI
sd doxygen    # the API reference, from the headers
```

C++17 and CMake 3.20 or newer. Windows and Linux.

## Dependencies

`cli/sushistack.deps.toml` is what SushiDSP needs beyond a compiler: SDL2 for the host's audio
device, and, optionally, the intel/llvm clang++ it prefers when one is around. GoogleTest comes
through CMake FetchContent, so nothing has to be installed for the suite.

```bash
sd setup --dry-run  # report what is missing and what would be written
sd setup            # provision it; --yes skips the LLVM-download prompt
sd doctor           # report whether this machine can build; --for GROUP limits it to one group
sd link             # record this checkout in a workspace's module registry
sd unlink           # remove it from that registry
```

`sd setup` provisions into the shared root, `~/.sushisystems` (or `SUSHISYSTEMS_HOME`), whether
or not the checkout sits in a SushiStack workspace. It then writes the tool paths it found to
`config.local.toml` and prints the doctor report. SushiStack is an
accelerator here, not a requirement: SushiDSP builds and tests without it.

`sushi-module.toml` is what makes a checkout kept inside a workspace recognisable to `hub`
without a catalog entry.

## How the models are held in place

1007 tests run under `sd test`. Twelve of them are golden files: one second of mono float32 per
node per sample rate, compared against a pinned render, failing when the RMS residual rises above
-80 dB. `tests/golden/README.md` records every time one was regenerated and what changed to
justify it, so a model's history reads as a list of deliberate decisions rather than a drifting
file.

The real-time path carries a harder rule: `IAudioNode::process()`, and everything it calls,
allocates nothing, locks nothing, makes no system call and throws nothing. Thirty-nine tests in
`tests/functional/unit/test_rt_safety.cpp` enforce the first of those four by replacing
`operator new` and failing the test if the audio path reaches it. The other three are held by
review.

## Where to go next

The manual index is [docs/README.md](docs/README.md); every document in the tree is reachable from
it. [docs/design/SUSHIDSP.md](docs/design/SUSHIDSP.md) is the design spec and records intent, so it
describes work that does not exist yet. [docs/design/REMAINING_WORK.md](docs/design/REMAINING_WORK.md)
is the single backlog of what is actually open. [docs/CONTRIBUTING.md](docs/CONTRIBUTING.md) is the
first thing to read before changing anything.

## License

Apache License, Version 2.0. See [LICENSE](LICENSE).

[NOTICE](NOTICE) carries the attribution the licence asks for, the third-party libraries a build
links, and the trademark position: the manufacturer and product names in this repository say which
circuit a model was written against, and none of those companies is affiliated with SushiDSP.
