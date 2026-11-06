# test_aliases.py
# SushiDSP - https://github.com/SushiSystems/SushiDSP
# Copyright (c) 2026-present Mustafa Garip & Sushi Systems
# Licensed under the Apache License 2.0. See LICENSE.
"""The old spellings sd still answers to: they run, stay hidden and carry one date."""

from __future__ import annotations

import contextlib
import dataclasses
import io
import json
from datetime import date
from types import SimpleNamespace

import pytest
import typer
from sushicore.aliases import AliasTable
from typer.testing import CliRunner

from sushidsp import aliases, cli
from sushidsp.cli import app


pytestmark = pytest.mark.medium


def test_doxygen_runs_the_api_build_and_exits_with_its_code(project):
    """`sd doxygen` builds the reference through the project service and exits with its code."""
    doxyfile = project.root / ".config" / "doxygen" / "Doxyfile"
    doxyfile.parent.mkdir(parents=True)
    doxyfile.write_text("", encoding="utf-8")
    doxygen = project.root / "doxygen"
    doxygen.write_text("", encoding="utf-8")
    project.workspace.config.doxygen_exe = str(doxygen)
    project.runner.script(5)

    assert CliRunner().invoke(app, ["doxygen"], obj=project.workspace).exit_code == 5
    assert [command[0] for command in project.commands] == [str(doxygen)]


def test_the_old_spelling_is_absent_from_help_and_describe():
    """Neither the help screen nor the catalogue names `doxygen`."""
    runner = CliRunner()
    page = runner.invoke(app, ["--help"]).output
    names = [command["name"]
             for command in json.loads(runner.invoke(app, ["--describe"]).output)["commands"]]

    assert "docs" in page.split() and "doxygen" not in page.split()
    assert "docs" in names and "doxygen" not in names


def test_every_row_leaves_on_the_shared_day():
    """Each row carries 2027-03-25, the day the old spellings leave on every CLI."""
    assert [row.old for row in aliases.K_ROWS] == ["sd doxygen", "sd run"]
    for row in aliases.K_ROWS:
        assert aliases.ALIASES.alias_for(row.old) is row
        assert row.removed_after == date(2027, 3, 25)


class _Terminal(io.StringIO):
    """Stands in for a terminal's stderr."""

    def isatty(self) -> bool:
        """Reports a terminal."""
        return True


@pytest.fixture
def terminal(monkeypatch) -> _Terminal:
    """Returns a terminal for `sd run` with the notice switch unset."""
    monkeypatch.delenv(aliases.ALIASES.notice_switch, raising=False)
    return _Terminal()


def _run_command(stderr: _Terminal, workspace, target: str | None = None,
                 sort: bool = False) -> None:
    """Calls the `sd run` callback as Typer would, with *stderr* in place of CliRunner's."""
    with contextlib.redirect_stderr(stderr), pytest.raises(typer.Exit):
        cli.run(SimpleNamespace(args=[], obj=workspace), target=target, sort=sort, type=None)


def _own_table(project):
    """Returns the project's workspace over a fresh table, so no notice is old to it."""
    return dataclasses.replace(project.workspace, aliases=AliasTable("sd", aliases.K_ROWS))


def test_bare_run_announces_host_once_to_a_terminal(terminal, project):
    """`sd run` with no target names `sd host` and the last day, once per table."""
    workspace = _own_table(project)
    _run_command(terminal, workspace)
    _run_command(terminal, workspace)

    (line,) = terminal.getvalue().splitlines()
    assert "`sd run` is now `sd host`" in line and "2027-03-25" in line


def test_run_with_a_target_or_a_pick_announces_nothing(terminal, project):
    """The generic runner is not an old spelling, so it prints no notice."""
    workspace = _own_table(project)
    _run_command(terminal, workspace, target="sushidsp_tests")
    _run_command(terminal, workspace, sort=True)

    assert terminal.getvalue() == ""


def test_no_row_has_expired_today():
    """A row past its date is a spelling that should have been deleted by now."""
    assert aliases.ALIASES.expired(date.today()) == []
