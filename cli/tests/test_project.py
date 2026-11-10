# test_project.py
# SushiDSP - https://github.com/SushiSystems/SushiDSP
# Copyright (c) 2026-present Mustafa Garip & Sushi Systems
# Licensed under the Apache License 2.0. See LICENSE.
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
from sushicore.testing import TestLabel
from sushidsp.services.project import BuildType, Sanitizer


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

    assert project_svc.build(None, defines=["FOO=1"], workspace=project.workspace) == 0

    configure, compile_ = project.commands
    assert _is_configure(configure) and _is_compile(compile_)
    assert configure[configure.index("FOO=1") - 1] == "-D"


def test_configure_hands_cmake_the_sushilog_package(project):
    """SushiLog reaches cmake as its package config folder, and no checkout directory does."""
    assert project_svc.build(BuildType.release, workspace=project.workspace) == 0

    configure = project.commands[0]
    assert f"-DSushiLog_DIR={project.packages.config_dir}" in configure
    assert not [item for item in configure if item.startswith("-DSUSHILOG_DIR")]


def test_build_leaves_a_configured_tree_alone_without_a_define(project):
    """A configured tree of the requested type is compiled without a configure."""
    project.configure_tree("Debug")

    assert project_svc.build(None, workspace=project.workspace) == 0

    assert [_is_compile(command) for command in project.commands] == [True]


def test_build_configures_the_tree_of_another_type_and_leaves_the_first_alone(project):
    """Each type owns a tree: asking for release configures build/default-release."""
    debug = project.configure_tree("Debug")
    cache = (debug / "CMakeCache.txt").read_text(encoding="utf-8")

    assert project_svc.build(BuildType.release, workspace=project.workspace) == 0

    configure = project.commands[0]
    assert _is_configure(configure) and "-DCMAKE_BUILD_TYPE=Release" in configure
    assert configure[configure.index("-B") + 1] == str(project.tree_of("Release"))
    assert (debug / "CMakeCache.txt").read_text(encoding="utf-8") == cache


def test_test_passes_the_filter_as_a_ctest_name_regex(project):
    """--filter reaches ctest as -R, beside the size label and the configuration."""
    project.configure_tree("Debug")

    assert project_svc.test(None, (TestLabel.SMALL,), filter="Jcm800.*",
        workspace=project.workspace) == 0

    ctest = project.commands[-1]
    assert ctest[ctest.index("-R") + 1] == "Jcm800.*"
    assert ctest[ctest.index("-L") + 1] == "^(small)$"
    assert "--output-junit" in ctest
    assert ctest[ctest.index("-C") + 1] == "Debug"


def test_test_without_a_filter_or_a_size_runs_small_and_medium(project):
    """ctest gets no -R when --filter is absent and small and medium when no size is named."""
    project.configure_tree("Debug")

    assert project_svc.test(None, workspace=project.workspace) == 0

    ctest = project.commands[-1]
    assert "--test-dir" in ctest and "-R" not in ctest
    assert ctest[ctest.index("-L") + 1] == "^(small|medium)$"


def test_test_refuses_an_unfiltered_large_run(project):
    """A large run without --filter ends with code 2 and starts no ctest."""
    project.configure_tree("Debug")

    assert project_svc.test(None, (TestLabel.LARGE,), workspace=project.workspace) == 2

    assert not [command for command in project.commands if "--test-dir" in command]


def test_test_passes_jobs_to_ctest_as_parallel(project):
    """--jobs reaches ctest as --parallel."""
    project.configure_tree("Debug")

    assert project_svc.test(None, jobs=1, workspace=project.workspace) == 0

    ctest = project.commands[-1]
    assert ctest[ctest.index("--parallel") + 1] == "1"


def test_test_without_jobs_lets_sushicore_choose_at_least_one_parallel_test(project):
    """Without --jobs sushicore picks the count from the cores and the free memory, never zero."""
    project.configure_tree("Debug")

    assert project_svc.test(None, workspace=project.workspace) == 0

    ctest = project.commands[-1]
    assert int(ctest[ctest.index("--parallel") + 1]) >= 1


def test_a_sanitizer_reconfigures_with_its_cache_variable(project):
    """--sanitize reaches cmake as SUSHIDSP_SANITIZE before the tests run."""
    project.configure_tree("Debug")

    assert project_svc.test(None, (TestLabel.SMALL,), sanitizer=Sanitizer.address,
        workspace=project.workspace) == 0

    configure = project.commands[0]
    assert _is_configure(configure)
    assert configure[configure.index("SUSHIDSP_SANITIZE=address") - 1] == "-D"
    assert configure[configure.index("-B") + 1] == str(project.tree_of("Debug", "asan"))

