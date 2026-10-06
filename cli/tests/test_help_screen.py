# test_help_screen.py
# SushiDSP - https://github.com/SushiSystems/SushiDSP
# Copyright (c) 2026-present Mustafa Garip & Sushi Systems
# Licensed under PolyForm Noncommercial 1.0.0. See LICENSE.
# Commercial use requires a licence from Sushi Systems.
"""sd's help screen groups its commands under three headings and carries examples."""

from __future__ import annotations

import pytest
from typer.testing import CliRunner

from sushidsp.cli import app

K_GROUPS = {
    "Build": ["configure", "build", "test", "run", "clean", "docs", "check"],
    "Environment": ["config", "env", "setup", "doctor", "link", "unlink"],
    "Application": ["host"],
}


def _run(*args: str) -> str:
    """Return the output of one sd invocation, asserting it exited zero."""
    result = CliRunner().invoke(app, list(args))
    assert result.exit_code == 0, result.output
    return result.output


def _commands_by_heading(page: str) -> dict[str, list[str]]:
    """Return the first word of each row, keyed by the heading above it.

    A row starts two columns in; a wrapped description sits deeper and is skipped.
    """
    grouped: dict[str, list[str]] = {}
    heading = ""
    for line in page.splitlines():
        if line and not line.startswith(" "):
            heading = line.rstrip()
            grouped[heading] = []
        elif line.startswith("  ") and not line.startswith("   "):
            grouped[heading].append(line.split()[0])
    return grouped


def test_root_help_lists_the_three_groups():
    lines = [line.rstrip() for line in _run("--help").splitlines()]
    positions = [lines.index(heading) for heading in K_GROUPS]
    assert positions == sorted(positions)


@pytest.mark.parametrize("heading", list(K_GROUPS))
def test_every_command_sits_under_its_own_group(heading):
    listed = _commands_by_heading(_run("--help"))[heading]
    assert [name for name in listed if name in K_GROUPS[heading]] == K_GROUPS[heading]


def test_no_command_is_listed_under_a_group_it_does_not_belong_to():
    grouped = _commands_by_heading(_run("--help"))
    for heading, names in K_GROUPS.items():
        assert set(grouped[heading]) == set(names)


@pytest.mark.parametrize("command", ["build", "host", "run"])
def test_a_command_help_carries_examples(command):
    out = _run(command, "--help")
    assert "Examples" in out and f"sd {command}" in out


def test_run_help_hides_the_type_option_it_keeps_for_the_old_spelling():
    assert "--type" not in _run("run", "--help")
    assert "--type" in _run("host", "--help") and "--no-run" in _run("host", "--help")
