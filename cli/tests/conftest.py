# conftest.py
# SushiDSP - https://github.com/SushiSystems/SushiDSP
# Copyright (c) 2026-present Mustafa Garip & Sushi Systems
# Licensed under the Apache License 2.0. See LICENSE.
"""Test fixtures for the SushiDSP CLI.

``sushidsp`` imports :mod:`sushicore` -- the shared core underneath every
Sushi* CLI -- at module load, so the suite cannot even be collected without
it. sushicore ships inside the SushiStack repository: installed in the
environment, or sitting next door, and that is what these tests use.
"""

from __future__ import annotations

import importlib
import importlib.util
import os
import sys
from dataclasses import dataclass, field
from pathlib import Path

import pytest


def _ensure_sushicore() -> None:
    """Make :mod:`sushicore` importable, or fail with a message that says how.

    Already installed (CI does `pip install -e <stack>/sushicore`) wins. Failing
    that, it is found on disk: a workspace puts it at ``<workspace>/sushicore``,
    a developer's flat checkout layout at ``<siblings>/sushistack/sushicore``.
    """
    if importlib.util.find_spec("sushicore") is not None:
        return

    repo = Path(__file__).resolve().parents[2]
    for candidate in (repo.parent / "sushicore", repo.parent / "sushistack" / "sushicore"):
        if (candidate / "sushicore" / "__init__.py").is_file():
            sys.path.insert(0, str(candidate))
            importlib.invalidate_caches()
            if importlib.util.find_spec("sushicore") is not None:
                return

    raise RuntimeError(
        "sushicore is not importable, so the sushidsp CLI cannot be imported. "
        "It ships in the SushiStack repository: install it with "
        "`pip install -e <sushistack>/sushicore`, or place a SushiStack "
        "checkout next to this one."
    )


_ensure_sushicore()

from sushicore.build_tree import tree_dir  # noqa: E402
from sushicore.console_memory import RecordingConsole  # noqa: E402
from sushicore.proc_memory import RecordingRunner  # noqa: E402

from sushidsp.config import Config  # noqa: E402
from sushidsp.services.project import Workspace  # noqa: E402


K_SIZE_TIMEOUTS = {"small": 60, "medium": 300, "large": 900}


def _sizes_of(item: pytest.Item) -> list[str]:
    """Returns the size labels *item* carries, one entry per label."""
    return [mark.name for mark in item.iter_markers() if mark.name in K_SIZE_TIMEOUTS]


def pytest_collection_modifyitems(items: list[pytest.Item]) -> None:
    """Fails collection when a test carries no size or more than one, and gives each its timeout."""
    wrong = [f"{item.nodeid}: {_sizes_of(item) or 'no size'}"
             for item in items if len(_sizes_of(item)) != 1]
    if wrong:
        raise pytest.UsageError(f"every test carries exactly one size: {', '.join(wrong)}")
    for item in items:
        item.add_marker(pytest.mark.timeout(K_SIZE_TIMEOUTS[_sizes_of(item)[0]]))


class FakePackages:
    """Stands in for the resolver: records each request and returns the SushiLog and SushiProfiler packages."""

    config_dir = "/store/sushilog/abc/prefix/lib/cmake/SushiLog"
    profiler_config_dir = "/store/sushiprofiler/def/prefix/lib/cmake/SushiProfiler"
    run_dir = "/store/sushilog/abc/prefix/lib"
    left_out: frozenset[str] = frozenset()

    def __init__(self) -> None:
        """Starts with no request recorded."""
        self.asked: list[dict] = []

    def names(self) -> list[str]:
        """Returns the library names."""
        return ["sushilog", "sushiprofiler"]

    def definitions(self) -> list[str]:
        """Returns the config folder definition."""
        return [f"-DSushiLog_DIR={self.config_dir}",
                f"-DSushiProfiler_DIR={self.profiler_config_dir}"]

    def run_dirs(self, platform: str) -> list[str]:
        """Returns the run-time folder."""
        return [self.run_dir]

    def sushi_packages(self, root, build_type, options=None, report=print, warn=print,
                       compiler=""):
        """Records the request and returns this object as the package set."""
        self.asked.append({"root": root, "build_type": build_type,
                           "options": dict(options or {}), "compiler": compiler})
        return self


@dataclass
class Project:
    """Holds a scratch checkout and what the project service did to it."""

    root: Path
    runner: RecordingRunner
    console: RecordingConsole
    packages: FakePackages
    workspace: Workspace
    build_dir: Path = field(init=False)

    @property
    def commands(self) -> list[list[str]]:
        """Returns the argv of every command the runner was handed, in order."""
        return [list(command.argv) for command in self.runner.commands]

    def __post_init__(self) -> None:
        """Derives the default tree's path, build/default-debug, from the root."""
        self.build_dir = self.tree_of("Debug")

    def tree_of(self, build_type: str = "Debug", name: str = "default") -> Path:
        """Returns the path of the tree *name* owns for *build_type*."""
        return tree_dir(self.root, name, build_type)

    def configure_tree(self, build_type: str = "Debug", name: str = "default") -> Path:
        """Plants the files a finished Ninja configure of *build_type* leaves behind."""
        tree = self.tree_of(build_type, name)
        tree.mkdir(parents=True, exist_ok=True)
        (tree / "build.ninja").write_text("", encoding="utf-8")
        (tree / "CMakeCache.txt").write_text(
            f"CMAKE_BUILD_TYPE:STRING={build_type}\nCMAKE_GENERATOR:INTERNAL=Ninja\n",
            encoding="utf-8")
        return tree

    def plant_executable(self, stem: str, *folders: str, build_type: str = "Debug") -> Path:
        """Creates an empty runnable file named *stem* under the tree of *build_type*."""
        exe = self.tree_of(build_type).joinpath(
            *folders, f"{stem}.exe" if os.name == "nt" else stem)
        exe.parent.mkdir(parents=True, exist_ok=True)
        exe.write_text("", encoding="utf-8")
        exe.chmod(0o755)
        return exe

    def plant_host_gui(self, *folders: str, build_type: str = "Debug") -> Path:
        """Creates an empty host GUI binary under the tree's applications/host_gui."""
        return self.plant_executable("sushidsp_host_gui", "applications", "host_gui", *folders,
                                     build_type=build_type)


@pytest.fixture
def project(tmp_path) -> Project:
    """Returns a scratch checkout whose workspace records argv and starts nothing."""
    (tmp_path / "CMakeLists.txt").write_text("project(x)\n", encoding="utf-8")
    console = RecordingConsole()
    runner = RecordingRunner(console)
    cfg = Config(cxx="clang++", generator="Ninja", ninja_exe="ninja",
                 ccache_exe=str(tmp_path / "no_ccache"))
    packages = FakePackages()
    cfg.sushi_packages = packages.sushi_packages
    workspace = Workspace(tmp_path, cfg, console, runner, runner, environment={})
    return Project(tmp_path, runner, console, packages, workspace)
