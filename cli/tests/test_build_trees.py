# test_build_trees.py
# SushiDSP - https://github.com/SushiSystems/SushiDSP
# Copyright (c) 2026-present Mustafa Garip & Sushi Systems
# Licensed under PolyForm Noncommercial 1.0.0. See LICENSE.
# Commercial use requires a licence from Sushi Systems.
"""The build type a command acts on, the tree each type and sanitizer owns, and the memory gate."""

from __future__ import annotations

from pathlib import Path

import pytest
from sushicore import build_admission
from sushicore.build_admission import BuildStoppedError
from sushicore.build_tree import K_DEFAULT_BUILD_TYPE
from sushicore.errors import ConfigError

from sushidsp.services import project as project_svc
from sushidsp.services.project import BuildType, Sanitizer


pytestmark = pytest.mark.medium


def _define_values(command: list[str]) -> list[str]:
    """Returns the value of every `-D VALUE` pair of a recorded configure command."""
    return [command[index + 1] for index, word in enumerate(command) if word == "-D"]


def test_the_default_build_type_is_the_one_sushicore_names():
    assert K_DEFAULT_BUILD_TYPE == "Debug"
    assert project_svc.K_DEFAULT_TYPE is BuildType.debug


def test_a_command_without_a_type_builds_debug_whatever_tree_exists(project):
    project.configure_tree("Release")

    assert project_svc.resolve_build_type(None) is BuildType.debug
    assert project_svc.resolve_build_type(BuildType.release) is BuildType.release


def test_the_benchmark_builds_release_without_a_type():
    assert project_svc.resolve_build_type(None, project_svc.K_BENCHMARK_TYPE) is BuildType.release


@pytest.mark.parametrize(("build_type", "sanitizer", "name"), [
    (BuildType.debug, None, "default-debug"),
    (BuildType.release, None, "default-release"),
    (BuildType.relwithdebinfo, None, "default-relwithdebinfo"),
    (BuildType.debug, Sanitizer.address, "asan-debug"),
    (BuildType.relwithdebinfo, Sanitizer.thread, "tsan-relwithdebinfo"),
])
def test_each_type_and_sanitizer_owns_one_tree(tmp_path, build_type, sanitizer, name):
    assert project_svc._build_dir(tmp_path, build_type, sanitizer) == tmp_path / "build" / name


def test_a_configure_of_another_type_into_a_tree_is_refused(project):
    project.build_dir.mkdir(parents=True)
    (project.build_dir / "CMakeCache.txt").write_text(
        "CMAKE_BUILD_TYPE:STRING=Release\n", encoding="utf-8")

    with pytest.raises(ConfigError, match="Release tree"):
        project_svc.build(None)

    assert project.runner.commands == []


def test_a_define_of_the_build_type_is_refused(project):
    with pytest.raises(ConfigError, match="CMAKE_BUILD_TYPE"):
        project_svc.build(None, defines=["CMAKE_BUILD_TYPE=Release"])

    assert project.runner.commands == []


def test_a_stopped_gate_refuses_the_build_before_any_command(project):
    stop = build_admission.stop_file(project.build_dir)
    stop.parent.mkdir(parents=True)
    stop.write_text("", encoding="utf-8")

    with pytest.raises(BuildStoppedError):
        project_svc.build(None)

    assert project.runner.commands == []


def test_the_configure_carries_both_launcher_entries(project):
    assert project_svc.build(None) == 0

    configure = project.runner.commands[0]
    expected = build_admission.launcher_defines(project.build_dir, "")
    assert len(expected) == 2
    for entry in expected:
        assert configure[configure.index(entry) - 1] == "-D"


def test_a_define_of_the_user_comes_after_the_gate_entries(project):
    assert project_svc.build(None, defines=["FOO=1"]) == 0

    values = _define_values(project.runner.commands[0])
    assert values.index("FOO=1") > max(
        values.index(entry) for entry in build_admission.launcher_defines(project.build_dir, ""))


def test_the_compile_gets_the_hardware_threads_without_jobs(project, monkeypatch):
    monkeypatch.setattr(build_admission, "hardware_threads", lambda: 13)
    project.configure_tree()

    assert project_svc.build(None) == 0

    compile_ = project.runner.commands[-1]
    assert compile_[compile_.index("--parallel") + 1] == "13"


def test_the_compile_takes_the_jobs_the_user_gave(project, monkeypatch):
    monkeypatch.setattr(build_admission, "hardware_threads", lambda: 13)
    project.configure_tree()

    assert project_svc.build(None, jobs=2) == 0

    compile_ = project.runner.commands[-1]
    assert compile_[compile_.index("--parallel") + 1] == "2"


def test_a_report_is_printed_and_saved_in_the_tree(project):
    project.configure_tree()

    assert project_svc.build(None, report=True) == 0

    (line,) = [text for text in project.console.of("info") if text.startswith("Report: ")]
    assert Path(line.removeprefix("Report: ")).is_file()


def test_no_report_is_written_without_the_flag(project):
    project.configure_tree()

    assert project_svc.build(None) == 0

    assert [text for text in project.console.of("info") if text.startswith("Report: ")] == []


def test_clean_removes_only_the_tree_it_names(project):
    debug = project.configure_tree("Debug")
    release = project.configure_tree("Release")
    address = project.configure_tree("Debug", name="asan")

    assert project_svc.clean(BuildType.release) == 0
    assert not release.exists() and debug.exists() and address.exists()

    assert project_svc.clean(None, Sanitizer.address) == 0
    assert not address.exists() and debug.exists()

    assert project_svc.clean() == 0
    assert not debug.exists()


def test_clean_all_removes_every_tree_and_the_gate_state(project):
    project.configure_tree("Debug")
    project.configure_tree("Release")

    assert project_svc.clean(everything=True) == 0

    assert not (project.root / "build").exists()
