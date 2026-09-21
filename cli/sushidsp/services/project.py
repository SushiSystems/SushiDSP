# Copyright (c) 2026-present Mustafa Garip & Sushi Systems
# Licensed under the Apache License, Version 2.0. See LICENSE.
"""Project configure / build / test logic.

This module holds SushiDSP's own build policy — which cache variables it passes, which
targets and test suites it has — and drives cmake and ctest through
``sushicore.cmake_driver.CMakeDriver``, inheriting the parent environment unchanged:
SushiDSP needs no vcvars snapshot or SYCL toolchain setup, so there is nothing to layer
on top of ``os.environ``.

``tests/CMakeLists.txt`` registers every test via ``gtest_discover_tests``
with the CTest label ``unit`` assigned from the GTest suite-name prefix
convention (``Unit_*`` / ``Regression_*``), matching
sushiblas/sushiengine/sushiruntime — see the ``Suite`` enum below.
``integration`` joins once a D-phase test needs it (see tests/CMakeLists.txt).
"""

from __future__ import annotations

import enum
import os
import shutil
import stat
import subprocess
import sys
from pathlib import Path
from typing import Optional

from sushicore import cmake_cache
from sushicore.cmake_driver import CMakeDriver
from sushicore.proc import Runner
from sushicore.toolchain_args import c_compiler_for

from .. import console
from ..config import Config, PROFILE, find_project_root, load_config
from ..env import load_build_env


class BuildType(str, enum.Enum):
    debug = "debug"
    release = "release"
    relwithdebinfo = "relwithdebinfo"


class Suite(str, enum.Enum):
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


def _build_dir(root: Path) -> Path:
    return root / "build"


_BUILD_TYPE_FROM_CMAKE = {value: key for key, value in _CMAKE_BUILD_TYPE.items()}


def resolve_build_type(requested: Optional[BuildType]) -> BuildType:
    """Settles which build type a command acts on when --type was not given.

    An explicit flag wins. Otherwise an already configured tree keeps the type it carries,
    so that `sd test` after `sd build --type release` measures the Release binary instead of
    reconfiguring the tree to Debug and rebuilding it unoptimised. A tree that does not exist
    yet, or one whose cache names a type this CLI does not offer, falls back to debug.
    """
    if requested is not None:
        return requested
    try:
        build_dir = _build_dir(find_project_root())
    except Exception:
        return BuildType.debug
    cached = cmake_cache.cached_value(build_dir, "CMAKE_BUILD_TYPE")
    return _BUILD_TYPE_FROM_CMAKE.get(cached or "", BuildType.debug)


_RUNNER = Runner(console, PROFILE.program)
_DRIVER = CMakeDriver(console, _RUNNER)

_DOXYGEN_HINT = (
    "  - Windows: winget install DimitriVanHeesch.Doxygen\n"
    "  - Linux:   apt-get install -y doxygen graphviz\n"
    "  - macOS:   brew install doxygen graphviz\n"
    "  - or set doxygen_exe in cli/config.local.toml to an existing doxygen binary.")


def _env(cfg: Config, build_dir: Path) -> dict[str, str]:
    return load_build_env(cfg, build_dir)


def _generator_changed(cfg: Config, build_dir: Path) -> bool:
    """Whether the tree was configured with a generator other than the one resolved now.

    CMake refuses to switch a tree's generator in place, so this is the one case where
    the tree has to go before configure can run.
    """
    cached = cmake_cache.cached_value(build_dir, "CMAKE_GENERATOR")
    if cached is None:
        return False
    wanted = cfg.resolved_generator()
    if wanted == "":
        return not cached.startswith(("Visual Studio", "Unix Makefiles", "Xcode"))
    return cached != wanted


def _remove_tree(path: Path) -> None:
    """rmtree that also removes read-only files: FetchContent's git checkouts under
    build/_deps leave read-only objects that plain rmtree refuses on Windows."""
    def make_writable_and_retry(func, target, _exc_info):
        os.chmod(target, stat.S_IWRITE)
        func(target)
    shutil.rmtree(path, onerror=make_writable_and_retry)


def _needs_configure(cfg: Config, build_dir: Path, expect: Optional[dict[str, str]]) -> bool:
    if not (build_dir / "CMakeCache.txt").is_file():
        return True
    generator = cfg.resolved_generator()
    if generator:
        return _DRIVER.needs_configure(build_dir, generator, expect=expect)
    # CMake's platform default: no sentinel to check, only the baked build type.
    return any(cmake_cache.cached_value(build_dir, key) != value
               for key, value in (expect or {}).items())


def configure(build_type: Optional[BuildType], defines: Optional[list[str]] = None) -> int:
    """Runs `cmake -S . -B build` with the detected compiler, generator and build type."""
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
    for define in defines or []:
        cmd += ["-D", define]

    return _DRIVER.configure(cmd, cwd=root, env=_env(cfg, build_dir))


