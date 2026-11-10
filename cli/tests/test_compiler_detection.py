# test_compiler_detection.py
# SushiDSP - https://github.com/SushiSystems/SushiDSP
# Copyright (c) 2026-present Mustafa Garip & Sushi Systems
# Licensed under the Apache License 2.0. See LICENSE.
"""Pins the compiler search order `sd` runs with no toolchain pinned, on an in-memory host.

    pinned cxx -> workspace intel/llvm bundle -> clang++ on PATH -> platform default
"""

from __future__ import annotations

from pathlib import Path

import pytest

from sushidsp.config import Config
from sushidsp.host_memory import MemoryHost

pytestmark = pytest.mark.small

K_ROOT = Path("/stack")
K_BUNDLE = K_ROOT / "dependencies" / "toolchains" / "llvm-sycl"
K_PATH_CLANG = {"clang++": "/usr/bin/clang++"}
K_BOTH_ON_PATH = {"clang++": "/usr/bin/clang++", "ninja": "/usr/bin/ninja"}


def _clang(folder: Path) -> Path:
    """Returns the clang++ a bundle in *folder* holds on this platform."""
    return folder / "bin" / ("clang++.exe" if Config().is_windows else "clang++")


def test_a_pinned_cxx_wins_over_every_other_step():
    host = MemoryHost(executables=K_PATH_CLANG, files=frozenset({_clang(K_BUNDLE)}),
                      workspace=K_ROOT)
    cfg = Config(cxx="/opt/mycc", llvm_root="/llvm")

    assert cfg.resolved_compiler(host=host) == "/opt/mycc"
    assert cfg.compiler_source(host).startswith("pinned")


def test_the_bundle_llvm_root_names_is_the_compiler():
    exe = _clang(Path("/llvm"))
    cfg = Config(llvm_root="/llvm")

    assert cfg.resolved_compiler(host=MemoryHost(files=frozenset({exe}))) == str(exe)
    assert "intel/llvm" in cfg.compiler_source(MemoryHost(files=frozenset({exe})))


def test_the_bundle_of_the_workspace_is_probed_without_llvm_root():
    exe = _clang(K_BUNDLE)
    host = MemoryHost(files=frozenset({exe}), workspace=K_ROOT)

    assert Config().resolved_compiler(host=host) == str(exe)


def test_the_bundle_of_a_provision_root_wins_over_the_workspace_one():
    home = Path("/home")
    exe = _clang(home / "toolchains" / "llvm-sycl")
    host = MemoryHost(files=frozenset({exe, _clang(K_BUNDLE)}), roots=(home,),
                      workspace=K_ROOT)
    cfg = Config()

    assert cfg.bundled_clang(host) == str(exe)
    assert cfg.compiler_source(host).startswith("provision root")


def test_the_clang_on_path_is_the_third_choice():
    cfg = Config()
    host = MemoryHost(executables=K_PATH_CLANG)

    assert cfg.resolved_compiler(host=host) == "/usr/bin/clang++"
    assert cfg.compiler_source(host).startswith("PATH")


def test_nothing_found_means_platform_default():
    cfg = Config()
    host = MemoryHost()

    assert cfg.resolved_compiler(host=host) == ""
    assert cfg.compiler_source(host).startswith("platform default")
    assert cfg.resolved_generator(host) == ""


@pytest.mark.parametrize(("executables", "generator"), [
    (K_BOTH_ON_PATH, "Ninja"),
    (K_PATH_CLANG, ""),
])
def test_a_found_compiler_takes_ninja_only_when_ninja_is_available(executables, generator):
    assert Config().resolved_generator(MemoryHost(executables=executables)) == generator


def test_a_pinned_generator_is_never_overridden():
    host = MemoryHost(executables=K_BOTH_ON_PATH)

    assert Config(generator="Unix Makefiles").resolved_generator(host) == "Unix Makefiles"
