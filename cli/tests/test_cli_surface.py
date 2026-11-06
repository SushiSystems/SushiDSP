# test_cli_surface.py
# SushiDSP - https://github.com/SushiSystems/SushiDSP
# Copyright (c) 2026-present Mustafa Garip & Sushi Systems
# Licensed under the Apache License 2.0. See LICENSE.
"""The surfaces sd takes from sushicore: the entry point, the root options and the diagnostics."""

from __future__ import annotations

import json
import os
import re
import subprocess
import sys
from pathlib import Path

import pytest
import typer.main
from typer.testing import CliRunner

from sushicore.root_options import version_line
from sushidsp import cli
from sushidsp.cli import app


pytestmark = pytest.mark.medium

K_DISTRIBUTION = "sushidsp-cli"
K_SUBCOMMANDS = ["docs bundle"]
K_SUSHICORE_GROUPS = ("backlog", "design")
K_ANSI_ESCAPE = re.compile(r"\x1b\[[0-9;]*m")
K_GIT = (
    "git", "-c", "user.name=tests", "-c", "user.email=tests@example.invalid",
    "-c", "commit.gpgsign=false",
)
K_PUBLISH_LIST = (
    'name = "sample"\n'
    'title = "Sample"\n'
    'summary = "A sample repository."\n'
    'sections = ["guides"]\n'
    'api = false\n'
)
K_ARCHIVE = "docs-bundle-1.2.3.tar.gz"
K_DOXYFILE = Path(__file__).resolve().parents[2] / ".config" / "doxygen" / "Doxyfile"


def _visible_commands() -> list[str]:
    """Returns the names of the commands sd lists in its help, sorted."""
    group = typer.main.get_command(app)
    return sorted(name for name, command in group.commands.items() if not command.hidden)


def test_main_reports_a_malformed_config_as_one_line_and_exit_one(tmp_path):
    """A cli/config.toml that is not TOML ends in one line on stderr and exit code 1."""
    (tmp_path / "CMakeLists.txt").write_text("project(x)\n", encoding="utf-8")
    (tmp_path / "cli").mkdir()
    (tmp_path / "cli" / "config.toml").write_text("[tool\ncxx = ", encoding="utf-8")
    script = "import sys; sys.argv = ['sd', 'config']; from sushidsp import cli; cli.main()"
    environment = {**os.environ, "PYTHONPATH": str(Path(cli.__file__).resolve().parents[1])}

    finished = subprocess.run([sys.executable, "-c", script], cwd=tmp_path, env=environment,
                              capture_output=True, text=True, check=False)

    assert finished.returncode == 1
    (line,) = finished.stderr.splitlines()
    assert line.startswith("sd: ") and "config.toml" in line


def test_version_prints_the_distribution_and_exits_zero():
    """--version prints the distribution name with its version and runs no command."""
    result = CliRunner().invoke(app, ["--version"])
    assert result.exit_code == 0
    assert result.output.strip() == version_line(K_DISTRIBUTION)


def test_describe_lists_every_visible_command_and_no_hidden_one():
    """--describe names exactly the commands the help lists, so no old spelling."""
    result = CliRunner().invoke(app, ["--describe"])
    assert result.exit_code == 0
    document = json.loads(result.output)
    assert document["program"] == "sd"
    own = [command["name"] for command in document["commands"]
           if command["name"].split()[0] not in K_SUSHICORE_GROUPS]
    assert own == sorted(
        [name for name in _visible_commands() if name not in K_SUSHICORE_GROUPS] + K_SUBCOMMANDS)
    assert "doxygen" not in _visible_commands()
    assert {"host", "run"} <= set(_visible_commands())


def test_docs_bundle_is_a_subcommand_of_docs_and_asks_for_a_release():
    """`sd docs bundle` exits 2 and names --release when it is given none."""
    missing = CliRunner().invoke(app, ["docs", "bundle"])
    assert missing.exit_code == 2
    assert "--release" in K_ANSI_ESCAPE.sub("", missing.output)


