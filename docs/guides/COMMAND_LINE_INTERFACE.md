# The `sd` command line

SushiDSP is never built by calling `cmake` or `ctest` directly (`docs/CONTRIBUTING.md`). Every
build action goes through `sd`, the Typer-based CLI in `cli/sushidsp/cli.py`. This guide covers
its two root options, each of its fourteen subcommands and how it picks a compiler.

## Root options

- `--version`, prints `sushidsp-cli` and its installed version, then exits.
- `--describe`, prints the command catalogue as JSON, then exits: every command the help
  lists, with its parameters, in the shape SushiHub's `contract/describe.schema.json` fixes.

A `cli/config.toml` or `config.local.toml` that is not valid TOML ends any command with one
line naming the file and the position, and exit code 1.

## `sd configure`

Runs CMake's configure step against the tree of one build type. Each type has a tree of its
own, `build/default-debug`, `build/default-release` and `build/default-relwithdebinfo`, so
switching type never recompiles a tree. A sanitizer build has its own pair, `build/asan-<type>`
and `build/tsan-<type>`. A configure refuses a tree that holds another type, and refuses
`-D CMAKE_BUILD_TYPE=...`: the type picks the tree.

Every compile and link of a tree runs through sushicore's memory gate, which admits an edge only
when its expected peak fits in the free memory. An empty `build/build_gate/stop` file makes the
gate refuse every edge. When `ccache` is on `PATH`, or `ccache_exe` in `config.local.toml` or
`SD_CCACHE` names it, host compiles go through it.

- `--type` / `-t debug | release | relwithdebinfo`, the build type. Default: `debug`.
- `--define` / `-D VAR=VALUE`, passes a CMake cache variable through to configure. Repeatable.
- `--sanitize address | thread`, configures the tree of that sanitizer.

## `sd build`

Configures the tree if it isn't already, then builds every CMake target.

- `--type` / `-t`, same values and default as `sd configure`.
- `--clean`, deletes the tree of the type before configuring.
- `--define` / `-D VAR=VALUE`, passes a CMake cache variable to configure. On a tree that is
  already configured it runs configure again with the variable, then builds. Repeatable.
- `--sanitize address | thread`, builds the tree of that sanitizer with AddressSanitizer and
  UndefinedBehaviorSanitizer, or with ThreadSanitizer, through `SUSHIDSP_SANITIZE`. Clang and
  GCC only; MSVC ignores it.
- `--jobs` / `-j <count>`, compile jobs. Without it the count is the hardware threads, or
  `CMAKE_BUILD_PARALLEL_LEVEL` when that is set.
- `--report`, prints the edges this build compiled by kind, with their time and peak memory,
  and saves the report in the tree.

A variable set with `-D` stays in that tree's cache, so a later `sd build` of the same tree
without the flag keeps it. `-D VAR=` clears one; `--clean` starts over.

## `sd test`

Builds the tree if it isn't already, then runs `ctest`.

- `--type` / `-t`, same values and default as `sd configure`.
- `--label` / `-l small | medium | large`, the test size to run. Repeatable; without it every
  size runs. `docs/CONTRIBUTING.md` says what each size allows.
- `--filter` / `-f <regex>`, a `ctest -R` regex over the `Suite.Case` test names, applied
  inside the chosen sizes.
- `--jobs` / `-j <count>`, how many tests run at once, passed to `ctest --parallel`. Without it
  ctest decides and honours `CTEST_PARALLEL_LEVEL`.
- `--sanitize address | thread`, builds and tests the tree of that sanitizer, as
  `sd build --sanitize` does.
- `--clean`, deletes the tree of the type before configuring.

## `sd bench`

Builds the tree if it isn't already, then runs `sushidsp_bench` and prints its table. A case
that carries a checks file prints a verdict line under its row, `check pass` or `check FAIL`
with the reason, and a failure that a known issue explains is labelled with its id. Each
row is the median of 2000 repetitions on a profiler timeline, with the 99th percentile, the
maximum and the jitter beside it. A verdict never changes the exit code.

- `GROUP...`, substrings of the case groups to run, such as `Room` or `Jcm800`. Without one
  every group runs.
