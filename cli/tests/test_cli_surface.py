# test_cli_surface.py
# SushiDSP - https://github.com/SushiSystems/SushiDSP
# Copyright (c) 2026-present Mustafa Garip & Sushi Systems
# Licensed under PolyForm Noncommercial 1.0.0. See LICENSE.
# Commercial use requires a licence from Sushi Systems.
"""The surfaces sd takes from sushicore: the entry point, the root options and the diagnostics."""

from __future__ import annotations

import importlib
import json
import re
import subprocess
from pathlib import Path
from types import SimpleNamespace

import pytest
import typer.main
from typer.testing import CliRunner

from sushicore.root_options import version_line
from sushidsp import cli
from sushidsp.cli import app
from sushidsp.services import diag as diag_svc
from sushidsp.services import project as project_svc
from sushidsp.services.project import BuildType

K_DISTRIBUTION = "sushidsp-cli"
K_SUBCOMMANDS = ["docs bundle"]
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


def test_main_reports_a_malformed_config_as_one_line_and_exit_one(tmp_path, monkeypatch, capsys):
    """A cli/config.toml that is not TOML ends in one line on stderr and exit code 1."""
    import sushidsp.console as console_module

    (tmp_path / "CMakeLists.txt").write_text("project(x)\n", encoding="utf-8")
    (tmp_path / "cli").mkdir()
    (tmp_path / "cli" / "config.toml").write_text("[tool\ncxx = ", encoding="utf-8")
    monkeypatch.chdir(tmp_path)
    monkeypatch.setattr("sys.argv", ["sd", "config"])
    importlib.reload(console_module)

    with pytest.raises(SystemExit) as caught:
        cli.main()

    importlib.reload(console_module)
    assert caught.value.code == 1
    (line,) = capsys.readouterr().err.splitlines()
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
    assert [command["name"] for command in document["commands"]] == sorted(
        _visible_commands() + K_SUBCOMMANDS)
    assert "doxygen" not in _visible_commands()
    assert {"host", "run"} <= set(_visible_commands())


def test_docs_runs_the_api_build_and_bundle_is_its_subcommand(monkeypatch, tmp_path):
    """`sd docs` still builds the reference, and `sd docs bundle` asks for a release."""
    monkeypatch.setattr(project_svc, "docs", lambda: 6)
    monkeypatch.setattr(cli, "find_project_root", lambda: tmp_path)
    runner = CliRunner()

    assert runner.invoke(app, ["docs"]).exit_code == 6
    assert runner.invoke(app, ["doxygen"]).exit_code == 6
    missing = runner.invoke(app, ["docs", "bundle"])
    assert missing.exit_code == 2
    assert "--release" in K_ANSI_ESCAPE.sub("", missing.output)


def test_the_command_set_is_the_documented_one():
    """sd answers to the fourteen commands its guide describes."""
    assert _visible_commands() == sorted([
        "configure", "build", "test", "clean", "docs", "check", "run", "host",
        "config", "env", "setup", "doctor", "link", "unlink"])


def test_config_and_env_come_from_the_diagnostics_object(monkeypatch):
    """config and env exit with what the diagnostics object returns, -a included."""
    calls: list[tuple] = []
    monkeypatch.setattr(type(diag_svc.DIAGNOSTICS), "config_show",
                        lambda self: calls.append(("config",)) or 0)
    monkeypatch.setattr(type(diag_svc.DIAGNOSTICS), "env_dump",
                        lambda self, show_all=False: calls.append(("env", show_all)) or 3)
    runner = CliRunner()

    assert runner.invoke(app, ["config"]).exit_code == 0
    assert runner.invoke(app, ["env", "-a"]).exit_code == 3
    assert calls == [("config",), ("env", True)]


def test_clean_delegates_to_the_project_service(monkeypatch):
    """sd clean exits with the service's code."""
    monkeypatch.setattr(project_svc, "clean", lambda: 4)
    assert CliRunner().invoke(app, ["clean"]).exit_code == 4


def test_test_hands_the_filter_to_the_project_service(monkeypatch):
    """sd test -f passes its regex through, and no filter when the flag is absent."""
    seen: list[str | None] = []

    def fake_test(build_type, suite, clean=False, filter=None):
        """Records the filter the command passed."""
        seen.append(filter)
        return 0

    monkeypatch.setattr(project_svc, "test", fake_test)
    runner = CliRunner()

    assert runner.invoke(app, ["test", "-f", "Unit_Dsp.*"]).exit_code == 0
    assert runner.invoke(app, ["test", "--filter", "A"]).exit_code == 0
    assert runner.invoke(app, ["test"]).exit_code == 0
    assert seen == ["Unit_Dsp.*", "A", None]


