"""Pins that `sd bench` was folded into `sd test --suite benchmark`."""

from __future__ import annotations

from typer.testing import CliRunner

from sushidsp.cli import app
from sushidsp.services import project
from sushidsp.services.project import Suite, _SUITE_LABEL_REGEX


def test_bench_command_no_longer_exists():
    result = CliRunner().invoke(app, ["bench", "--help"])
    assert result.exit_code != 0


def test_benchmark_suite_selects_the_benchmark_label():
    assert _SUITE_LABEL_REGEX[Suite.benchmark] == "^benchmark$"


def test_test_command_accepts_benchmark_suite(monkeypatch):
    seen = {}

    def fake_test(build_type, suite, clean=False):
        seen["suite"] = suite
        return 0

    monkeypatch.setattr(project, "test", fake_test)
    result = CliRunner().invoke(app, ["test", "-s", "benchmark"])
    assert result.exit_code == 0
    assert seen["suite"] == Suite.benchmark
