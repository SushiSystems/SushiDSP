# Copyright (c) 2026-present Mustafa Garip & Sushi Systems
# Licensed under the Apache License, Version 2.0. See LICENSE.
"""Tests that sd carries the shared provision commands."""

from __future__ import annotations

from typer.testing import CliRunner

from sushicore.provision.config import ProvisionSettings
from sushidsp.cli import app
from sushidsp.config import Config


def test_sd_registers_the_four_provision_commands():
    """Assert setup, doctor, link and unlink appear in sd's help."""
    out = CliRunner().invoke(app, ["--help"]).output
    for name in ("setup", "doctor", "link", "unlink"):
        assert name in out


def test_setup_no_longer_takes_install():
    """Assert the old --install flag is gone in favour of --dry-run."""
    out = CliRunner().invoke(app, ["setup", "--help"]).output
    assert "--dry-run" in out
    assert "--install" not in out


def test_config_is_a_provision_config():
    """Assert sd's config carries every field provision reads."""
    assert issubclass(Config, ProvisionSettings)


def test_bundled_clang_is_found_under_the_provision_root(tmp_path, monkeypatch):
    """Assert a clang++ under the provision root's toolchains is picked up."""
    monkeypatch.setenv("SUSHISYSTEMS_HOME", str(tmp_path))
    exe_name = Config()._clang_name()
    exe = tmp_path / "toolchains" / "llvm-sycl-nightly" / "bin" / exe_name
    exe.parent.mkdir(parents=True)
    exe.write_text("")
    assert Config().bundled_clang() == str(exe)
