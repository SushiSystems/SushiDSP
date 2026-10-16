# test_test_sizes.py
# SushiDSP - https://github.com/SushiSystems/SushiDSP
# Copyright (c) 2026-present Mustafa Garip & Sushi Systems
# Licensed under PolyForm Noncommercial 1.0.0. See LICENSE.
# Commercial use requires a licence from Sushi Systems.
"""Pins that `sd test` selects tests by size and `sd bench` runs the benchmark program."""

from __future__ import annotations

import pytest
from typer.testing import CliRunner

from sushidsp.cli import app
from sushidsp.services import project
from sushidsp.services.project import Sanitizer, Size, sanitizer_defines, size_label_regex


pytestmark = pytest.mark.medium


def test_no_size_selects_every_test():
    assert size_label_regex(()) is None


def test_sizes_become_one_anchored_label_regex():
    assert size_label_regex((Size.medium, Size.small)) == "^(medium|small)$"


def test_a_sanitizer_becomes_its_cache_variable():
    assert sanitizer_defines(Sanitizer.address) == ["SUSHIDSP_SANITIZE=address"]
    assert sanitizer_defines(None) == []


def test_the_suite_option_is_gone():
    result = CliRunner().invoke(app, ["test", "--suite", "unit"])
    assert result.exit_code != 0


def test_test_command_passes_each_label(monkeypatch):
    seen = {}

    def fake_test(build_type, sizes, clean=False, filter=None, sanitizer=None, jobs=None):
        seen["sizes"] = list(sizes)
        seen["sanitizer"] = sanitizer
        return 0

    monkeypatch.setattr(project, "test", fake_test)
    result = CliRunner().invoke(
        app, ["test", "--label", "small", "-l", "medium", "--sanitize", "address"])
    assert result.exit_code == 0
    assert seen == {"sizes": [Size.small, Size.medium], "sanitizer": Sanitizer.address}


def test_bench_command_passes_its_groups(monkeypatch):
    seen = {}

    def fake_bench(build_type, groups, clean=False):
        seen["groups"] = list(groups)
        return 0

    monkeypatch.setattr(project, "bench", fake_bench)
    result = CliRunner().invoke(app, ["bench", "Room", "Jcm800"])
    assert result.exit_code == 0
    assert seen["groups"] == ["Room", "Jcm800"]
