# test_test_sizes.py
# SushiDSP - https://github.com/SushiSystems/SushiDSP
# Copyright (c) 2026-present Mustafa Garip & Sushi Systems
# Licensed under the Apache License 2.0. See LICENSE.
"""Pins that `sd test` selects tests by size and `sd bench` runs the benchmark program."""

from __future__ import annotations

import pytest
from typer.testing import CliRunner

from sushidsp.cli import app
from sushidsp.services.project import Sanitizer, sanitizer_defines


@pytest.mark.small
def test_a_sanitizer_becomes_its_cache_variable():
    assert sanitizer_defines(Sanitizer.address) == ["SUSHIDSP_SANITIZE=address"]
    assert sanitizer_defines(None) == []


@pytest.mark.medium
def test_the_suite_option_is_gone():
    result = CliRunner().invoke(app, ["test", "--suite", "unit"])
    assert result.exit_code != 0


@pytest.mark.medium
def test_test_command_passes_each_label(project):
    project.configure_tree("Debug")

    result = CliRunner().invoke(
        app, ["test", "--label", "small", "-l", "medium", "--sanitize", "address"],
        obj=project.workspace)

    assert result.exit_code == 0
    ctest = next(command for command in project.commands if "-L" in command)
    assert ctest[ctest.index("-L") + 1] == "^(small|medium)$"
    assert any("SUSHIDSP_SANITIZE=address" in part for command in project.commands
               for part in command)


@pytest.mark.medium
def test_bench_command_passes_its_groups(project):
    benchmark = project.plant_executable("sushidsp_bench", "bench", build_type="Release")

    result = CliRunner().invoke(app, ["bench", "Room", "Jcm800"], obj=project.workspace)

    assert result.exit_code == 0
    assert project.commands[-1] == [str(benchmark), "Room", "Jcm800"]
