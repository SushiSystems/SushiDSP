# Copyright (c) 2026-present Mustafa Garip & Sushi Systems
# Licensed under the Apache License, Version 2.0. See LICENSE.
"""Pins the two paths `sd setup` takes and what each one runs.

Nothing here installs a package, reaches the network, or reads this machine's
real workspace: the workspace lookup and the command runner are both replaced
with recorders, and the fragment is written into tmp_path.
"""

from __future__ import annotations

from pathlib import Path

import pytest

from sushidsp.services import setup as setup_svc


class _Runner:
    """Records what would have been run, and runs nothing.

    *hub* is what resolve_exe answers for "hub"; the name itself means not found.
    """

    def __init__(self, hub: str = "/usr/bin/hub") -> None:
        self.commands: list[list[str]] = []
        self.codes: list[int] = []
        self._hub = hub

    def resolve_exe(self, name: str, env=None) -> str:
        return self._hub if name == "hub" else name

    def run(self, cmd: list[str], cwd: Path, env=None) -> int:
        self.commands.append(list(cmd))
        return self.codes.pop(0) if self.codes else 0


_FRAGMENT = """
[intel-llvm]
description = "A compiler preference that installs nothing."
required = false
linux_apt = []
windows_vcpkg = []

[sdl2]
description = "Audio device backend."
required = true
linux_apt = ["libsdl2-dev"]
windows_vcpkg = ["sdl2"]
"""


@pytest.fixture
def repo(tmp_path, monkeypatch):
    """A checkout carrying SushiDSP's fragment, with no workspace above it."""
    path = tmp_path / "sushidsp" / setup_svc.FRAGMENT
    path.parent.mkdir(parents=True)
    path.write_text(_FRAGMENT, encoding="utf-8")
    root = tmp_path / "sushidsp"
    monkeypatch.setattr(setup_svc, "find_project_root", lambda: root)
    monkeypatch.setattr(setup_svc, "workspace_home", lambda root=None: None)
    return root


@pytest.fixture
def runner(monkeypatch):
    recorder = _Runner()
    monkeypatch.setattr(setup_svc, "_RUNNER", recorder)
    return recorder


@pytest.fixture
def workspace(tmp_path, monkeypatch):
    home = tmp_path / "workspace"
    home.mkdir()
    monkeypatch.setattr(setup_svc, "workspace_home", lambda root=None: home)
    return home


def test_a_workspace_hands_the_job_to_hub(repo, runner, workspace):
    assert setup_svc.setup(install=True) == 0
    assert runner.commands == [["hub", "install"]]


def test_a_workspace_reports_hub_without_installing(repo, runner, workspace):
    assert setup_svc.setup() == 0
    assert runner.commands == []


def test_a_workspace_without_hub_falls_back_to_the_fragment(repo, monkeypatch, workspace):
    recorder = _Runner(hub="hub")
    monkeypatch.setattr(setup_svc, "_RUNNER", recorder)
    assert setup_svc.setup(install=True) == 0
    assert recorder.commands == [setup_svc.deps_fragment.install_command(
        setup_svc.deps_fragment.Dependency(
            name="sdl2", linux_apt=["libsdl2-dev"], windows_vcpkg=["sdl2"]),
        setup_svc._platform())]


def test_standalone_names_the_command_and_runs_nothing(repo, runner, capsys):
    assert setup_svc.setup() == 0
    assert runner.commands == []
    out = capsys.readouterr().out
    assert "libsdl2-dev" in out or "sdl2" in out


def test_standalone_installs_exactly_what_it_reported(repo, runner):
    assert setup_svc.setup(install=True) == 0
    assert len(runner.commands) == 1
    assert "sdl2" in " ".join(runner.commands[0])


def test_an_entry_with_no_package_here_installs_nothing(repo, runner):
    setup_svc.setup(install=True)
    assert all("intel-llvm" not in " ".join(cmd) for cmd in runner.commands)


def test_a_failing_install_is_reported_as_its_exit_code(repo, runner):
    runner.codes = [3]
    assert setup_svc.setup(install=True) == 3
