# test_aliases.py
# SushiDSP - https://github.com/SushiSystems/SushiDSP
# Copyright (c) 2026-present Mustafa Garip & Sushi Systems
# Licensed under the Apache License 2.0. See LICENSE.
"""The old spellings sd still answers to: they run, stay hidden and carry one date."""

from __future__ import annotations

import contextlib
import dataclasses
import io
from datetime import date
from types import SimpleNamespace

import pytest
import typer
from sushicore.aliases import AliasTable
from typer.testing import CliRunner

from sushidsp import aliases, cli
from sushidsp.cli import app


@pytest.mark.medium
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


@pytest.mark.medium
def test_the_old_spelling_is_absent_from_the_help_screen():
    """The help screen names `docs` and not `doxygen`."""
    page = CliRunner().invoke(app, ["--help"]).output

    assert "docs" in page.split() and "doxygen" not in page.split()


@pytest.mark.small
def test_the_rows_are_the_two_old_spellings():
    """The table holds `sd doxygen` and `sd run`, and nothing else."""
    assert [row.old for row in aliases.K_ROWS] == ["sd doxygen", "sd run"]


@pytest.mark.small
@pytest.mark.parametrize("old", ["sd doxygen", "sd run"])
def test_a_row_leaves_on_the_shared_day(old):
    """The row carries 2027-03-25, the day the old spellings leave on every CLI."""
    assert aliases.ALIASES.alias_for(old).removed_after == date(2027, 3, 25)


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


@pytest.mark.medium
@pytest.mark.parametrize(("target", "sort", "notices"), [
    (None, False, 1),
    ("sushidsp_tests", False, 0),
    (None, True, 0),
])
def test_only_a_bare_run_announces_host_once_to_a_terminal(terminal, project, target, sort,
                                                           notices):
    """`sd run` with no target names `sd host` and the last day, once per table; a target or a pick, nothing."""
    workspace = _own_table(project)
    _run_command(terminal, workspace, target=target, sort=sort)
    _run_command(terminal, workspace, target=target, sort=sort)

    lines = terminal.getvalue().splitlines()
    assert len(lines) == notices
    assert all("`sd run` is now `sd host`" in line and "2027-03-25" in line for line in lines)


@pytest.mark.small
@pytest.mark.parametrize(("today", "expired"), [
    (date(2027, 3, 25), []),
    (date(2027, 3, 26), ["sd doxygen", "sd run"]),
])
def test_a_row_expires_the_day_after_its_last_day(today, expired):
    """No row is expired on its last day, and every row is the day after."""
    assert [row.old for row in aliases.ALIASES.expired(today)] == expired


@pytest.mark.medium
def test_no_row_has_expired_today():
    """A row is removed by its last day, so none has outlived it on the real clock."""
    assert [row.old for row in aliases.ALIASES.expired(date.today())] == []
