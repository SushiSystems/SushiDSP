# The `sd` command line

SushiDSP is never built by calling `cmake` or `ctest` directly (`docs/CONTRIBUTING.md`). Every
build action goes through `sd`, the Typer-based CLI in `cli/sushidsp/cli.py`. This guide covers each of its twelve subcommands and how it picks a compiler.

## `sd configure`

Runs CMake's configure step against `build/`.

- `--type` / `-t debug | release | relwithdebinfo` — the build type. Defaults to whatever type
  the existing build tree already carries, else `debug`.
- `--define` / `-D VAR=VALUE` — passes a CMake cache variable through to configure. Repeatable.

## `sd build`

Configures the tree if it isn't already, then builds every CMake target.

- `--type` / `-t` — same values and default as `sd configure`.
- `--clean` — deletes the build tree before configuring.
- `--define` / `-D VAR=VALUE` — same as `sd configure`. Repeatable.

## `sd test`

Builds the tree if it isn't already, then runs `ctest`.

- `--type` / `-t` — same values and default as `sd configure`.
- `--suite` / `-s unit | regression | benchmark | all` — which CTest label to run. Defaults to
  `unit`.
- `--clean` — deletes the build tree before configuring.

## `sd config`

Prints the resolved configuration — every field `ToolConfig` carries (compiler, generator,
CMake/Ninja/CTest paths, vcvars, the intel/llvm bundle root) and which layer each value came
from: built-in default, `config.toml`, a `config.local.toml`, or an `SD_*` environment variable.
It then reports two things `sd env` doesn't: which step of the compiler search won
(`Config.compiler_source()`) and which generator was resolved (`Config.resolved_generator()`).
This is the command to run before asking "which compiler is actually building this."

## `sd env`

Prints the environment the build subprocesses run in: the `vcvars` snapshot plus the detected
toolchain's `bin` directory on `PATH`.

- `--all` / `-a` — show every variable, not just the build-relevant ones.

## `sd doxygen`

Generates the API reference at `build/docs/api-site/html/` from the headers under
`include/SushiDSP/`, starting from `docs/reference/API_REFERENCE.md`. Takes no flags.

## `sd check`

Runs repository layout, changelog, and layering checkers from `tools/`.

- `--report` / `-r` — prints issue counts without returning an error exit code.
- `--rule <name>` — runs only the specified rule function.

## `sd run`

Launches the `sushidsp_host_gui` standalone application.

- `--type` / `-t debug | release | relwithdebinfo` — specifies which configuration binary to launch.
- `[extra_args]...` — extra arguments forwarded directly to the executable.

## `sd setup`

Provisions what `cli/sushistack.deps.toml` lists into the shared root, `~/.sushisystems` (or
`SUSHISYSTEMS_HOME`), writes the tool paths it found to `config.local.toml`, then prints the
doctor report.

- `--dry-run` — reports what is missing and what would be written, and changes nothing.
- `--yes` — skips the confirmation prompt before the intel/llvm download.

## `sd doctor`

Reports whether this machine can build SushiDSP, one row per check, and exits non-zero when a
required check fails.

- `--for GROUP` — runs only that group's checks and counts its optional checks as required.

## `sd link`

Records this checkout in a SushiStack workspace's module registry.

- `--workspace <path>` — the workspace root to register this checkout in.

## `sd unlink`

Removes this checkout from the workspace's module registry.

- `--workspace <path>` — the workspace root to remove this checkout from.

## Compiler resolution order

SushiDSP pins no toolchain by default. `Config.resolved_compiler()` (`cli/sushidsp/config.py`)
searches for a C++17 compiler in this order, and `sd config`'s "Compiler search:" line names
whichever step won:

1. **An explicit `cxx`** — set as `cxx` in a `config.local.toml` or as the `SD_CXX` environment
   variable. Reported as `pinned (cxx)`.
2. **The intel/llvm `clang++` that `sd setup` provisions.** `sd setup` writes the bundle's
   root to `llvm_root` in `config.local.toml`, or a workspace provides it in the
   workspace-shared `config.local.toml`; if that field is
   absent, `Config.bundled_clang()` also probes
   `<workspace>/dependencies/toolchains/llvm-sycl-nightly` and
   `<workspace>/dependencies/toolchains/llvm-sycl`, in that order. Reported as
   `SushiStack workspace (intel/llvm)`.
3. **A `clang++` on `PATH`.** Reported as `PATH (clang++)`.
4. **CMake's platform default** — MSVC through the Visual Studio generator on Windows, the
   system compiler elsewhere — when none of the above resolved. Reported as
   `platform default (no clang++ found; `sd setup` provisions intel/llvm)`.

A compiler found in step 1–3 also decides the generator: `resolved_generator()` pairs it with
Ninja when a `ninja` binary is available (never with the Visual Studio generator, which would
otherwise build with MSVC regardless of the resolved compiler); with no `ninja` found, CMake's
single-config default (Makefiles or NMake) carries the compiler instead. `sd config`'s
"Generator:" line reports the result, or `(CMake platform default)` when nothing overrode it.

This order means a standalone checkout with no SushiStack workspace still builds — it falls
through to a `clang++` on `PATH`, or to CMake's own default — while a workspace upgrades the
compiler automatically the moment `sd setup` provisions one.

## Configuration precedence

`cli/config.toml` documents the layering `sd` resolves every value through, lowest to highest
precedence: built-in defaults, then `config.toml` (committed, shared defaults), then a
workspace-shared `config.local.toml`, then a repo-local `config.local.toml`, then `SD_*`
environment variables, then CLI flags such as `--type` or `--define`. Machine-specific paths —
a pinned compiler, CMake, or CTest — belong in a gitignored `config.local.toml` next to
`cli/config.toml`, using the same `[tool]` section layout.
