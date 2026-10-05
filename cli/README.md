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
| `services/discovery.py` | The executable search behind `sd run`, over sushicore's index. |
| `services/diag.py` | The diagnostics object behind `sd config` and `sd env`. |

sushicore supplies `--version`, `--describe`, `config`, `env`, `setup`, `doctor`, `link`,
`unlink`, the alias table's mechanism and the entry point that turns a configuration error into
one line.

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
