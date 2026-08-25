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
import shutil
from pathlib import Path
from typing import Optional

from sushicore.cmake_driver import CMakeDriver
from sushicore.proc import Runner

from .. import console
from ..config import PROFILE, find_project_root, load_config


class BuildType(str, enum.Enum):
    debug = "debug"
    release = "release"
    relwithdebinfo = "relwithdebinfo"


class Suite(str, enum.Enum):
    unit = "unit"
    regression = "regression"
    all = "all"


_SUITE_LABEL_REGEX = {
    Suite.unit: "^unit$",
    Suite.regression: "^regression$",
}

_CMAKE_BUILD_TYPE = {
    BuildType.debug: "Debug",
    BuildType.release: "Release",
    BuildType.relwithdebinfo: "RelWithDebInfo",
}


def _build_dir(root: Path) -> Path:
    return root / "build"


_RUNNER = Runner(console, PROFILE.program)
_DRIVER = CMakeDriver(console, _RUNNER)

_DOXYGEN_HINT = (
    "  - Windows: winget install DimitriVanHeesch.Doxygen\n"
    "  - Linux:   apt-get install -y doxygen graphviz\n"
    "  - macOS:   brew install doxygen graphviz\n"
    "  - or set doxygen_exe in cli/config.local.toml to an existing doxygen binary.")


def configure(build_type: BuildType, defines: Optional[list[str]] = None) -> int:
    """Runs `cmake -S . -B build` with the resolved generator and build type."""
    root = find_project_root()
    cfg = load_config()
    build_dir = _build_dir(root)
    build_dir.mkdir(parents=True, exist_ok=True)

    cmd = [
        _DRIVER.cmake(cfg), "-S", str(root), "-B", str(build_dir),
        f"-DCMAKE_BUILD_TYPE={_CMAKE_BUILD_TYPE[build_type]}",
    ]
    if cfg.generator:
        cmd += ["-G", cfg.generator]
    for define in defines or []:
        cmd += ["-D", define]

    return _DRIVER.configure(cmd, cwd=root, env=None)


def build(build_type: BuildType, clean: bool = False,
          defines: Optional[list[str]] = None) -> int:
    """Configures (if needed or forced by --clean) then builds the project."""
    root = find_project_root()
    build_dir = _build_dir(root)

    if clean and build_dir.exists():
        console.info(f"Removing {build_dir} ...")
        shutil.rmtree(build_dir)

    if clean or not (build_dir / "CMakeCache.txt").is_file():
        rc = configure(build_type, defines)
        if rc != 0:
            return rc

    cfg = load_config()
    return _DRIVER.compile(cfg, build_dir, cwd=root, env=None,
                           config=_CMAKE_BUILD_TYPE[build_type])


def test(build_type: BuildType, suite: Suite, clean: bool = False) -> int:
    """Builds (if needed) then runs ctest, optionally filtered by *suite*."""
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
    label_regex = _SUITE_LABEL_REGEX.get(suite)
    if label_regex is not None:
        cmd += ["-L", label_regex]

    return _RUNNER.run(cmd, root, env=None)


def doxygen() -> int:
    """Generates the API reference from the headers via the repo-root Doxyfile."""
    console.header("Doxygen Generation")
    root = find_project_root()
    cfg = load_config()
    doxyfile = root / "Doxyfile"
    return _DRIVER.doxygen(cfg, doxyfile, root, env=None, install_hint=_DOXYGEN_HINT)