def test_the_command_set_is_the_documented_one():
    """sd answers to the fourteen commands its guide describes and sushicore's two groups."""
    assert _visible_commands() == sorted([
        "configure", "build", "test", "bench", "clean", "docs", "check", "run", "host",
        "config", "env", "setup", "doctor", "link", "unlink", *K_SUSHICORE_GROUPS])


def test_config_and_env_come_from_the_shared_declarations():
    """config and env are the commands sushicore declares once for every CLI."""
    group = typer.main.get_command(app)
    for name in ("config", "env"):
        assert group.commands[name].callback.__module__ == "sushicore.diag_commands"


def test_clean_removes_the_tree_it_names(project):
    """sd clean removes the tree of the type and the sanitizer, or build/ whole with --all."""
    default = project.configure_tree("Debug")
    tsan = project.configure_tree("Release", name="tsan")
    runner = CliRunner()

    assert runner.invoke(app, ["clean"], obj=project.workspace).exit_code == 0
    assert not default.exists() and tsan.exists()
    result = runner.invoke(app, ["clean", "-t", "release", "--sanitize", "thread"],
                           obj=project.workspace)
    assert result.exit_code == 0 and not tsan.exists()
    assert runner.invoke(app, ["clean", "--all"], obj=project.workspace).exit_code == 0
    assert not (project.root / "build").exists()


def test_build_hands_the_jobs_and_the_report_flag_to_the_project_service(project):
    """sd build -j and --report reach the service; without them it gets none and no report."""
    project.configure_tree("Debug")
    runner = CliRunner()

    assert runner.invoke(app, ["build"], obj=project.workspace).exit_code == 0
    assert project.console.texts("info") == []
    assert runner.invoke(app, ["build", "-j", "3", "--report"],
                         obj=project.workspace).exit_code == 0

    plain, reported = project.commands
    assert plain[plain.index("--parallel") + 1] != "3"
    assert reported[reported.index("--parallel") + 1] == "3"
    assert [text for text in project.console.texts("info") if text.startswith("Report: ")]


def test_test_hands_the_filter_to_the_project_service(project):
    """sd test -f passes its regex through, and no filter when the flag is absent."""
    project.configure_tree("Debug")
    runner = CliRunner()

    assert runner.invoke(app, ["test", "-f", "Jcm800.*"], obj=project.workspace).exit_code == 0
    assert runner.invoke(app, ["test", "--filter", "A"], obj=project.workspace).exit_code == 0
    assert runner.invoke(app, ["test"], obj=project.workspace).exit_code == 0

    ctests = [command for command in project.commands if "-L" in command]
    assert [command[command.index("-R") + 1] if "-R" in command else None
            for command in ctests] == ["Jcm800.*", "A", None]


def _launches(project) -> list[list[str]]:
    """Returns the commands the workspace's app runner started, in order."""
    return [command for command in project.commands
            if not {"-S", "--build", "-R", "-L"} & set(command)]


def test_host_builds_and_launches_with_the_arguments_after_the_separator(project):
    """sd host hands the type and everything after -- to the service and exits with its code."""
    project.configure_tree("Release")
    host = project.plant_host_gui(build_type="Release")

    result = CliRunner().invoke(app, ["host", "-t", "release", "--", "--device", "0"],
                                obj=project.workspace)

    assert result.exit_code == 0
    assert _launches(project) == [[str(host), "--device", "0"]]


def test_host_no_run_asks_the_service_not_to_launch(project):
    """sd host --no-run builds only."""
    project.plant_host_gui()

    assert CliRunner().invoke(app, ["host", "--no-run"], obj=project.workspace).exit_code == 0
    assert _launches(project) == []


def test_run_with_a_target_runs_that_executable_and_forwards_arguments(project):
    """sd run TARGET reaches the generic runner, never the host command."""
    exe = project.plant_executable("sushidsp_tests", "bin")

    result = CliRunner().invoke(app, ["run", "sushidsp_tests", "--", "--gtest_shuffle"],
                                obj=project.workspace)

    assert result.exit_code == 0
    assert project.commands == [[str(exe), "--gtest_shuffle"]]


