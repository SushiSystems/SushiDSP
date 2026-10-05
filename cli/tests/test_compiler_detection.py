# test_compiler_detection.py
# SushiDSP - https://github.com/SushiSystems/SushiDSP
# Copyright (c) 2026 Sushi Systems
# Licensed under PolyForm Noncommercial 1.0.0. See LICENSE.
# Commercial use requires a licence from Sushi Systems.
"""Pins the compiler search order `sd` runs with no toolchain pinned.

    pinned cxx -> workspace intel/llvm bundle -> clang++ on PATH -> platform default
"""

from __future__ import annotations

from pathlib import Path

import pytest

from sushidsp import config as config_module
from sushidsp.config import Config


def _clang_name() -> str:
    return "clang++.exe" if Config().is_windows else "clang++"


@pytest.fixture
def no_path_clang(monkeypatch):
    monkeypatch.setattr(config_module.shutil, "which", lambda name: None)


@pytest.fixture
def no_workspace(monkeypatch):
    monkeypatch.setattr(config_module, "workspace_home", lambda root=None: None)


def test_pinned_cxx_wins(no_path_clang, tmp_path):
    cfg = Config(cxx=str(tmp_path / "mycc"), llvm_root=str(tmp_path))
    assert cfg.resolved_compiler() == str(tmp_path / "mycc")
    assert cfg.compiler_source().startswith("pinned")


def test_workspace_bundle_is_found_via_llvm_root(no_path_clang, no_workspace, tmp_path):
    exe = tmp_path / "bin" / _clang_name()
    exe.parent.mkdir()
    exe.write_text("")
    cfg = Config(llvm_root=str(tmp_path))
    assert cfg.resolved_compiler() == str(exe)
    assert "intel/llvm" in cfg.compiler_source()


def test_workspace_bundle_is_probed_without_llvm_root(no_path_clang, monkeypatch, tmp_path):
    exe = tmp_path / "dependencies" / "toolchains" / "llvm-sycl" / "bin" / _clang_name()
    exe.parent.mkdir(parents=True)
    exe.write_text("")
    monkeypatch.setattr(config_module, "workspace_home", lambda root=None: tmp_path)
    assert Config().resolved_compiler() == str(exe)


def test_path_clang_is_the_third_choice(no_workspace, monkeypatch):
    monkeypatch.setattr(config_module.shutil, "which",
                        lambda name: "/usr/bin/clang++" if name == "clang++" else None)
    cfg = Config()
    assert cfg.resolved_compiler() == "/usr/bin/clang++"
    assert cfg.compiler_source().startswith("PATH")


def test_nothing_found_means_platform_default(no_path_clang, no_workspace):
    cfg = Config()
    assert cfg.resolved_compiler() == ""
    assert cfg.compiler_source().startswith("platform default")
    assert cfg.resolved_generator() == ""


def test_found_compiler_takes_ninja_when_available(no_workspace, monkeypatch):
    def which(name):
        return {"clang++": "/usr/bin/clang++", "ninja": "/usr/bin/ninja"}.get(name)
    monkeypatch.setattr(config_module.shutil, "which", which)
    assert Config().resolved_generator() == "Ninja"


def test_found_compiler_without_ninja_leaves_generator_to_cmake(no_workspace, monkeypatch):
    monkeypatch.setattr(config_module.shutil, "which",
                        lambda name: "/usr/bin/clang++" if name == "clang++" else None)
    assert Config().resolved_generator() == ""


def test_pinned_generator_is_never_overridden(no_workspace, monkeypatch):
    monkeypatch.setattr(config_module.shutil, "which", lambda name: "/usr/bin/" + name)
    assert Config(generator="Unix Makefiles").resolved_generator() == "Unix Makefiles"
