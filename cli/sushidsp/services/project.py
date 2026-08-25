"""Project configure / build / test logic.

The cmake / ctest invocations are issued directly via subprocess, inheriting
the parent environment unchanged — SushiDSP needs no vcvars snapshot or SYCL
toolchain setup, so there is nothing to layer on top of ``os.environ``.

``tests/CMakeLists.txt`` registers every test via ``gtest_discover_tests``
with the CTest label ``unit`` assigned from the GTest suite-name prefix
convention (``Unit_*``), matching sushiblas/sushiengine/sushiruntime — see the
``Suite`` enum below. Only ``unit`` exists today; ``integration``/
``regression`` join once D-phase tests need them (see tests/CMakeLists.txt).
"""

from __future__ import annotations

import enum
import shutil
import subprocess
from pathlib import Path
from typing import Optional

from .. import console
from ..config import Config, find_project_root, load_config


class BuildType(str, enum.Enum):
    debug = "debug"
    release = "release"
    relwithdebinfo = "relwithdebinfo"


class Suite(str, enum.Enum):
    unit = "unit"
    all = "all"


_SUITE_LABEL_REGEX = {
    Suite.unit: "^unit$",
}

_CMAKE_BUILD_TYPE = {
    BuildType.debug: "Debug",
    BuildType.release: "Release",
    BuildType.relwithdebinfo: "RelWithDebInfo",
}


def _build_dir(root: Path) -> Path:
    return root / "build"


def _cmake(cfg: Config) -> str:
    """The cmake executable: the configured path if set, else bare 'cmake'."""
    return cfg.expand(cfg.cmake_exe) if cfg.cmake_exe else "cmake"


def _ctest(cfg: Config) -> str:
    """The ctest executable: the configured path if set, else bare 'ctest'."""
    return cfg.expand(cfg.ctest_exe) if cfg.ctest_exe else "ctest"


def _resolve_exe(name: str) -> str:
    """Return the full path to *name* from PATH, or *name* itself as fallback."""
    return shutil.which(name) or name


def _run(cmd: list[str], cwd: Path) -> int:
    resolved = list(cmd)
    resolved[0] = _resolve_exe(cmd[0])
    console.command(subprocess.list2cmdline(resolved))
    try:
        return subprocess.run(resolved, cwd=str(cwd)).returncode
    except FileNotFoundError:
        console.error(
            f"Executable not found: '{cmd[0]}'.\n"
            f"  - it is not on PATH and no explicit path is configured.\n"
            f"  - set its path in cli/config.local.toml (cmake_exe / ctest_exe).")
        return 1


def configure(build_type: BuildType, defines: Optional[list[str]] = None) -> int:
    """Runs `cmake -S . -B build` with the resolved generator and build type."""
    root = find_project_root()
    cfg = load_config()
    build_dir = _build_dir(root)
    build_dir.mkdir(parents=True, exist_ok=True)

    cmd = [
        _cmake(cfg), "-S", str(root), "-B", str(build_dir),
        f"-DCMAKE_BUILD_TYPE={_CMAKE_BUILD_TYPE[build_type]}",
    ]
    if cfg.generator:
        cmd += ["-G", cfg.generator]
    for define in defines or []:
        cmd += ["-D", define]

    return _run(cmd, root)


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
    cmd = [_cmake(cfg), "--build", str(build_dir), "--config", _CMAKE_BUILD_TYPE[build_type]]
    return _run(cmd, root)


def test(build_type: BuildType, suite: Suite, clean: bool = False) -> int:
    """Builds (if needed) then runs ctest, optionally filtered by *suite*."""
    rc = build(build_type, clean=clean)
    if rc != 0:
        return rc

    root = find_project_root()
    build_dir = _build_dir(root)
    cfg = load_config()

    cmd = [_ctest(cfg), "--test-dir", str(build_dir),
           "-C", _CMAKE_BUILD_TYPE[build_type], "--output-on-failure"]
    label_regex = _SUITE_LABEL_REGEX.get(suite)
    if label_regex is not None:
        cmd += ["-L", label_regex]

    return _run(cmd, root)
