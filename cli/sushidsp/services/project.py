# project.py
# SushiDSP - https://github.com/SushiSystems/SushiDSP
# Copyright (c) 2026 Sushi Systems
# Licensed under PolyForm Noncommercial 1.0.0. See LICENSE.
# Commercial use requires a licence from Sushi Systems.
"""Drives SushiDSP's configure, build, test, clean, docs, check, host and run steps.

Holds the build policy: the cache variables, the suites and their CTest labels, the checkers
and the host GUI's place in the tree. cmake and ctest run through
``sushicore.cmake_driver.CMakeDriver`` under the environment ``sushidsp.env`` assembles.
"""

from __future__ import annotations

import enum
import os
import shutil
import stat
import sys
from collections.abc import Sequence
from pathlib import Path

from sushicore import cmake_cache
from sushicore.cmake_driver import CMakeDriver
from sushicore.proc import Runner
from sushicore.toolchain_args import c_compiler_for

from .. import console
from ..config import Config, PROFILE, find_project_root, load_config
from ..env import load_build_env
from . import discovery


class BuildType(str, enum.Enum):
    """Names the build types `--type` accepts."""

    debug = "debug"
    release = "release"
    relwithdebinfo = "relwithdebinfo"


class Suite(str, enum.Enum):
    """Names the test suites `--suite` accepts."""

    unit = "unit"
    regression = "regression"
    benchmark = "benchmark"
    all = "all"


_SUITE_LABEL_REGEX = {
    Suite.unit: "^unit$",
    Suite.regression: "^regression$",
    Suite.benchmark: "^benchmark$",
}

_CMAKE_BUILD_TYPE = {
    BuildType.debug: "Debug",
    BuildType.release: "Release",
    BuildType.relwithdebinfo: "RelWithDebInfo",
}

_BUILD_TYPE_FROM_CMAKE = {value: key for key, value in _CMAKE_BUILD_TYPE.items()}

_CHECKERS = (
    Path("tools", "documentation", "check_docs_layout.py"),
    Path("tools", "documentation", "check_changelog.py"),
    Path("tools", "layering", "check_layering.py"),
)

_HOST_GUI = "sushidsp_host_gui"
_HOST_GUI_FOLDERS = (Path("apps", "host_gui"), Path("bin"))

_DOXYGEN_HINT = (
    "  - Windows: winget install DimitriVanHeesch.Doxygen\n"
    "  - Linux:   apt-get install -y doxygen graphviz\n"
    "  - macOS:   brew install doxygen graphviz\n"
    "  - or set doxygen_exe in cli/config.local.toml to an existing doxygen binary.")

_RUNNER = Runner(console, PROFILE.program)
_APP_RUNNER = Runner(console, PROFILE.program, catch_interrupt=True)
_DRIVER = CMakeDriver(console, _RUNNER)


def _build_dir(root: Path) -> Path:
    """Returns the build tree of the checkout at *root*."""
    return root / "build"


def _env(cfg: Config, build_dir: Path) -> dict[str, str]:
    """Returns the environment cmake, ctest and the host GUI run under."""
    return load_build_env(cfg, build_dir)


def resolve_build_type(requested: BuildType | None) -> BuildType:
    """Settles which build type a command acts on when --type was not given.

    Returns:
        The requested type; else the type a configured tree carries; else debug, also for a
        tree whose cache names a type this CLI does not offer.
    """
    if requested is not None:
        return requested
    cached = cmake_cache.cached_value(_build_dir(find_project_root()), "CMAKE_BUILD_TYPE")
    return _BUILD_TYPE_FROM_CMAKE.get(cached or "", BuildType.debug)


def _generator_changed(cfg: Config, build_dir: Path) -> bool:
    """Tells whether the tree was configured with a generator other than the one resolved now."""
    cached = cmake_cache.cached_value(build_dir, "CMAKE_GENERATOR")
    if cached is None:
        return False
    wanted = cfg.resolved_generator()
    if wanted == "":
        return not cached.startswith(("Visual Studio", "Unix Makefiles", "Xcode"))
    return cached != wanted


def _remove_tree(build_dir: Path) -> bool:
    """Removes the build tree, read-only files included, and reports a failure in one line.

    Returns:
        False when a file in the tree could not be removed.
    """
    def make_writable_and_retry(func, target, _exc_info) -> None:
        """Clears the read-only bit FetchContent's git objects carry and retries once."""
        os.chmod(target, stat.S_IWRITE)
        func(target)

    console.info(f"Removing {build_dir} ...")
    try:
        shutil.rmtree(build_dir, onerror=make_writable_and_retry)
    except OSError as error:
        console.error(
            f"Could not remove {error.filename or build_dir} ({error.strerror}); {build_dir} "
            "is left partly removed. Close the program that holds the file and run again.")
        return False
    return True


