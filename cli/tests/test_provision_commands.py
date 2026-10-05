# test_provision_commands.py
# SushiDSP - https://github.com/SushiSystems/SushiDSP
# Copyright (c) 2026 Sushi Systems
# Licensed under PolyForm Noncommercial 1.0.0. See LICENSE.
# Commercial use requires a licence from Sushi Systems.
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


def test_setup_takes_the_shared_toolchain_options():
    """Assert setup offers --toolchain and --no-gpu."""
    out = CliRunner().invoke(app, ["setup", "--help"]).output
    for option in ("--toolchain", "--no-gpu"):
        assert option in out


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


def _plant_clang(root, exe_name):
    """Create an empty clang++ under ``root``/bin and return its path."""
    exe = root / "bin" / exe_name
    exe.parent.mkdir(parents=True)
    exe.write_text("")
    return exe


def test_provision_root_clang_wins_over_the_workspace_one(tmp_path, monkeypatch):
    home = tmp_path / "home"
    workspace = tmp_path / "ws"
    monkeypatch.setenv("SUSHISYSTEMS_HOME", str(home))
    monkeypatch.setattr("sushidsp.config.workspace_home", lambda: workspace)
    cfg = Config()
    name = cfg._clang_name()
    _plant_clang(workspace / "dependencies" / "toolchains" / "llvm-sycl", name)
    exe = _plant_clang(home / "toolchains" / "llvm-sycl", name)
    assert cfg.bundled_clang() == str(exe)
    assert cfg.compiler_source().startswith("provision root")


def test_workspace_clang_is_labelled_as_the_workspace(tmp_path, monkeypatch):
    workspace = tmp_path / "ws"
    monkeypatch.setenv("SUSHISYSTEMS_HOME", str(tmp_path / "home"))
    monkeypatch.setattr("sushidsp.config.workspace_home", lambda: workspace)
    cfg = Config()
    _plant_clang(workspace / "dependencies" / "toolchains" / "llvm-sycl", cfg._clang_name())
    assert cfg.compiler_source() == "SushiStack workspace (intel/llvm)"


def test_setup_dry_run_in_a_workspace_never_calls_hub(tmp_path, monkeypatch):
    import subprocess

    workspace = tmp_path / "ws"
    (workspace / ".sushistack").mkdir(parents=True)
    module = workspace / "sushidsp"
    module.mkdir()
    (module / "CMakeLists.txt").write_text("")
    monkeypatch.setenv("SUSHISTACK_HOME", str(workspace))
    monkeypatch.setenv("SUSHISYSTEMS_HOME", str(tmp_path / "home"))
    monkeypatch.chdir(module)

    def _is_hub(argv):
        first = argv if isinstance(argv, str) else (list(argv) or [""])[0]
        return str(first).replace("\\", "/").rsplit("/", 1)[-1].split(".")[0] == "hub"

    real_popen = subprocess.Popen

    class _GuardedPopen(real_popen):
        def __init__(self, args, *rest, **kwargs):
            assert not _is_hub(args), f"hub was invoked: {args}"
            super().__init__(args, *rest, **kwargs)

    monkeypatch.setattr(subprocess, "Popen", _GuardedPopen)
    result = CliRunner().invoke(app, ["setup", "--dry-run"])
    assert result.exception is None or isinstance(result.exception, SystemExit), result.output
    for step in ("setup: detect", "setup: install-deps", "setup: configure"):
        assert step in result.output
    assert "hub install" not in result.output
