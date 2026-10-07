# Frequently asked questions

Short answers to what a developer meeting SushiDSP for the first time asks: what it is, what it
needs, how to build and run it, what it leaves undone, how it is licensed and where to report a
problem. Each answer links to the manual page that holds the detail.

## What is SushiDSP?

SushiDSP is a C++17 library of guitar amplifier and pedal models. Each model solves its circuit
sample by sample in real time, at 4x or 8x the host rate: the schematic's own nodal equations,
with the triode and pentode laws in the loop. No stage is a trained network, and no stage is
fitted to a recording. The repository calls this a white box model.

A graph of `IAudioNode`s, the rig, chains the models in any order, the way a pedalboard does.
The core depends on the C++17 standard library and SIMD intrinsics and on nothing else.
[Getting started](../getting_started/INTRODUCTION.md) introduces the library, and the
[glossary](../reference/GLOSSARY.md) fixes the words the manual uses.

## Which amplifiers and effects does SushiDSP model?

Six circuits. The amplifiers are the Marshall JCM800 2203, the Mesa/Boogie Mark IIC+ in lead
mode, the Peavey 5150 (lead channel) and the Diezel VH4 (channel 3). The pedals are the Ibanez
Tube Screamer and the Boss DD-3. Cabinet impulse-response convolution, a room model and the
splitter, mixer and pan nodes that wire a rig together sit beside them.

[Modules](../architecture/MODULES.md) lists each module with the id it registers under, such as
`sushidsp.jcm800`, and links to its README. That README says where each constant comes from and
lists, in a stand-in table, every figure nobody has published. The manufacturer and product
names say which circuit a model was written against; none of those companies is affiliated with
SushiDSP, and [`NOTICE.md`](../../NOTICE.md) holds the trademark position.

## What do I need before I can build SushiDSP?

- Windows or Linux.
- A C++17 compiler and CMake 3.20 or newer.
- Python 3.10, for `sd`, the repository's CLI.
- sushicore 0.7.0 or newer. It is not on PyPI, so it is installed from its own checkout.

`cli/sushistack.deps.toml` lists what `sd setup` provisions beyond those. GoogleTest comes
through CMake FetchContent, so nothing has to be installed for the test suite.
[Getting started](../getting_started/INTRODUCTION.md) has the first-run sequence.

## How do I install the `sd` command and build SushiDSP?

Run these from the repository root:

```bash
python -m pip install -e <sushicore checkout>
python -m pip install -e ./cli
sd setup --dry-run   # report what is missing and what would be written
sd setup             # provision it and write the tool paths to cli/config.local.toml
sd doctor            # report whether this machine can build
sd build             # configure if needed, then build every target
sd test              # build if needed, then run the unit suite
```

Every build action goes through `sd`; `cmake` and `ctest` are not called by hand. `sd` also sets
the runtime search path, so a binary started any other way will not find its libraries.
[The `sd` command line](COMMAND_LINE_INTERFACE.md) describes every subcommand and its flags.

## How do I run the standalone host?

`sd host` builds the tree if it is out of date, then starts `sushidsp_host_gui`. That
application opens an audio device, builds a rig out of the registered products, draws their
front panels, and records and plays back takes against a timeline.

