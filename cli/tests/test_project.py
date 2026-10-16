# test_project.py
# SushiDSP - https://github.com/SushiSystems/SushiDSP
# Copyright (c) 2026-present Mustafa Garip & Sushi Systems
# Licensed under PolyForm Noncommercial 1.0.0. See LICENSE.
# Commercial use requires a licence from Sushi Systems.
"""The command lines the project service hands cmake, ctest, the checkers and the host GUI.

Every test runs against the `project` fixture, which records argv and starts no process.
"""

from __future__ import annotations

import os
import stat
import sys
from pathlib import Path

import pytest

from sushidsp.services import project as project_svc
from sushidsp.services.project import BuildType, Sanitizer, Size


pytestmark = pytest.mark.medium


def _is_configure(command: list[str]) -> bool:
    """Tells whether a recorded command is a cmake configure."""
    return "-S" in command


def _is_compile(command: list[str]) -> bool:
    """Tells whether a recorded command is a cmake build."""
    return "--build" in command


def test_build_passes_a_define_to_an_already_configured_tree(project):
    """A -D on a configured tree reconfigures with the variable before compiling."""
    project.configure_tree("Debug")

    assert project_svc.build(None, defines=["FOO=1"]) == 0

    configure, compile_ = project.runner.commands
    assert _is_configure(configure) and _is_compile(compile_)
    assert configure[configure.index("FOO=1") - 1] == "-D"


def test_build_leaves_a_configured_tree_alone_without_a_define(project):
    """A configured tree of the requested type is compiled without a configure."""
    project.configure_tree("Debug")

    assert project_svc.build(None) == 0

    assert [_is_compile(command) for command in project.runner.commands] == [True]


def test_build_configures_a_tree_of_another_type(project):
    """Asking for a type the tree does not carry reconfigures it."""
    project.configure_tree("Debug")

    assert project_svc.build(BuildType.release) == 0

    configure = project.runner.commands[0]
    assert _is_configure(configure) and "-DCMAKE_BUILD_TYPE=Release" in configure


def test_test_passes_the_filter_as_a_ctest_name_regex(project):
    """--filter reaches ctest as -R, beside the size label and the configuration."""
    project.configure_tree("Debug")

    assert project_svc.test(None, (Size.small,), filter="Jcm800.*") == 0

    ctest = project.runner.commands[-1]
    assert ctest[ctest.index("-R") + 1] == "Jcm800.*"
    assert ctest[ctest.index("-L") + 1] == "^(small)$"
    assert ctest[ctest.index("-C") + 1] == "Debug"


def test_test_without_a_filter_or_a_size_runs_everything(project):
    """ctest gets no -R when --filter is absent, and no -L when no size is named."""
    project.configure_tree("Debug")

    assert project_svc.test(None) == 0

    ctest = project.runner.commands[-1]
    assert "--test-dir" in ctest
    assert "-R" not in ctest and "-L" not in ctest


def test_test_passes_jobs_to_ctest_as_parallel(project):
    """--jobs reaches ctest as --parallel, and no --parallel is passed without it."""
    project.configure_tree("Debug")

    assert project_svc.test(None, jobs=1) == 0
    ctest = project.runner.commands[-1]
    assert ctest[ctest.index("--parallel") + 1] == "1"

    assert project_svc.test(None) == 0
    assert "--parallel" not in project.runner.commands[-1]


def test_a_sanitizer_reconfigures_with_its_cache_variable(project):
    """--sanitize reaches cmake as SUSHIDSP_SANITIZE before the tests run."""
    project.configure_tree("Debug")

    assert project_svc.test(None, (Size.small,), sanitizer=Sanitizer.address) == 0

    configure = project.runner.commands[0]
    assert _is_configure(configure)
    assert configure[configure.index("SUSHIDSP_SANITIZE=address") - 1] == "-D"

def test_clean_removes_the_build_tree_with_its_read_only_files(project):
    """clean deletes build/, read-only files included, and starts no process."""
    project.configure_tree()
    locked = project.build_dir / "_deps" / "pack.idx"
    locked.parent.mkdir()
    locked.write_text("", encoding="utf-8")
    os.chmod(locked, stat.S_IREAD)

    assert project_svc.clean() == 0

    assert not project.build_dir.exists()
    assert project.runner.commands == []


def test_clean_succeeds_when_there_is_no_build_tree(project):
    """clean on a checkout that was never built reports that and exits zero."""
    assert project_svc.clean() == 0
    assert project.console.of("error") == []


def _refuse_removal(monkeypatch) -> None:
    """Makes every tree removal fail the way a file held open on Windows fails it."""
    def refuse(path, onerror=None):
        """Raises the error a locked file raises."""
        raise PermissionError(13, "The process cannot access the file", str(path))

    monkeypatch.setattr(project_svc.shutil, "rmtree", refuse)


def test_clean_reports_a_locked_tree_as_one_error(project, monkeypatch):
    """A file held open ends clean with one error line and exit code 1."""
    project.configure_tree()
    _refuse_removal(monkeypatch)

    assert project_svc.clean() == 1

    (line,) = project.console.of("error")
    assert str(project.build_dir) in line