def build(build_type: Optional[BuildType], clean: bool = False,
          defines: Optional[list[str]] = None) -> int:
    """Configures (if needed or forced by --clean) then builds the project."""
    # --clean removes the tree, so there is no cached type left to inherit from.
    build_type = resolve_build_type(build_type) if not clean else (build_type or BuildType.debug)
    root = find_project_root()
    cfg = load_config()
    build_dir = _build_dir(root)

    if build_dir.exists() and (clean or _generator_changed(cfg, build_dir)):
        if not clean:
            console.warn("Build tree was configured with a different generator; rebuilding from scratch.")
        console.info(f"Removing {build_dir} ...")
        _remove_tree(build_dir)

    # A single-config generator bakes CMAKE_BUILD_TYPE into the tree, so changing -t is a
    # reconfigure there; the Visual Studio generator selects it at build time instead.
    expect = None if cfg.is_multi_config() else {"CMAKE_BUILD_TYPE": _CMAKE_BUILD_TYPE[build_type]}
    if _needs_configure(cfg, build_dir, expect):
        rc = configure(build_type, defines)
        if rc != 0:
            return rc

    return _DRIVER.compile(cfg, build_dir, cwd=root, env=_env(cfg, build_dir),
                           config=_CMAKE_BUILD_TYPE[build_type])


def test(build_type: Optional[BuildType], suite: Suite, clean: bool = False) -> int:
    """Builds (if needed) then runs ctest, optionally filtered by *suite*."""
    build_type = resolve_build_type(build_type) if not clean else (build_type or BuildType.debug)
    rc = build(build_type, clean=clean)
    if rc != 0:
        return rc

    root = find_project_root()
    build_dir = _build_dir(root)
    cfg = load_config()

    # Not CMakeDriver.ctest_run: it has no "-C <config>" knob (this generator can be
    # multi-config, unlike the other four modules' single-config Ninja/Make trees, so the
    # flag is load-bearing here and not just belt-and-braces) and it drains the child's
    # output through a pipe, where this call has always inherited stdout directly.
    cmd = [_DRIVER.ctest(cfg), "--test-dir", str(build_dir),
           "-C", _CMAKE_BUILD_TYPE[build_type], "--output-on-failure"]
    # The benchmark case always "passes" (it has no assertion, only a timing table), so
    # --output-on-failure alone would hide the very numbers this suite exists to show.
    if suite == Suite.benchmark:
        cmd += ["-V"]
    label_regex = _SUITE_LABEL_REGEX.get(suite)
    if label_regex is not None:
        cmd += ["-L", label_regex]

    return _RUNNER.run(cmd, root, env=_env(cfg, build_dir))


def doxygen() -> int:
    """Generates the API reference from the headers via the repo-root Doxyfile."""
    console.header("Doxygen Generation")
    root = find_project_root()
    cfg = load_config()
    doxyfile = root / "Doxyfile"
    return _DRIVER.doxygen(cfg, doxyfile, root, env=_env(cfg, _build_dir(root)), install_hint=_DOXYGEN_HINT)


def check(report: bool = False, rule: Optional[str] = None) -> int:
    """Runs repository layout, changelog, and layering checkers.

    Args:
        report: Whether to report issue counts instead of failing.
        rule: Optional single rule name to run.

    Returns:
        Non-zero exit code if any checker returns non-zero.
    """
    try:
        root = find_project_root()
    except (SystemExit, Exception):
        root = Path(__file__).resolve().parents[3]

    checkers = [
        root / "tools" / "documentation" / "check_docs_layout.py",
        root / "tools" / "documentation" / "check_changelog.py",
        root / "tools" / "layering" / "check_layering.py",
    ]

    exit_code = 0
    for checker in checkers:
        cmd = [sys.executable, str(checker)]
        if report:
            cmd.append("--report")
        if rule:
            cmd.extend(["--rule", rule])
        result = subprocess.run(cmd, cwd=root)
        if result.returncode != 0:
            exit_code = max(exit_code, result.returncode)

    return exit_code


def run(
    build_type: Optional[BuildType] = None,
    args: Optional[list[str]] = None,
) -> int:
    """Launches the sushidsp_host_gui standalone application.

    Args:
        build_type: Desired build configuration (debug, release, relwithdebinfo).
        args: Extra CLI arguments to pass to the executable.

    Returns:
        Process exit code.
    """
    root = find_project_root()
    cfg = load_config()
    bt = resolve_build_type(build_type)
    build_dir = _build_dir(root)

    exe_name = "sushidsp_host_gui.exe" if os.name == "nt" else "sushidsp_host_gui"
    cmake_bt = _CMAKE_BUILD_TYPE[bt]

    def _locate() -> Optional[Path]:
        candidates = [
            build_dir / "apps" / "host_gui" / exe_name,
            build_dir / "apps" / "host_gui" / cmake_bt / exe_name,
            build_dir / "bin" / exe_name,
            build_dir / "bin" / cmake_bt / exe_name,
        ]
        for p in candidates:
            if p.is_file():
                return p
        return None

    exe_path = _locate()
    if exe_path is None:
        console.header(f"Building {exe_name}")
        build_res = build(bt)
        if build_res != 0:
            return build_res
        exe_path = _locate()

    if exe_path is None:
        console.header(f"Executable {exe_name} not found")
        return 1

    console.header(f"Launching {exe_path.name} ({cmake_bt})")
    env = _env(cfg, build_dir)
    cmd = [str(exe_path), *(args or [])]
    try:
        proc = subprocess.run(cmd, cwd=root, env=env)
        return proc.returncode
    except KeyboardInterrupt:
        return 0


