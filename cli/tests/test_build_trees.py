# test_build_trees.py
# SushiDSP - https://github.com/SushiSystems/SushiDSP
# Copyright (c) 2026-present Mustafa Garip & Sushi Systems
# Licensed under the Apache License 2.0. See LICENSE.
"""The build type a command acts on, the tree each type and sanitizer owns, and the memory gate."""

from __future__ import annotations

from dataclasses import replace
from pathlib import Path

import pytest
from sushicore import build_admission
from sushicore.build_admission import BuildStoppedError
from sushicore.console_memory import RecordingConsole
from sushicore.errors import ConfigError
from sushicore.proc_memory import RecordingRunner

from sushidsp.config import Config
from sushidsp.services import project as project_svc
from sushidsp.services.project import BuildType, Sanitizer, Workspace

K_ROOT = Path("checkout")


def _configure_of(project) -> list[str]:
    """Returns the recorded cmake configure command."""
    return next(command for command in project.commands if "-S" in command)


def _define_values(command: list[str]) -> list[str]:
    """Returns the value of every `-D VALUE` pair of a recorded configure command."""
    return [command[index + 1] for index, word in enumerate(command) if word == "-D"]


@pytest.mark.small
@pytest.mark.parametrize(("requested", "default", "resolved"), [
    (None, None, BuildType.debug),
    (BuildType.release, None, BuildType.release),
    (None, project_svc.K_BENCHMARK_TYPE, BuildType.release),
    (BuildType.debug, project_svc.K_BENCHMARK_TYPE, BuildType.debug),
])
def test_a_command_builds_the_type_it_is_given_else_its_default(requested, default, resolved):
    """A command without a type builds Debug, a benchmark builds Release, and a given type wins."""
    if default is None:
        assert project_svc.resolve_build_type(requested) is resolved
    else:
        assert project_svc.resolve_build_type(requested, default) is resolved


@pytest.mark.small
@pytest.mark.parametrize(("build_type", "sanitizer", "name"), [
    (BuildType.debug, None, "default-debug"),
    (BuildType.release, None, "default-release"),
    (BuildType.relwithdebinfo, None, "default-relwithdebinfo"),
    (BuildType.debug, Sanitizer.address, "asan-debug"),
    (BuildType.relwithdebinfo, Sanitizer.thread, "tsan-relwithdebinfo"),
])
def test_each_type_and_sanitizer_owns_one_tree(build_type, sanitizer, name):
    """The workspace gives the type and the sanitizer the tree build/<name>-<type>."""
    console = RecordingConsole()
    runner = RecordingRunner(console)
    workspace = Workspace(K_ROOT, Config(), console, runner, runner)

    assert workspace.tree_path(build_type, sanitizer) == K_ROOT / "build" / name


@pytest.mark.medium
def test_a_configure_of_another_type_into_a_tree_is_refused(project):
    project.build_dir.mkdir(parents=True)
    (project.build_dir / "CMakeCache.txt").write_text(
        "CMAKE_BUILD_TYPE:STRING=Release\n", encoding="utf-8")

    with pytest.raises(ConfigError, match="Release tree"):
        project_svc.build(None, workspace=project.workspace)

    assert project.commands == []


@pytest.mark.medium
def test_a_define_of_the_build_type_is_refused(project):
    with pytest.raises(ConfigError, match="CMAKE_BUILD_TYPE"):
        project_svc.build(None, defines=["CMAKE_BUILD_TYPE=Release"], workspace=project.workspace)

    assert project.commands == []


@pytest.mark.medium
def test_a_stopped_gate_refuses_the_build_before_any_command(project):
    stop = build_admission.stop_file(project.build_dir)
    stop.parent.mkdir(parents=True)
    stop.write_text("", encoding="utf-8")

    with pytest.raises(BuildStoppedError):
        project_svc.build(None, workspace=project.workspace)

    assert project.commands == []


@pytest.mark.medium
def test_the_configure_carries_both_launcher_entries(project):
    assert project_svc.build(None, workspace=project.workspace) == 0

    configure = _configure_of(project)
    expected = build_admission.launcher_defines(project.build_dir, "")
    assert len(expected) == 2
    for entry in expected:
        assert configure[configure.index(entry) - 1] == "-D"


@pytest.mark.medium
def test_a_define_of_the_user_comes_after_the_gate_entries(project):
    assert project_svc.build(None, defines=["FOO=1"], workspace=project.workspace) == 0

    values = _define_values(_configure_of(project))
    assert values.index("FOO=1") > max(
        values.index(entry) for entry in build_admission.launcher_defines(project.build_dir, ""))


@pytest.mark.medium
@pytest.mark.parametrize(("jobs", "parallel"), [(None, "13"), (2, "2")])
def test_the_compile_takes_the_jobs_the_user_gave_else_the_hardware_threads(
        project, jobs, parallel):
    """The compile gets --parallel from -j, or from the hardware threads without it."""
    project.configure_tree()
    workspace = replace(project.workspace, threads=lambda: 13)

    assert project_svc.build(None, jobs=jobs, workspace=workspace) == 0

    compile_ = project.commands[-1]
    assert compile_[compile_.index("--parallel") + 1] == parallel


@pytest.mark.medium
@pytest.mark.parametrize("report", [True, False])
def test_a_report_is_printed_and_saved_only_with_the_flag(project, report):
    """--report prints the report path and saves the file in the tree; without it nothing is."""
    project.configure_tree()

    assert project_svc.build(None, report=report, workspace=project.workspace) == 0

    lines = [text for text in project.console.texts("info") if text.startswith("Report: ")]
    assert len(lines) == int(report)
    assert all(Path(line.removeprefix("Report: ")).is_file() for line in lines)


@pytest.mark.medium
def test_clean_removes_the_tree_of_the_type(project):
    """A clean of a type removes that tree and leaves the others."""
    debug = project.configure_tree("Debug")
    release = project.configure_tree("Release")

    assert project_svc.clean(BuildType.release, workspace=project.workspace) == 0

    assert not release.exists() and debug.exists()


@pytest.mark.medium
def test_clean_removes_the_tree_of_the_sanitizer(project):
    """A clean of a sanitizer removes its tree and leaves the default one."""
    debug = project.configure_tree("Debug")
    address = project.configure_tree("Debug", name="asan")

    assert project_svc.clean(None, Sanitizer.address, workspace=project.workspace) == 0

    assert not address.exists() and debug.exists()


@pytest.mark.medium
def test_clean_without_a_type_removes_the_default_debug_tree(project):
    """A clean with no argument removes build/default-debug."""
    debug = project.configure_tree("Debug")

    assert project_svc.clean(workspace=project.workspace) == 0

    assert not debug.exists()


@pytest.mark.medium
def test_clean_all_removes_the_whole_build_folder(project):
    """Clean --all removes build/ with every tree in it."""
    project.configure_tree("Debug")
    project.configure_tree("Release")

    assert project_svc.clean(everything=True, workspace=project.workspace) == 0

    assert not (project.root / "build").exists()