@pytest.fixture
def launches(monkeypatch) -> list[tuple]:
    """Returns the calls `sd host` and `sd run` make to a project service that starts nothing."""
    calls: list[tuple] = []

    def fake_host(build_type=None, launch=True, app_args=None):
        """Records one host call."""
        calls.append(("host", build_type, launch, app_args))
        return 6

    def fake_run(target=None, sort=False, app_args=None):
        """Records one run call."""
        calls.append(("run", target, sort, app_args))
        return 7

    monkeypatch.setattr(project_svc, "host", fake_host)
    monkeypatch.setattr(project_svc, "run", fake_run)
    return calls


def test_host_builds_and_launches_with_the_arguments_after_the_separator(launches):
    """sd host hands the type and everything after -- to the service and exits with its code."""
    result = CliRunner().invoke(app, ["host", "-t", "release", "--", "--device", "0"])

    assert result.exit_code == 6
    assert launches == [("host", BuildType.release, True, ["--device", "0"])]


def test_host_no_run_asks_the_service_not_to_launch(launches):
    """sd host --no-run builds only."""
    assert CliRunner().invoke(app, ["host", "--no-run"]).exit_code == 6
    assert launches == [("host", None, False, [])]


def test_run_with_a_target_runs_that_executable_and_forwards_arguments(launches):
    """sd run TARGET reaches the generic runner, never the host command."""
    result = CliRunner().invoke(app, ["run", "sushidsp_tests", "--", "--gtest_shuffle"])

    assert result.exit_code == 7
    assert launches == [("run", "sushidsp_tests", False, ["--gtest_shuffle"])]


def test_run_sort_asks_the_runner_for_a_pick(launches):
    """sd run --sort reaches the generic runner with no target."""
    assert CliRunner().invoke(app, ["run", "--sort"]).exit_code == 7
    assert launches == [("run", None, True, [])]


def test_bare_run_does_what_host_does(launches):
    """sd run with no target builds and launches the host GUI, with the type it was given."""
    runner = CliRunner()

    assert runner.invoke(app, ["run"]).exit_code == 6
    assert runner.invoke(app, ["run", "--type", "release"]).exit_code == 6
    assert launches == [("host", None, True, None), ("host", BuildType.release, True, None)]


def test_run_with_only_forwarded_arguments_is_the_old_host_spelling(launches):
    """sd run -- ARGS, the old way to pass arguments to the host GUI, still launches it with them."""
    result = CliRunner().invoke(app, ["run", "--type", "debug", "--", "--device", "0"])

    assert result.exit_code == 6
    assert launches == [("host", BuildType.debug, True, ["--device", "0"])]


def test_run_refuses_a_type_beside_a_target(launches):
    """--type belongs to the old bare spelling; beside a target it is a usage error."""
    assert CliRunner().invoke(app, ["run", "sushidsp_tests", "-t", "release"]).exit_code == 2
    assert launches == []


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
    said: list[str] = []
    monkeypatch.setattr(cli, "console", SimpleNamespace(success=said.append))
    result = CliRunner().invoke(cli.app, ["docs", "bundle", "--release", "1.2.3"])
    assert result.exit_code == 0, result.output
    bundle = tmp_path / "build" / "docs" / "bundle"
    assert (bundle / K_ARCHIVE).is_file()
    assert (bundle / (K_ARCHIVE + ".sha256")).is_file()
    assert len(said) == 2, said
    assert said[0].endswith(f"{K_ARCHIVE} (1 pages)")
    assert said[1].startswith("sha256 ")


def test_the_api_source_reads_the_xml_doxygen_writes_under_the_site_folder(monkeypatch, tmp_path):
    """Names `<OUTPUT_DIRECTORY>/<XML_OUTPUT>` of the Doxyfile, below the repository root."""
    monkeypatch.setattr(cli, "find_project_root", lambda: tmp_path)
    written = Path(_doxyfile_value("OUTPUT_DIRECTORY", "")) / _doxyfile_value("XML_OUTPUT", "xml")
    assert cli._api_source().xml_dir == tmp_path / written
