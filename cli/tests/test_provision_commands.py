# test_provision_commands.py
# SushiDSP - https://github.com/SushiSystems/SushiDSP
# Copyright (c) 2026-present Mustafa Garip & Sushi Systems
# Licensed under the Apache License 2.0. See LICENSE.
"""Tests that sd carries the shared provision commands."""

from __future__ import annotations

from pathlib import Path

import pytest
from typer.testing import CliRunner

from sushicore.provision.config import ProvisionSettings
from sushidsp.cli import app
from sushidsp.config import Config
from sushidsp.host_memory import MemoryHost



@pytest.mark.medium
def test_setup_no_longer_takes_install():
    """Assert the old --install flag is gone in favour of --dry-run."""
    out = CliRunner().invoke(app, ["setup", "--help"]).output
    assert "--dry-run" in out
    assert "--install" not in out


@pytest.mark.medium
def test_setup_takes_the_shared_toolchain_options():
    """Assert setup offers --toolchain and --no-gpu."""
    out = CliRunner().invoke(app, ["setup", "--help"]).output
    for option in ("--toolchain", "--no-gpu"):
        assert option in out


@pytest.mark.small
def test_config_is_a_provision_config():
    """Assert sd's config carries every field provision reads."""
    assert issubclass(Config, ProvisionSettings)


def _clang(folder: Path) -> Path:
    """Returns the clang++ a bundle in *folder* holds on this platform."""
    return folder / "bin" / ("clang++.exe" if Config().is_windows else "clang++")


@pytest.mark.small
def test_a_bundled_clang_is_found_under_the_provision_root():
    """A clang++ under the provision root's toolchains is picked up."""
    exe = _clang(Path("/home") / "toolchains" / "llvm-sycl-nightly")

    assert Config().bundled_clang(MemoryHost(roots=(Path("/home"),), files=frozenset({exe})))         == str(exe)


@pytest.mark.small
def test_the_provision_root_clang_wins_over_the_workspace_one():
    """With a bundle in both places, the provision root's is the compiler."""
    home, workspace = Path("/home"), Path("/ws")
    exe = _clang(home / "toolchains" / "llvm-sycl")
    host = MemoryHost(roots=(home,), workspace=workspace,
                      files=frozenset({exe, _clang(workspace / "dependencies" / "toolchains"
                                                   / "llvm-sycl")}))

    assert Config().bundled_clang(host) == str(exe)


@pytest.mark.small
def test_a_clang_found_through_the_provision_roots_is_labelled_so():
    """The label of a found bundle names the provision root."""
    home = Path("/home")
    host = MemoryHost(roots=(home,), files=frozenset({_clang(home / "toolchains" / "llvm-sycl")}))

    assert Config().compiler_source(host).startswith("provision root")


@pytest.mark.small
def test_a_clang_of_the_workspace_is_found_through_the_provision_roots():
    """The workspace's bundle is found when no provision root holds one."""
    workspace = Path("/ws")
    exe = _clang(workspace / "dependencies" / "toolchains" / "llvm-sycl")
    host = MemoryHost(roots=(Path("/home"),), workspace=workspace, files=frozenset({exe}))

    assert Config().bundled_clang(host) == str(exe)


@pytest.mark.medium
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