def test_bare_run_does_what_host_does(project):
    """sd run with no target builds and launches the host GUI, with the type it was given."""
    project.configure_tree("Release")
    host = project.plant_host_gui(build_type="Release")

    result = CliRunner().invoke(app, ["run", "--type", "release"], obj=project.workspace)

    assert result.exit_code == 0
    assert _launches(project) == [[str(host)]]


def test_run_with_only_forwarded_arguments_is_the_old_host_spelling(project):
    """sd run -- ARGS, the old way to pass arguments to the host GUI, still launches it with them."""
    project.configure_tree("Debug")
    host = project.plant_host_gui()

    result = CliRunner().invoke(app, ["run", "--type", "debug", "--", "--device", "0"],
                                obj=project.workspace)

    assert result.exit_code == 0
    assert _launches(project) == [[str(host), "--device", "0"]]


def test_run_with_a_target_looks_in_the_tree_of_the_type(project):
    """--type beside a target picks the tree the executable is run from."""
    exe = project.plant_executable("sushidsp_tests", "bin", build_type="Release")

    result = CliRunner().invoke(app, ["run", "sushidsp_tests", "-t", "release"],
                                obj=project.workspace)

    assert result.exit_code == 0
    assert project.commands == [[str(exe)]]


def _published_checkout(root: Path) -> None:
    """Writes the CLI's root marker, a one-page docs/ tree and a git commit that holds them."""
    guides = root / "docs" / "guides"
    guides.mkdir(parents=True)
    (root / cli.PROFILE.root_marker).write_text("", encoding="utf-8")
    (root / "docs" / "publish.toml").write_text(K_PUBLISH_LIST, encoding="utf-8")
    (root / "docs" / "README.md").write_text(
        "# Sample\n\n- [Using](guides/USING.md)\n", encoding="utf-8")
    (guides / "USING.md").write_text("# Using\n\nRun it.\n", encoding="utf-8")
    for arguments in (("init", "--quiet"), ("add", "--all"),
                      ("commit", "--quiet", "--message", "docs")):
        subprocess.run([*K_GIT, *arguments], cwd=root, check=True, capture_output=True)


def _doxyfile_value(key: str, default: str) -> str:
    """Returns the value the repository's Doxyfile gives *key*, or *default* when it gives none."""
    for line in K_DOXYFILE.read_text(encoding="utf-8").splitlines():
        name, separator, value = line.partition("=")
        if separator and name.strip() == key:
            return value.strip().strip('"')
    return default


def test_docs_bundle_writes_the_archive_of_the_checkout_it_runs_in(tmp_path, monkeypatch):
    """Writes the archive and its digest file at the checkout's root and reports both lines."""
    _published_checkout(tmp_path)
    monkeypatch.chdir(tmp_path / "docs" / "guides")
    result = CliRunner().invoke(cli.app, ["docs", "bundle", "--release", "1.2.3"])
    assert result.exit_code == 0, result.output
    bundle = tmp_path / "build" / "docs" / "bundle"
    assert (bundle / K_ARCHIVE).is_file()
    assert (bundle / (K_ARCHIVE + ".sha256")).is_file()
    assert f"{K_ARCHIVE} (1 pages)" in result.output
    assert "sha256 " in result.output


def test_the_api_source_reads_the_xml_doxygen_writes_under_the_site_folder(
        tmp_path, monkeypatch):
    """Names `<OUTPUT_DIRECTORY>/<XML_OUTPUT>` of the Doxyfile, below the repository root."""
    (tmp_path / cli.PROFILE.root_marker).write_text("", encoding="utf-8")
    monkeypatch.chdir(tmp_path)
    written = Path(_doxyfile_value("OUTPUT_DIRECTORY", "")) / _doxyfile_value("XML_OUTPUT", "xml")
    assert cli._api_source().xml_dir == tmp_path / written