def test_clean_removes_the_build_tree_with_its_read_only_files(project):
    """clean deletes build/, read-only files included, and starts no process."""
    project.configure_tree()
    locked = project.build_dir / "_deps" / "pack.idx"
    locked.parent.mkdir()
    locked.write_text("", encoding="utf-8")
    os.chmod(locked, stat.S_IREAD)

    assert project_svc.clean(workspace=project.workspace) == 0

    assert not project.build_dir.exists()
    assert project.commands == []


def test_clean_succeeds_when_there_is_no_build_tree(project):
    """clean on a checkout that was never built reports that and exits zero."""
    assert project_svc.clean(workspace=project.workspace) == 0
    assert project.console.texts("error") == []


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

    assert project_svc.clean(workspace=project.workspace) == 1

    (line,) = project.console.texts("error")
    assert str(project.build_dir) in line


def test_build_clean_stops_on_a_locked_tree(project, monkeypatch):
    """build --clean on a locked tree returns 1 and runs neither configure nor compile."""
    project.configure_tree()
    _refuse_removal(monkeypatch)

    assert project_svc.build(None, clean=True, workspace=project.workspace) == 1

    assert project.commands == []
    assert len(project.console.texts("error")) == 1


def test_host_builds_the_requested_type_then_launches_its_binary(project):
    """host builds the tree of the requested type before it starts that tree's host GUI."""
    exe = project.plant_host_gui(build_type="Release")

    assert project_svc.host(BuildType.release, app_args=["--foo"], workspace=project.workspace) == 0

    configure, compile_, launch = project.commands
    assert "-DCMAKE_BUILD_TYPE=Release" in configure and _is_compile(compile_)
    assert launch == [str(exe), "--foo"]


def test_host_without_launch_builds_and_starts_nothing(project):
    """host with launch off compiles the tree and leaves the host GUI alone."""
    project.configure_tree("Debug")
    project.plant_host_gui()

    assert project_svc.host(None, launch=False, workspace=project.workspace) == 0

    assert [_is_compile(command) for command in project.commands] == [True]


def test_host_ignores_a_binary_from_another_configuration(project):
    """A single-config tree never launches a binary that sits in a per-configuration folder."""
    project.configure_tree("Debug")
    project.plant_host_gui("Release")

    assert project_svc.host(None, workspace=project.workspace) == 1

    assert all(_is_compile(command) for command in project.commands)
    (line,) = project.console.texts("error")
    assert "sushidsp_host_gui" in line


def test_run_starts_the_executable_of_that_exact_name_without_building(project):
    """run prefers the exact name over a longer one that contains it."""
    project.configure_tree("Debug")
    project.plant_executable("sushidsp_tests_extra", "tests")
    exe = project.plant_executable("sushidsp_tests", "tests")

    assert project_svc.run("sushidsp_tests", workspace=project.workspace) == 0

    assert project.commands == [[str(exe)]]


def test_run_forwards_its_arguments_to_the_executable(project):
    """run starts the executable with the arguments it was given after the name."""
    project.configure_tree("Debug")
    exe = project.plant_executable("sushidsp_tests", "tests")

    assert project_svc.run("sushidsp_tests", app_args=["--gtest_shuffle"],
        workspace=project.workspace) == 0

    assert project.commands == [[str(exe), "--gtest_shuffle"]]


def test_run_falls_back_to_a_substring_of_the_name(project):
    """A target no executable is named after matches the first name that contains it."""
    project.configure_tree("Debug")
    exe = project.plant_executable("sushidsp_tests", "tests")

    assert project_svc.run("tests", workspace=project.workspace) == 0

    assert project.commands == [[str(exe)]]


def test_run_reports_a_target_the_tree_does_not_hold(project):
    """An unmatched target ends with one error line and starts nothing."""
    project.configure_tree("Debug")
    project.plant_executable("sushidsp_tests", "tests")

    assert project_svc.run("absent", workspace=project.workspace) == 1

    assert project.commands == []
    (line,) = project.console.texts("error")
    assert "absent" in line


def test_run_without_a_build_tree_points_at_sd_build(project):
    """run on a checkout that was never built names the command that builds it."""
    assert project_svc.run("sushidsp_tests", workspace=project.workspace) == 1

    assert project.commands == []
    (line,) = project.console.texts("error")
    assert "sd build --type debug" in line


def test_run_looks_in_the_tree_of_the_type_it_is_given(project):
    """run --type release starts the executable of build/default-release, not the debug one."""
    project.plant_executable("sushidsp_tests", "tests")
    exe = project.plant_executable("sushidsp_tests", "tests", build_type="Release")

    assert project_svc.run("sushidsp_tests", build_type=BuildType.release,
        workspace=project.workspace) == 0

    assert project.commands == [[str(exe)]]


