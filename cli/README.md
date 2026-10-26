# sushidsp-cli

`cli/` is `sd`, the SushiDSP developer CLI: a Python package that configures, builds, tests and
runs the repository. Each command in `sushidsp/cli.py` parses its arguments and hands them to
`sushidsp/services/`, which issues the `cmake` and `ctest` calls. The commands and their flags
are described in [`docs/guides/COMMAND_LINE_INTERFACE.md`](../docs/guides/COMMAND_LINE_INTERFACE.md).

## Install

`pyproject.toml` requires Python 3.10 and `sushicore>=0.7.0`. sushicore is not on PyPI, so
install it from its own checkout first:

```bash
python -m pip install -e <sushicore checkout>
python -m pip install -e ./cli
```

The distribution is `sushidsp-cli`. Its console script is `sd`, with `sushidsp` as a second
name and `python -m sushidsp` as a third way in.

## What it owns

| Module | What it owns |
| --- | --- |
| `cli.py` | The Typer commands and `main()`, the entry point. |
| `aliases.py` | The old spellings `sd` still answers to, removed after 2027-03-25. |
| `config.py` | The module profile, the layered configuration and the compiler search. |
| `env.py` | The environment `cmake`, `ctest` and the host GUI run under. |
| `console.py` | The console, built from `config.toml` on first use. |
| `services/project.py` | Configure, build, test, clean, docs, check, host and run. |
| `services/library_packages.py` | SushiLog as a built package: the request, the configure arguments and the run-time folders. |
| `services/discovery.py` | The executable search behind `sd run`, over sushicore's index. |
| `services/diag.py` | The diagnostics object behind `sd config` and `sd env`. |

sushicore supplies `--version`, `--describe`, `config`, `env`, `setup`, `doctor`, `link`,
`unlink`, the alias table's mechanism and the entry point that turns a configuration error into
one line. It also supplies the `docs` group: `cli.py` hands `register_docs_commands` the
Doxygen build in `services/project.py` and the folder its XML lands in, so `sd docs` builds the
API reference and `sd docs bundle` packs the documentation bundle `docs/publish.toml` describes.

## Configuration

`config.toml` in this folder holds the committed defaults. A gitignored `config.local.toml`
beside it holds machine-specific paths, in the same `[tool]` layout. `SD_*` environment
variables override both: `SD_CXX`, `SD_CMAKE_GENERATOR`, `SD_CMAKE`, `SD_CTEST`, `SD_NINJA`,
`SD_VCVARS` and `SD_TARGET_BIN`. `sd config` prints every value and the layer it came from.

## Tests

```bash
cd cli
python -m pytest -q
```

The suite starts no `cmake`, `ctest` or build. `tests/conftest.py` provides the `project`
fixture, which records the command lines the project service would run.

## Notes on the source

### config.py

The layers, from lowest precedence to highest: built-in defaults, `config.toml`, the workspace
`config.local.toml`, the repository `config.local.toml`, then the `SD_*` environment variables.

SushiDSP pins no toolchain. It builds with whatever C++17 compiler it can find, in this order,
and `sd config` reports which one won:

1. an explicit `cxx`, from `config.local.toml` or `SD_CXX`;
2. the intel/llvm `clang++` that `sd setup` or the workspace provides (`llvm_root` in a
   `config.local.toml`);
3. a `clang++` on `PATH`;
4. nothing, which leaves CMake's platform default: MSVC through the Visual Studio generator on
   Windows, the system compiler elsewhere.

Step 4 is why a standalone checkout keeps building with no SushiStack at all. The workspace
only upgrades the compiler when it is there to be found.

`_ENV_OVERRIDES` maps each field to its environment variable and is derived from the profile's
prefix. It is a module attribute because `sd config` reports which values came from the
environment and needs the reverse map.

`Config.generator` is left unset so that, with no compiler found, CMake picks the platform
default, which on Windows is Visual Studio's multi-config generator. When a compiler is found,
`resolved_generator()` chooses the generator.

### console.py

`console.py` is a thin wrapper around sushicore. The theme, icon and renderer logic and its
`[cli]` config schema live there, shared with every other Sushi CLI; sushicore's README says
how to change colours.

`sushicore.cli_console.LazyConsole` builds the console on first use. Constructing it needs this
repository's `cli/` directory, and locating that fails outside a checkout. Building at import
time would make every `sd` invocation abort with "Not inside a SushiDSP project", `sd --help`
included, which Typer can serve with no project at all. The module-level `__getattr__` (PEP 562)
resolves attributes on first access, so importing the module never touches the filesystem or
the project layout.

### env.py

A found `clang++` needs two things the parent shell may not have. On Windows it needs the MSVC
headers, libraries and linker that `clang++` targets. `env.py` loads them by running `vcvars64`
in a child shell and snapshotting the result, and caches the snapshot on disk keyed by the
settings that produced it. On every platform it needs the toolchain's own `bin` directory, and
on Linux its `lib` directory, ahead on `PATH`.

With no compiler found nothing is snapshotted and the current environment is used as is. A
plain checkout with MSVC or g++ already reachable needs no help.

### services/library_packages.py

SushiDSP links SushiLog as a built package and never compiles it. Every `sd build` asks
`cfg.sushi_packages` for the packages of its closure, with the compiler `resolved_compiler`
finds and the build type of the command, builds a missing one through SushiLog's own CLI, and
prints one `reused` or `built` line. The configure passes `-DSushiLog_DIR` and no
`-DSUSHILOG_DIR`. The packages are chosen on every build, so a changed SushiLog reaches the
link and the path without a reconfigure. A package that cannot be chosen ends the command with
one error line. With no compiler found there is nothing to build a package with, so the
resolver refuses.

The run-time folders of the packages go first on `PATH` (Windows) or `LD_LIBRARY_PATH` (Linux)
in `env.py`'s result for every command. `build` records them in `.sushidsp_package_dirs.json`
in the tree, and `sd run`, `sd host` and `sd test` read the file, so a program starts with the
same folders the build linked.

### tests/test_console_lazy.py

These are regression tests for the lazy console. `console.py` once built the console at import
time, which called `config_dir()`, then `find_project_root()`, and raised `SystemExit` outside
a checkout. `cli.py` imports the console transitively, so every `sd` invocation died with "Not
inside a SushiDSP project". That included `sd --help`, which Typer can answer with no project.

Four of the five CLIs fixed this independently and sushiengine's copy never did, so `se --help`
outside a checkout failed for as long as the copies existed. The lifecycle now lives in
`sushicore.cli_console.LazyConsole`. The tests pin that this CLI is wired to it, which is the
part a shared implementation cannot check on its own.
