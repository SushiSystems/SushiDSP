# conftest.py
# SushiDSP - https://github.com/SushiSystems/SushiDSP
# Copyright (c) 2026-present Mustafa Garip & Sushi Systems
# Licensed under PolyForm Noncommercial 1.0.0. See LICENSE.
# Commercial use requires a licence from Sushi Systems.
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


class RecordingRunner:
    """Records every command line it is handed and starts none of them."""

    def __init__(self) -> None:
        """Starts with no command recorded."""
        self.commands: list[list[str]] = []

    def resolve_exe(self, name: str, env: dict[str, str] | None = None) -> str:
        """Returns the name unchanged, as for a tool that is not on PATH."""
        return name

    def run(self, cmd: list[str], cwd: Path, env: dict[str, str] | None = None) -> int:
        """Records the command and reports success."""
        self.commands.append(list(cmd))
        return 0

    def run_drained(self, cmd: list[str], cwd: Path, env: dict[str, str] | None = None) -> int:
        """Records the command and reports success."""
        return self.run(cmd, cwd, env)


class RecordingConsole:
    """Records every line a service prints, keyed by the helper that printed it."""

    def __init__(self) -> None:
        """Starts with no line recorded."""
        self.lines: list[tuple[str, str]] = []

    def __getattr__(self, kind: str):
        """Returns a printer that records its text under the helper's name."""
        return lambda text: self.lines.append((kind, text))

    def of(self, kind: str) -> list[str]:
        """Returns the texts printed through one helper, in order."""
        return [text for printed, text in self.lines if printed == kind]


@dataclass
class Project:
    """Holds a scratch checkout and what the project service did to it."""

    root: Path
    runner: RecordingRunner
    console: RecordingConsole
    build_dir: Path = field(init=False)

    def __post_init__(self) -> None:
        """Derives the build tree's path from the root."""
        self.build_dir = self.root / "build"

    def configure_tree(self, build_type: str = "Debug") -> None:
        """Plants the files a finished Ninja configure of *build_type* leaves behind."""
        self.build_dir.mkdir(exist_ok=True)
        (self.build_dir / "build.ninja").write_text("", encoding="utf-8")
        (self.build_dir / "CMakeCache.txt").write_text(
            f"CMAKE_BUILD_TYPE:STRING={build_type}\nCMAKE_GENERATOR:INTERNAL=Ninja\n",
            encoding="utf-8")

    def plant_executable(self, stem: str, *folders: str) -> Path:
        """Creates an empty runnable file named *stem* under build/*folders*; returns its path."""
        exe = self.build_dir.joinpath(*folders, f"{stem}.exe" if os.name == "nt" else stem)
        exe.parent.mkdir(parents=True, exist_ok=True)
        exe.write_text("", encoding="utf-8")
        exe.chmod(0o755)
        return exe

    def plant_host_gui(self, *folders: str) -> Path:
        """Creates an empty host GUI binary under build/apps/host_gui and returns its path."""
        return self.plant_executable("sushidsp_host_gui", "apps", "host_gui", *folders)


@pytest.fixture
def project(tmp_path, monkeypatch) -> Project:
    """Returns a scratch checkout whose project service records argv and starts nothing."""
    from sushicore.cmake_driver import CMakeDriver

    from sushidsp.config import Config
    from sushidsp.services import project as project_svc

    (tmp_path / "CMakeLists.txt").write_text("project(x)\n", encoding="utf-8")
    runner, console = RecordingRunner(), RecordingConsole()
    cfg = Config(cxx="clang++", generator="Ninja", ninja_exe="ninja")
    monkeypatch.setattr(project_svc, "find_project_root", lambda: tmp_path)
    monkeypatch.setattr(project_svc, "load_config", lambda: cfg)
    monkeypatch.setattr(project_svc, "load_build_env", lambda cfg, build_dir: {})
    monkeypatch.setattr(project_svc, "console", console)
    monkeypatch.setattr(project_svc, "_RUNNER", runner)
    monkeypatch.setattr(project_svc, "_APP_RUNNER", runner, raising=False)
    monkeypatch.setattr(project_svc, "_DRIVER", CMakeDriver(console, runner))
    return Project(tmp_path, runner, console)
