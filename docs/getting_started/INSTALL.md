# Install SushiDSP

SushiDSP installs in two ways. Both give the same checkout and the same `sd` command, and
neither needs the other: SushiHub is not required to build SushiDSP, and a checkout made
without it joins a SushiHub workspace later without any change.

## Standalone

You need:

- Git, Python 3.10 or later and CMake 3.20 or later.
- A C++17 compiler, on Windows or Linux. `sd setup` writes the tool paths it finds to
  `cli/config.local.toml`; [`COMMAND_LINE_INTERFACE.md`](../guides/COMMAND_LINE_INTERFACE.md)
  has the compiler search.
- Network access, for the libraries `cli/sushistack.deps.toml` lists and for the GoogleTest
  release that CMake fetches.

SushiDSP needs no sibling repository: its core links the C++17 standard library and SIMD
intrinsics only.

```bash
git clone https://github.com/SushiSystems/SushiDSP.git
cd SushiDSP
python -m pip install -e ./cli
sd setup
sd build
sd test
```

`pip install` also installs `sushicore`, the command-line layer `sd` shares with the other Sushi
modules, from PyPI at the version `cli/pyproject.toml` pins.

## With SushiHub

```bash
hub add sushidsp            # clone it into the workspace
hub install-cli sushidsp    # put sd on the path
hub link sushidsp <path>    # adopt a checkout you already have, instead of hub add
```

`hub link` records where the checkout is and changes nothing in it; `sushi-module.toml` is what
makes it recognisable. After either route, `sd build` and `sd test` are the commands above.