def _needs_configure(cfg: Config, build_dir: Path, expect: dict[str, str] | None) -> bool:
    """Tells whether the tree must be configured before it can be compiled."""
    if not (build_dir / "CMakeCache.txt").is_file():
        return True
    generator = cfg.resolved_generator()
    if generator:
        return _DRIVER.needs_configure(build_dir, generator, expect=expect)
    # CMake's platform default leaves no generator sentinel, only the cache entries.
    return any(cmake_cache.cached_value(build_dir, key) != value
               for key, value in (expect or {}).items())


def configure(build_type: BuildType | None, defines: Sequence[str] | None = None) -> int:
    """Runs `cmake -S . -B build` with the detected compiler, generator and build type.

    Args:
        defines: `VAR=VALUE` cache variables, passed after every flag this function sets.
    """
    build_type = resolve_build_type(build_type)
    root = find_project_root()
    cfg = load_config()
    build_dir = _build_dir(root)
    build_dir.mkdir(parents=True, exist_ok=True)

    cmd = [
        _DRIVER.cmake(cfg), "-S", str(root), "-B", str(build_dir),
        f"-DCMAKE_BUILD_TYPE={_CMAKE_BUILD_TYPE[build_type]}",
    ]
    generator = cfg.resolved_generator()
    if generator:
        cmd += ["-G", generator]
    if generator == "Ninja":
        cmd += [f"-DCMAKE_MAKE_PROGRAM={cfg.resolved_ninja()}"]
    compiler = cfg.resolved_compiler()
    if compiler:
        console.info(f"Compiler: {compiler}  [{cfg.compiler_source()}]")
        cmd += [f"-DCMAKE_CXX_COMPILER={compiler}",
                f"-DCMAKE_C_COMPILER={c_compiler_for(compiler)}"]
    else:
        console.info(f"Compiler: {cfg.compiler_source()}")
    for define in defines or ():
        cmd += ["-D", define]

    return _DRIVER.configure(cmd, cwd=root, env=_env(cfg, build_dir))


def build(build_type: BuildType | None, clean: bool = False,
          defines: Sequence[str] | None = None) -> int:
    """Configures when the tree needs it or *defines* are given, then compiles every target.

    Args:
        clean: Removes the build tree first.
        defines: `VAR=VALUE` cache variables; any of them reconfigures a configured tree.
    """
    # --clean removes the tree, so there is no cached type left to inherit from.
    build_type = resolve_build_type(build_type) if not clean else (build_type or BuildType.debug)
    root = find_project_root()
    cfg = load_config()
    build_dir = _build_dir(root)

    if build_dir.exists() and (clean or _generator_changed(cfg, build_dir)):
        if not clean:
            console.warn(
                "Build tree was configured with a different generator; rebuilding from scratch.")
        if not _remove_tree(build_dir):
            return 1

    # A single-config generator bakes CMAKE_BUILD_TYPE into the tree; Visual Studio does not.
    expect = None if cfg.is_multi_config() else {"CMAKE_BUILD_TYPE": _CMAKE_BUILD_TYPE[build_type]}
    if defines or _needs_configure(cfg, build_dir, expect):
        rc = configure(build_type, defines)
        if rc != 0:
            return rc

    return _DRIVER.compile(cfg, build_dir, cwd=root, env=_env(cfg, build_dir),
                           config=_CMAKE_BUILD_TYPE[build_type])


def _benchmark_ctest_args(cfg: Config, build_dir: Path, config: str,
                          filter: str | None) -> list[str]:
    """Returns the ctest command line of the benchmark suite, whose timing table needs -V."""
    args = [_DRIVER.ctest(cfg), "--test-dir", str(build_dir), "-C", config,
            "--output-on-failure", "-V", "-L", _SUITE_LABEL_REGEX[Suite.benchmark]]
    if filter:
        args += ["-R", filter]
    return args


def test(build_type: BuildType | None, suite: Suite, clean: bool = False,
         filter: str | None = None) -> int:
    """Builds when needed, then runs ctest over the tests *suite* and *filter* select.

    Args:
        filter: A ctest -R regex over the `Suite.Case` names gtest_discover_tests registers.
    """
    build_type = resolve_build_type(build_type) if not clean else (build_type or BuildType.debug)
    rc = build(build_type, clean=clean)
    if rc != 0:
        return rc

    root = find_project_root()
    build_dir = _build_dir(root)
    cfg = load_config()
    env = _env(cfg, build_dir)
    config = _CMAKE_BUILD_TYPE[build_type]

    # CMakeDriver.ctest_run has no verbose switch, so the benchmark suite builds its own argv.
    if suite is Suite.benchmark:
        return _RUNNER.run(_benchmark_ctest_args(cfg, build_dir, config, filter), root, env)
    return _DRIVER.ctest_run(cfg, build_dir, env, label_regex=_SUITE_LABEL_REGEX.get(suite),
                             filter=filter, config=config)


