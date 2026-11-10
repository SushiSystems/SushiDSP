# test_help_screen.py
# SushiDSP - https://github.com/SushiSystems/SushiDSP
# Copyright (c) 2026-present Mustafa Garip & Sushi Systems
# Licensed under the Apache License 2.0. See LICENSE.
"""sd's help screen groups its commands under three headings and carries examples."""

from __future__ import annotations

import re

import pytest
from typer.testing import CliRunner

from sushidsp.cli import app


pytestmark = pytest.mark.medium

K_GROUPS = {
    "Build": [
        "configure", "build", "test", "bench", "run", "clean", "check", "docs", "backlog",
        "design",
    ],
    "Environment": ["config", "env", "setup", "doctor", "link", "unlink"],
    "Application": ["host"],
}
K_ANSI_ESCAPE = re.compile(r"\x1b\[[0-9;]*m")


def _run(*args: str) -> str:
    """Return the output of one sd invocation without colour codes, asserting it exited zero."""
    result = CliRunner().invoke(app, list(args))
    assert result.exit_code == 0, result.output
    return K_ANSI_ESCAPE.sub("", result.output)


def _commands_by_heading(page: str) -> dict[str, list[str]]:
    """Return the first word of each row, keyed by the heading above it.

    A row starts two columns in; a wrapped description sits deeper and is skipped.
    """
    grouped: dict[str, list[str]] = {}
    heading = ""
    for line in K_ANSI_ESCAPE.sub("", page).splitlines():
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
def test_a_group_lists_exactly_its_commands(heading):
    grouped = _commands_by_heading(_run("--help"))

    assert set(grouped[heading]) == set(K_GROUPS[heading])


@pytest.mark.parametrize("command", ["build", "host", "run"])
def test_a_command_help_carries_examples(command):
    out = _run(command, "--help")
    assert "Examples" in out and f"sd {command}" in out


@pytest.mark.parametrize("command", ["run", "host", "clean", "bench", "build"])
def test_a_command_that_finds_a_tree_offers_its_type(command):
    assert "--type" in _run(command, "--help")


def test_the_host_help_offers_no_run():
    assert "--no-run" in _run("host", "--help")


@pytest.mark.parametrize("option", ["--report", "--jobs"])
def test_the_build_help_offers_the_option(option):
    assert option in _run("build", "--help")


def test_the_bench_help_names_release_as_its_default():
    assert "Default: release" in " ".join(_run("bench", "--help").split())