- `--type` / `-t`, same values as `sd configure`. Default: `release`, because a timing is a
  Release figure.
- `--clean`, deletes the tree of the type before configuring.

## `sd clean`

Removes one build tree, `build/default-debug` by default, including the read-only files
FetchContent's git checkouts leave under `_deps`. When a file is held open, by a running
`sushidsp_host_gui` for example, it prints one line naming the file and exits 1;
`sd build --clean` and `sd test --clean` stop the same way.

- `--type` / `-t`, the tree's build type. Default: `debug`.
- `--sanitize address | thread`, removes the tree of that sanitizer instead.
- `--all`, removes `build/` whole: every tree, the gate's state and its cache.

## `sd config`

Prints the resolved configuration, every field `Config` carries (compiler, generator,
CMake/Ninja/CTest paths, vcvars, the intel/llvm bundle root) and which layer each value came
from: built-in default, `config.toml`, a `config.local.toml`, or an `SD_*` environment variable.
It then reports two things `sd env` doesn't: which step of the compiler search won
(`Config.compiler_source()`) and which generator was resolved (`Config.resolved_generator()`).
This is the command to run before asking "which compiler is actually building this."

## `sd env`

Prints the environment the build subprocesses run in: the `vcvars` snapshot plus the detected
toolchain's `bin` directory on `PATH`.

- `--all` / `-a`, show every variable, not just the build-relevant ones.

## `sd docs`

Generates the API reference at `build/docs/api-site/html/` from the headers under
`include/SushiDSP/`, starting from `docs/reference/API_REFERENCE.md`. Takes no flags, and
exits with Doxygen's code.

`sd docs bundle --release X.Y.Z` writes the archive `docs.sushisystems.io` reads:
`docs-bundle-X.Y.Z.tar.gz` and its `.sha256` file. `docs/publish.toml` lists what goes in: the
manual pages of the sections it names and, because it sets `api = true`, the Doxygen XML from
`build/docs/api-site/xml`, which the command builds first.

- `--release X.Y.Z`, the release the bundle is built for, three integers. Required; without
  it the command exits 2.
- `--out <folder>`, where the archive is written. Defaults to `build/docs/bundle`.

A page with no level-one heading, a page `docs/README.md` does not link, or a link to a file
that does not exist stops the command with one line naming the page, and exit code 1. The
archive's layout is in SushiCore's `docs/reference/DOCS_BUNDLE.md`.

The bare command was `sd doxygen` until 2026-10-05. The old word still runs it, hidden from the
help, and says so once on a terminal; it is removed after 2027-03-25.
`SD_NO_DEPRECATION_NOTICE=1` silences the notice.

## `sd check`

Runs the eight shared checkers from `tools/`: source comments, docs layout, changelog, docs
truth, layering, test mirror, test sizes and logging. Each runs through this Python interpreter,
and the command exits with the highest of their exit codes. It does not run the checker unit
tests; `python tools/check_all.py .` does.

- `--report` / `-r`, prints issue counts without returning an error exit code.
- `--rule <name>`, runs only that rule, in the checkers whose rule table names it. A rule no
  checker names is an error.

## `sd host`

Builds the tree for the requested type, which does nothing when it is up to date, then
launches the `sushidsp_host_gui` standalone application of that type. It exits 1 when the
tree holds no host GUI (`SUSHIDSP_BUILD_GUI=OFF`) and 130 after Ctrl+C.

- `--type` / `-t debug | release | relwithdebinfo`, the configuration to build and launch.
  Default: `debug`.
- `--no-run`, builds and stops there.
- `-- <args>...`, everything after `--` reaches the host GUI unchanged.

## `sd run`

Runs an executable a build tree already holds, as `run` does on every Sushi CLI. It does not
build first; without the tree it names `sd build` and exits 1.

- `TARGET`, the executable's name. An exact match on the name or its stem wins; otherwise the
  first name containing `TARGET` runs. No match is one error line and exit code 1.
- `--type` / `-t`, whose tree to look in. Default: `debug`.
- `--sort`, lists the executables under the tree and prompts for one.
- `-- <args>...`, everything after `--` reaches the executable unchanged:
  `sd run sushidsp_tests -- --gtest_filter='Jcm800.*'`.

