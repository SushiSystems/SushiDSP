"""Pins that `sd bench` is registered and defaults to a release build."""

from __future__ import annotations

from typer.testing import CliRunner

from sushidsp.cli import app
from sushidsp.services import project


def test_bench_command_is_registered():
    result = CliRunner().invoke(app, ["bench", "--help"])
    assert result.exit_code == 0
    assert "timing harness" in result.output


def test_bench_defaults_to_release(monkeypatch):
    seen = {}

    def fake_bench(build_type, filters):
        seen["type"] = build_type
        seen["filters"] = filters
        return 0

    monkeypatch.setattr(project, "bench", fake_bench)
    result = CliRunner().invoke(app, ["bench", "-f", "Jcm800"])
    assert result.exit_code == 0
    assert seen["type"] == project.BuildType.release
    assert seen["filters"] == ["Jcm800"]