def clean() -> int:
    """Removes the build tree and reports what happened.

    Returns:
        1 when a file in the tree could not be removed, else 0.
    """
    console.header("Project Clean")
    build_dir = _build_dir(find_project_root())
    if not build_dir.exists():
        console.info(f"{build_dir} does not exist, nothing to clean.")
        return 0
    if not _remove_tree(build_dir):
        return 1
    console.success(f"{build_dir} removed.")
    return 0


def docs() -> int:
    """Generates the API reference from the headers through the Doxyfile at the root."""
    console.header("Doxygen Generation")
    root = find_project_root()
    cfg = load_config()
    return _DRIVER.doxygen(cfg, root / "Doxyfile", root, env=_env(cfg, _build_dir(root)),
                           install_hint=_DOXYGEN_HINT)


def check(report: bool = False, rule: str | None = None) -> int:
    """Runs the layout, changelog and layering checkers under tools/.

    Args:
        report: Makes each checker print issue counts and exit zero.
        rule: The one rule to run; it is forwarded to every checker.

    Returns:
        The highest exit code among the checkers.
    """
    root = find_project_root()
    arguments = (["--report"] if report else []) + (["--rule", rule] if rule else [])
    return max(_RUNNER.run([sys.executable, str(root / checker), *arguments], root)
               for checker in _CHECKERS)


def _locate_host_gui(cfg: Config, build_dir: Path, config: str) -> Path | None:
    """Returns the host GUI binary the tree holds for *config*, or None.

    A multi-config generator keeps each configuration in a folder of its own; a single-config
    tree holds one binary, of the type it was configured with.
    """
    name = f"{_HOST_GUI}.exe" if os.name == "nt" else _HOST_GUI
    suffix = Path(config) if cfg.is_multi_config() else Path()
    for folder in _HOST_GUI_FOLDERS:
        candidate = build_dir / folder / suffix / name
        if candidate.is_file():
            return candidate
    return None


def host(build_type: BuildType | None = None, launch: bool = True,
         app_args: Sequence[str] | None = None) -> int:
    """Builds the requested type, then launches the standalone host GUI of that type.

    Args:
        launch: False stops after the build.
        app_args: Arguments handed to the host GUI unchanged.

    Returns:
        The build's code when it fails or *launch* is off, 1 when the tree holds no host GUI,
        130 after Ctrl+C, else the host GUI's own exit code.
    """
    build_type = resolve_build_type(build_type)
    rc = build(build_type)
    if rc != 0 or not launch:
        return rc

    root = find_project_root()
    cfg = load_config()
    build_dir = _build_dir(root)
    config = _CMAKE_BUILD_TYPE[build_type]

    exe = _locate_host_gui(cfg, build_dir, config)
    if exe is None:
        console.error(
            f"{_HOST_GUI} was not found under {build_dir} after a {config} build. "
            "It is built only when SUSHIDSP_BUILD_GUI is ON.")
        return 1

    console.header(f"Launching {exe.name} ({config})")
    return _APP_RUNNER.run([str(exe), *(app_args or ())], root, _env(cfg, build_dir))


def run(target: str | None = None, sort: bool = False,
        app_args: Sequence[str] | None = None) -> int:
    """Runs an executable the build tree already holds, without building first.

    Args:
        target: The executable's name, matched exactly and then as a substring; None runs
            the configured `target_bin`.
        sort: Prompts for one executable from a list and ignores *target*.
        app_args: Arguments handed to the executable unchanged.

    Returns:
        1 when there is no build tree or no match, 130 after Ctrl+C, else the executable's code.
    """
    root = find_project_root()
    cfg = load_config()
    build_dir = _build_dir(root)
    if not build_dir.is_dir():
        console.error(f"{build_dir} does not exist. Run `sd build` first.")
        return 1

    exe = discovery.resolve(build_dir, console, target=target, sort=sort, default=cfg.target_bin)
    if exe is None:
        return 1

    console.info(f"Executing: {exe}")
    return _APP_RUNNER.run([str(exe), *(app_args or ())], root, _env(cfg, build_dir))