Until 2026-10-05 `sd run` launched the host GUI. With neither `TARGET` nor `--sort` it still
does what `sd host` does, `--type` included, and says so once on a terminal. `sd run -- <args>`
is the same old spelling: a first argument that starts with `-` cannot name an executable, so
it and what follows go to the host GUI. Both are removed after 2027-03-25; write
`sd host -- <args>`.

## `sd setup`

Provisions what `cli/sushistack.deps.toml` lists into the shared root, `~/.sushisystems` (or
`SUSHISYSTEMS_HOME`), writes the tool paths it found to `config.local.toml`, then prints the
doctor report.

- `--dry-run`, reports what is missing and what would be written, and changes nothing.
- `--yes`, skips the confirmation prompt before the intel/llvm download.
- `--toolchain <name>`, also installs that toolchain. SushiDSP marks intel/llvm optional, so
  it downloads only with `--toolchain intel-llvm`. Repeatable.
- `--no-gpu`, skips the GPU toolkit. SushiDSP declares no GPU dependency, so nothing changes.

## `sd doctor`

Reports whether this machine can build SushiDSP, one row per check, and exits non-zero when a
required check fails.

- `--for GROUP`, runs only that group's checks and counts its optional checks as required.

## `sd link`

Records this checkout in a SushiStack workspace's module registry.

- `--workspace <path>`, the workspace root to register this checkout in.

## `sd unlink`

Removes this checkout from the workspace's module registry.

- `--workspace <path>`, the workspace root to remove this checkout from.

## Compiler resolution order

SushiDSP pins no toolchain by default. `Config.resolved_compiler()` (`cli/sushidsp/config.py`)
searches for a C++17 compiler in this order, and `sd config`'s "Compiler search:" line names
whichever step won:

1. **An explicit `cxx`**: set as `cxx` in a `config.local.toml` or as the `SD_CXX` environment
   variable. Reported as `pinned (cxx)`.
2. **The intel/llvm `clang++` that `sd setup --toolchain intel-llvm` provisions.** `sd setup` writes the bundle's
   root to `llvm_root` in `config.local.toml`, or a workspace provides it in the
   workspace-shared `config.local.toml`. `Config.bundled_clang()` probes three places in
   order: `llvm_root` itself, reported as `llvm_root (intel/llvm)`; then
   `<provision root>/toolchains/`, where `sd setup` installs, reported as
   `provision root <root> (intel/llvm)`; then `<workspace>/dependencies/toolchains/`,
   reported as `SushiStack workspace (intel/llvm)`. Under the last two it looks for
   `llvm-sycl-nightly` before `llvm-sycl`.
3. **A `clang++` on `PATH`.** Reported as `PATH (clang++)`.
4. **CMake's platform default**: MSVC through the Visual Studio generator on Windows, the
   system compiler elsewhere, when none of the above resolved. Reported as
   `platform default (no clang++ found; `sd setup --toolchain intel-llvm` provisions one)`.

A compiler found in step 1–3 also decides the generator: `resolved_generator()` pairs it with
Ninja when a `ninja` binary is available (never with the Visual Studio generator, which would
otherwise build with MSVC regardless of the resolved compiler); with no `ninja` found, CMake's
single-config default (Makefiles or NMake) carries the compiler instead. `sd config`'s
"Generator:" line reports the result, or `(CMake platform default)` when nothing overrode it.

This order means a standalone checkout with no SushiStack workspace still builds, it falls
through to a `clang++` on `PATH`, or to CMake's own default, while a workspace upgrades the
compiler automatically the moment `sd setup --toolchain intel-llvm` provisions one.

## Configuration precedence

`cli/config.toml` documents the layering `sd` resolves every value through, lowest to highest
precedence: built-in defaults, then `config.toml` (committed, shared defaults), then a
workspace-shared `config.local.toml`, then a repo-local `config.local.toml`, then `SD_*`
environment variables, then CLI flags such as `--type` or `--define`. Machine-specific paths , 
a pinned compiler, CMake, or CTest, belong in a gitignored `config.local.toml` next to
`cli/config.toml`, using the same `[tool]` section layout.