def test_bench_builds_and_runs_the_release_tree_by_default(project):
    """A timing is taken on what ships, so bench without --type is a release build."""
    exe = project.plant_executable("sushidsp_bench", "tests", build_type="Release")

    assert project_svc.bench(None, workspace=project.workspace) == 0

    configure, compile_, launch = project.commands
    assert "-DCMAKE_BUILD_TYPE=Release" in configure and _is_compile(compile_)
    assert launch == [str(exe)]


def test_run_without_a_target_resolves_the_configured_target_bin(project):
    """With neither a target nor a pick, run starts what target_bin names."""
    project.configure_tree("Debug")
    exe = project.plant_host_gui()

    assert project_svc.run(workspace=project.workspace) == 0

    assert project.commands == [[str(exe)]]


def test_check_runs_every_checker_through_the_runner(project):
    """check starts each checker with this interpreter and forwards --report."""
    assert project_svc.check(report=True, workspace=project.workspace) == 0

    assert [Path(command[1]).name for command in project.commands] == [
        "check_docs_layout.py", "check_changelog.py", "check_docs_truth.py",
        "check_source_comments.py", "check_layering.py", "check_test_mirror.py",
        "check_test_sizes.py", "check_logging.py"]
    for command in project.commands:
        assert command[0] == sys.executable and command[2:] == ["--report"]


K_CHECKERS = (
    Path("tools", "documentation", "check_docs_layout.py"),
    Path("tools", "documentation", "check_changelog.py"),
    Path("tools", "documentation", "check_docs_truth.py"),
    Path("tools", "documentation", "check_source_comments.py"),
    Path("tools", "layering", "check_layering.py"),
    Path("tools", "testing", "check_test_mirror.py"),
    Path("tools", "testing", "check_test_sizes.py"),
    Path("tools", "logs", "check_logging.py"),
)


def _plant_checkers(project, tables: dict[str, set[str]]) -> None:
    """Writes every checker the service runs under the scratch checkout, each listing its rules."""
    for checker in K_CHECKERS:
        path = project.root / checker
        path.parent.mkdir(parents=True, exist_ok=True)
        rules = " ".join(sorted(tables.get(checker.name, set())))
        path.write_text(
            "import sys\n"
            "if sys.argv[1:] == ['--list-rules']:\n"
            f"    print({rules!r})\n",
            encoding="utf-8")


def test_check_sends_a_rule_to_the_checker_whose_table_names_it(project):
    """--rule reaches the checker that lists it and no other."""
    _plant_checkers(project, {"check_test_mirror.py": {"rule_untested_unit"}})

    assert project_svc.check(rule="rule_untested_unit", workspace=project.workspace) == 0

    assert [Path(command[1]).name for command in project.commands] == ["check_test_mirror.py"]
    assert project.commands[0][2:] == ["--rule", "rule_untested_unit"]


def test_check_fails_a_rule_no_checker_lists_and_runs_nothing(project):
    """A rule no checker has ends with code 1, one error line and no run."""
    _plant_checkers(project, {"check_test_mirror.py": {"rule_untested_unit"}})

    assert project_svc.check(rule="rule_nowhere", workspace=project.workspace) == 1

    assert project.commands == []
    assert project.console.texts("error") == ["no checker under tools/ has the rule rule_nowhere"]


def test_rules_of_reads_the_rule_table_of_a_real_checker():
    """_rules_of reads the rules a checker of this checkout lists."""
    root = Path(__file__).resolve().parents[2]
    rules = project_svc._rules_of(root / "tools" / "testing" / "check_test_mirror.py")
    assert "rule_untested_unit" in rules


def test_check_outside_a_checkout_fails_like_every_other_command(tmp_path, monkeypatch):
    """check has no fallback root: outside a checkout it ends with the shared message."""
    monkeypatch.chdir(tmp_path)

    with pytest.raises(SystemExit) as caught:
        project_svc.check()
    assert "SushiDSP" in str(caught.value.code)


def test_docs_runs_doxygen_on_the_doxyfile_and_exits_with_its_code(project):
    """docs hands the Doxyfile of the checkout to the configured doxygen and returns its code."""
    doxyfile = project.root / ".config" / "doxygen" / "Doxyfile"
    doxyfile.parent.mkdir(parents=True)
    doxyfile.write_text("", encoding="utf-8")
    doxygen = project.root / "doxygen"
    doxygen.write_text("", encoding="utf-8")
    project.workspace.config.doxygen_exe = str(doxygen)
    project.runner.script(6)

    assert project_svc.docs(workspace=project.workspace) == 6
    assert project.commands == [[str(doxygen), ".config/doxygen/Doxyfile"]]