def test_build_clean_stops_on_a_locked_tree(project, monkeypatch):
    """build --clean on a locked tree returns 1 and runs neither configure nor compile."""
    project.configure_tree()
    _refuse_removal(monkeypatch)

    assert project_svc.build(None, clean=True) == 1

    assert project.runner.commands == []
    assert len(project.console.of("error")) == 1


def test_host_builds_the_requested_type_then_launches_its_binary(project):
    """host brings the tree to the requested type before it starts the host GUI."""
    project.configure_tree("Debug")
    exe = project.plant_host_gui()

    assert project_svc.host(BuildType.release, app_args=["--foo"]) == 0

    configure, compile_, launch = project.runner.commands
    assert "-DCMAKE_BUILD_TYPE=Release" in configure and _is_compile(compile_)
    assert launch == [str(exe), "--foo"]


def test_host_without_launch_builds_and_starts_nothing(project):
    """host with launch off compiles the tree and leaves the host GUI alone."""
    project.configure_tree("Debug")
    project.plant_host_gui()

    assert project_svc.host(None, launch=False) == 0

    assert [_is_compile(command) for command in project.runner.commands] == [True]


def test_host_ignores_a_binary_from_another_configuration(project):
    """A single-config tree never launches a binary that sits in a per-configuration folder."""
    project.configure_tree("Debug")
    project.plant_host_gui("Release")

    assert project_svc.host(None) == 1

    assert all(_is_compile(command) for command in project.runner.commands)
    (line,) = project.console.of("error")
    assert "sushidsp_host_gui" in line


def test_run_starts_the_executable_of_that_exact_name_without_building(project):
    """run prefers the exact name over a longer one that contains it, and forwards arguments."""
    project.configure_tree("Debug")
    project.plant_executable("sushidsp_tests_extra", "tests")
    exe = project.plant_executable("sushidsp_tests", "tests")

    assert project_svc.run("sushidsp_tests", app_args=["--gtest_shuffle"]) == 0

    assert project.runner.commands == [[str(exe), "--gtest_shuffle"]]


def test_run_falls_back_to_a_substring_of_the_name(project):
    """A target no executable is named after matches the first name that contains it."""
    project.configure_tree("Debug")
    exe = project.plant_executable("sushidsp_tests", "tests")

    assert project_svc.run("tests") == 0

    assert project.runner.commands == [[str(exe)]]


def test_run_reports_a_target_the_tree_does_not_hold(project):
    """An unmatched target ends with one error line and starts nothing."""
    project.configure_tree("Debug")
    project.plant_executable("sushidsp_tests", "tests")

    assert project_svc.run("absent") == 1

    assert project.runner.commands == []
    (line,) = project.console.of("error")
    assert "absent" in line


def test_run_without_a_build_tree_points_at_sd_build(project):
    """run on a checkout that was never built names the command that builds it."""
    assert project_svc.run("sushidsp_tests") == 1

    assert project.runner.commands == []
    (line,) = project.console.of("error")
    assert "sd build" in line


def test_run_without_a_target_resolves_the_configured_target_bin(project):
    """With neither a target nor a pick, run starts what target_bin names."""
    project.configure_tree("Debug")
    exe = project.plant_host_gui()

    assert project_svc.run() == 0

    assert project.runner.commands == [[str(exe)]]


def test_check_runs_every_checker_through_the_runner(project):
    """check starts each checker with this interpreter and forwards --report."""
    assert project_svc.check(report=True) == 0

    assert [Path(command[1]).name for command in project.runner.commands] == [
        "check_docs_layout.py", "check_changelog.py", "check_docs_truth.py",
        "check_source_comments.py", "check_layering.py", "check_test_mirror.py",
        "check_test_sizes.py", "check_logging.py"]
    for command in project.runner.commands:
        assert command[0] == sys.executable and command[2:] == ["--report"]


def test_check_sends_a_rule_only_to_the_checkers_whose_table_names_it(project, monkeypatch):
    """--rule reaches the checker that has it, and a rule no checker has fails with no run."""
    tables = {"check_test_mirror.py": {"rule_untested_unit"}}
    monkeypatch.setattr(project_svc, "_rules_of",
                        lambda checker: frozenset(tables.get(checker.name, set())))

    assert project_svc.check(rule="rule_untested_unit") == 0
    assert [Path(command[1]).name for command in project.runner.commands] == ["check_test_mirror.py"]
    assert project.runner.commands[0][2:] == ["--rule", "rule_untested_unit"]

    assert project_svc.check(rule="rule_nowhere") == 1
    assert len(project.runner.commands) == 1


def test_rules_of_reads_the_rule_table_of_a_real_checker():
    """_rules_of reads the rules a checker of this checkout lists."""
    root = Path(__file__).resolve().parents[2]
    rules = project_svc._rules_of(root / "tools" / "testing" / "check_test_mirror.py")
    assert "rule_untested_unit" in rules


def test_check_outside_a_checkout_fails_like_every_other_command(monkeypatch):
    """check has no fallback root: outside a checkout it ends with the shared message."""
    def outside():
        """Raises what find_project_root raises outside a checkout."""
        raise SystemExit("Not inside a SushiDSP project")

    monkeypatch.setattr(project_svc, "find_project_root", outside)

    with pytest.raises(SystemExit) as caught:
        project_svc.check()
    assert caught.value.code == "Not inside a SushiDSP project"
