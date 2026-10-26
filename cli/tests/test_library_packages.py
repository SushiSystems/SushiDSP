# test_library_packages.py
# SushiDSP - https://github.com/SushiSystems/SushiDSP
# Copyright (c) 2026-present Mustafa Garip & Sushi Systems
# Licensed under PolyForm Noncommercial 1.0.0. See LICENSE.
# Commercial use requires a licence from Sushi Systems.
"""Tests that `sd` consumes SushiLog as a package and puts its run-time folders on the path."""

from __future__ import annotations

import os

import pytest
from sushicore.provision.sushi_packages import PackageError

from sushidsp.config import Config
from sushidsp.services import library_packages
from sushidsp.services import project as project_svc
from sushidsp.services.project import BuildType

pytestmark = pytest.mark.medium


class _EnvRunner:
    """Records the environment of every command and starts none of them."""

    def __init__(self) -> None:
        """Starts with no environment recorded."""
        self.environments: list[dict[str, str]] = []

    def run(self, cmd, cwd, env=None) -> int:
        """Records the environment and reports success."""
        self.environments.append(dict(env or {}))
        return 0


def test_the_build_asks_for_the_sushilog_closure_with_its_compiler_and_type(project):
    assert project_svc.build(BuildType.relwithdebinfo) == 0

    assert project.packages.asked == [{
        "root": project.root, "build_type": "relwithdebinfo", "options": {},
        "compiler": "clang++"}]


def test_the_packages_are_chosen_on_every_build_not_only_on_a_configure(project):
    project.configure_tree("Debug")

    assert project_svc.build(BuildType.debug) == 0

    assert len(project.packages.asked) == 1
    assert "-S" not in project.runner.commands[0]


def test_the_configure_command_asks_once_and_passes_the_package(project):
    assert project_svc.configure(BuildType.debug) == 0

    assert len(project.packages.asked) == 1
    assert f"-DSushiLog_DIR={project.packages.config_dir}" in project.runner.commands[0]


def test_the_run_dirs_are_recorded_and_lead_the_search_path_of_a_run(project, monkeypatch):
    runner = _EnvRunner()
    monkeypatch.setattr(project_svc, "_APP_RUNNER", runner)
    project.plant_executable("sushidsp_host")
    assert project_svc.build(BuildType.debug) == 0

    assert library_packages.read_run_dirs(project.build_dir) == [project.packages.run_dir]
    assert project_svc.run(target="sushidsp_host") == 0

    path = runner.environments[0]["LD_LIBRARY_PATH"].split(os.pathsep)
    assert path[0] == project.packages.run_dir


def test_the_windows_run_dirs_lead_path():
    env = {"PATH": "C:/old"}

    library_packages.prepend_run_dirs(Config(platform="windows"), env, ["C:/pkg/bin"])

    assert env["PATH"].startswith("C:/pkg/bin")


def test_a_package_that_cannot_be_chosen_ends_the_build_in_one_error_line(project, monkeypatch):
    def refuse(*arguments, **options):
        raise PackageError("SushiLog has no cli and no stored package.")

    monkeypatch.setattr(project_svc.load_config(), "sushi_packages", refuse)

    assert project_svc.build(BuildType.debug) == 1

    assert project.console.of("error") == ["SushiLog has no cli and no stored package."]
    assert project.runner.commands == []
