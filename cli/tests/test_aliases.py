# test_aliases.py
# SushiDSP - https://github.com/SushiSystems/SushiDSP
# Copyright (c) 2026-present Mustafa Garip & Sushi Systems
# Licensed under PolyForm Noncommercial 1.0.0. See LICENSE.
# Commercial use requires a licence from Sushi Systems.
"""The old spellings sd still answers to: they run, stay hidden and carry one date."""

from __future__ import annotations

import contextlib
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
from sushidsp.services import project as project_svc


pytestmark = pytest.mark.medium


def test_doxygen_still_runs_the_docs_command(monkeypatch, tmp_path):
    """`sd doxygen` runs what `sd docs` runs and exits with its code."""
    monkeypatch.setattr(project_svc, "docs", lambda: 5)
    monkeypatch.setattr(cli, "find_project_root", lambda: tmp_path)
    runner = CliRunner()

    assert runner.invoke(app, ["docs"]).exit_code == 5
    assert runner.invoke(app, ["doxygen"]).exit_code == 5


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
    """Returns a terminal for `sd run`, over a fresh table and a service that starts nothing."""
    stream = _Terminal()
    monkeypatch.delenv(aliases.ALIASES.notice_switch, raising=False)
    monkeypatch.setattr(cli, "ALIASES", AliasTable("sd", aliases.K_ROWS))
    monkeypatch.setattr(project_svc, "host", lambda build_type=None, app_args=None: 0)
    monkeypatch.setattr(project_svc, "run", lambda target=None, sort=False, app_args=None,
                        build_type=None: 0)
    return stream


def _run_command(stderr: _Terminal, target: str | None = None, sort: bool = False) -> None:
    """Calls the `sd run` callback as Typer would, with *stderr* in place of CliRunner's."""
    with contextlib.redirect_stderr(stderr), pytest.raises(typer.Exit):
        cli.run(SimpleNamespace(args=[]), target=target, sort=sort, type=None)


def test_bare_run_announces_host_once_to_a_terminal(terminal):
    """`sd run` with no target names `sd host` and the last day, once per process."""
    _run_command(terminal)
    _run_command(terminal)

    (line,) = terminal.getvalue().splitlines()
    assert "`sd run` is now `sd host`" in line and "2027-03-25" in line


def test_run_with_a_target_or_a_pick_announces_nothing(terminal):
    """The generic runner is not an old spelling, so it prints no notice."""
    _run_command(terminal, target="sushidsp_tests")
    _run_command(terminal, sort=True)

    assert terminal.getvalue() == ""


def test_no_row_has_expired_today():
    """A row past its date is a spelling that should have been deleted by now."""
    assert aliases.ALIASES.expired(date.today()) == []