`sd host --type release` chooses the build type, `--no-run` builds and stops there, and
everything after `--` reaches the host unchanged. The command exits 1 when the tree was
configured with `SUSHIDSP_BUILD_GUI=OFF`. No cabinet impulse response ships in the checkout, so
a fresh amplifier slot starts without one. The
[host's README](../../apps/host_gui/README.md) says what the host looks for at startup.

## Which platforms and compilers does SushiDSP support?

Windows and Linux, with a C++17 compiler and CMake 3.20 or newer. SushiDSP pins no toolchain.
`sd` searches for a compiler in this order:

1. An explicit `cxx`, set in a `config.local.toml` or as the `SD_CXX` environment variable.
2. The intel/llvm `clang++` that `sd setup --toolchain intel-llvm` provisions.
3. A `clang++` on `PATH`.
4. CMake's platform default: MSVC through the Visual Studio generator on Windows, the system
   compiler elsewhere.

`sd config` reports which step won and which generator was resolved. The golden-file
comparisons hold across compilers because `cmake/SushiDSPCompilerFlags.cmake` pins FMA
contraction off for every toolchain. The detail is under "Compiler resolution order" in
[the `sd` command line](COMMAND_LINE_INTERFACE.md).

## How do I use the models from my own host application?

The three hosts under `apps/` show the pattern: each owns an `IAudioDevice`, builds a `RigGraph`
from `node_registry()`, and drives `process()` once per audio callback. Every product exposes one
`NodeDescriptor` (an id, a name, parameters, ports and a `make()` function), so a host
instantiates a node through `make_node()` without naming its concrete class.

The real-time path carries one rule: `IAudioNode::process()` and everything it calls allocates
nothing, locks nothing, makes no system call and throws nothing.
[Architecture overview](../architecture/OVERVIEW.md) describes the layers and the core
interfaces. `sd docs` writes the generated API reference to `build/docs/api-site/html/`.

## Do I need SushiStack or the other Sushi Systems repositories?

You need sushicore. `sd` depends on it, and it supplies the `setup`, `doctor`, `link`, `unlink`,
`config` and `env` commands.

SushiStack is optional: SushiDSP builds and tests without it. `sd setup` provisions into the
shared root, `~/.sushisystems` (or `SUSHISYSTEMS_HOME`), whether or not the checkout sits in a
SushiStack workspace. Inside a workspace, `sd link` records the checkout in the workspace's
module registry, and `sushi-module.toml` makes it recognisable to `hub`. The core itself links
no SushiRuntime, no SYCL and no game engine.
[`cli/README.md`](../../cli/README.md) says what the CLI takes from sushicore.

## Is there a VST or CLAP plugin?

No. A VST3/CLAP plugin wrapper is design intent only: no `apps/plugin` directory exists in the
tree. The same holds for any packaging step beyond what `sd build` and `sd test` exercise.

What the tree holds today is three standalone hosts: `apps/host`, a minimal SDL2-backed host;
`apps/host_asio`, its ASIO-backed counterpart; and `apps/host_gui`, the rig-editing GUI.
[Architecture overview](../architecture/OVERVIEW.md) lists what is specified and not yet built.

## What does SushiDSP not do yet?

[Known issues](../reference/KNOWN_ISSUES.md) records each measured defect that is left open,
with its figure and what closes it. Some that a first-time user meets:

- The Mesa Mark IIC+ model has no clean setting; it limits at every input level.
- The Mark IIC+ costs 1.852 ms for a 256-frame block against the 1.5 ms budget that block is
  allowed at 48 kHz, so a rig that includes it can underrun at the smallest USB buffer.
- The Diezel VH4's phase inverter and power section use the JCM800's figures as a stand-in.
- A take leaves the host as WAV only; OGG export is deferred.
- A 44.1 kHz WAV dropped on a 48 kHz rig plays 8.8 % slow, because a take's own sample rate is
  never read. Imported mp3 files are not affected.
- The ASIO host renders the rig in mono.

[Remaining work](../design/REMAINING_WORK.md) is the single backlog of what is open.

## How is SushiDSP licensed, and can I use it commercially?

SushiDSP is source-available, free for non-commercial use under the PolyForm Noncommercial
License 1.0.0. [`LICENSE`](../../LICENSE) is the binding text and lists the permitted purposes:
personal, non-commercial use, and use by the non-commercial organisations it names.

Any commercial purpose needs a separate, paid licence from Sushi Systems. That includes use
inside a company, use in paid client work, and use in a product or service that is sold. To ask
for one, write to hello@sushisystems.io with what you want to build and who will use it;
[`COMMERCIAL.md`](../../COMMERCIAL.md) says the same. [`NOTICE.md`](../../NOTICE.md) lists the
third-party libraries a build links, the third-party data in the tree and the terms of a build
with the Steinberg ASIO SDK.

## Where do I report a problem, and can I contribute?

Open an issue on the repository for a bug report or a design discussion. Contributions from
outside Sushi Systems are not accepted yet, so an issue is the way in;
[Contributing](../CONTRIBUTING.md) says so in its first paragraph.

Do not open a public issue for a security problem. Report it privately by email to
mustafagarip@sushisystems.io or through GitHub's private vulnerability reporting, under the
repository's Security tab. The [security policy](../../.github/SECURITY.md) promises an
acknowledgement within 72 hours and an initial assessment within 7 days.
