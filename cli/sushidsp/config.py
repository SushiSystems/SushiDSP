# Copyright (c) 2026-present Mustafa Garip & Sushi Systems
# Licensed under the Apache License, Version 2.0. See LICENSE.
"""Layered configuration loading for the SushiDSP CLI.

Precedence (low to high):

    built-in defaults -> config.toml -> workspace config.local.toml -> repo
    config.local.toml -> SD_* env vars

SushiDSP pins no toolchain. It builds with whatever C++17 compiler it can
find, in this order, and `sd config` reports which one won:

    1. an explicit ``cxx`` (config.local.toml or SD_CXX);
    2. the intel/llvm clang++ that ``hub install`` provisions into the
       SushiStack workspace (``llvm_root``, written by ``hub`` into the
       workspace-shared config.local.toml);
    3. a ``clang++`` on PATH;
    4. nothing -- CMake's platform default (MSVC through the Visual Studio
       generator on Windows, the system compiler elsewhere).

Step 4 is why a standalone checkout keeps building with no SushiStack at all;
the workspace only upgrades the compiler when it is there to be found.
"""

from __future__ import annotations

import shutil
from dataclasses import dataclass
from pathlib import Path

from sushicore.config_base import ToolConfig
from sushicore.module_config import ModuleConfig
from sushicore.profile import ModuleProfile

#: How SushiDSP describes itself to the shared machinery.
PROFILE = ModuleProfile(
    name="SushiDSP",
    program="sd",
    env_prefix="SD",
    root_marker="CMakeLists.txt",
    default_target="sushidsp_host",
    # A deliberate subset: the knobs SushiDSP actually has. No vcpkg, no
    # sibling checkouts.
    env_fields=("cxx", "generator", "cmake_exe", "ctest_exe", "ninja_exe",
                "vs_vcvars", "llvm_root", "target_bin"),
)

# The workspace-shared config.local.toml is where `hub install` records the
# bundled intel/llvm root, vcvars, ninja and cmake it found; reading it is how
# step 2 of the compiler search works without SushiDSP pinning anything itself.
_MODULE = ModuleConfig(PROFILE, use_workspace_config=True)

find_project_root = _MODULE.find_project_root
config_dir = _MODULE.config_dir
workspace_home = _MODULE.workspace_home

#: Field -> environment variable, derived from the profile's prefix. Exposed as
#: a module attribute because `sd config` reports which values came from the
#: environment and needs the reverse map.
_ENV_OVERRIDES = PROFILE.env_overrides()

#: Directory names `hub install` has unpacked the intel/llvm bundle under, in
#: preference order, probed when llvm_root is not written down.
_INTEL_LLVM_BUNDLE_DIRS = ("llvm-sycl-nightly", "llvm-sycl")


@dataclass
class Config(ToolConfig):
    """Resolved, platform-specific tool configuration for the SushiDSP build.

    Inherits every field (cmake/ninja paths, cxx, generator, vs_vcvars, ...) from
    :class:`ToolConfig` and adds the intel/llvm bundle root ``hub`` records.
    """

    target_bin: str = "sushidsp_host"

    # Left unset so that, with no compiler found, CMake picks the platform default
    # (Visual Studio's multi-config generator on Windows). When a compiler is
    # found the generator is chosen by resolved_generator() instead.
    generator: str = ""

    # intel/llvm bundle root holding bin/clang++, as written by `hub install`.
    llvm_root: str = ""

    def _clang_name(self) -> str:
        return "clang++.exe" if self.is_windows else "clang++"

    def bundled_clang(self) -> str:
        """The workspace's intel/llvm clang++, or '' when none is installed."""
        candidates: list[Path] = []
        if self.llvm_root:
            candidates.append(Path(self.expand(self.llvm_root)))
        home = workspace_home()
        if home:
            candidates += [home / "dependencies" / "toolchains" / name
                           for name in _INTEL_LLVM_BUNDLE_DIRS]
        for root in candidates:
            exe = root / "bin" / self._clang_name()
            if exe.is_file():
                return str(exe)
        return ""

    def resolved_compiler(self, root: Path | None = None) -> str:
        """The C++ compiler to drive the build, or '' for CMake's platform default.

        @param root Unused; accepted so the shared `config` diagnostics, which
            call ``resolved_compiler(root)`` on every config that has one, work.
        """
        if self.cxx:
            return self.expand(self.cxx)
        bundled = self.bundled_clang()
        if bundled:
            return bundled
        return shutil.which("clang++") or ""

    def compiler_source(self) -> str:
        """Which step of the search produced resolved_compiler(), for diagnostics."""
        if self.cxx:
            return "pinned (cxx)"
        if self.bundled_clang():
            return "SushiStack workspace (intel/llvm)"
        if shutil.which("clang++"):
            return "PATH (clang++)"
        return "platform default (no clang++ found; `hub install` provisions intel/llvm)"

    def resolved_ninja(self) -> str:
        """The ninja binary to hand CMake, or '' when none is available."""
        if self.ninja_exe:
            return self.expand(self.ninja_exe)
        return shutil.which("ninja") or ""

    def resolved_generator(self) -> str:
        """The CMake generator: pinned, else Ninja with a found compiler, else CMake's default.

        A found clang++ must not be paired with the Visual Studio generator (which
        would silently build with MSVC anyway), so it takes Ninja when one is
        available; with no ninja the compiler is still passed and CMake's
        single-config Makefile/NMake default carries it.
        """
        if self.generator:
            return self.generator
        if self.resolved_compiler() and self.resolved_ninja():
            return "Ninja"
        return ""

    def is_multi_config(self) -> bool:
        """Whether the generator in use bakes the configuration at build time
        (Visual Studio, Xcode) rather than at configure time (Ninja, Makefiles)."""
        generator = self.resolved_generator()
        return generator == "" and self.is_windows or generator.startswith(("Visual Studio", "Xcode"))


def load_config() -> Config:
    """Load and resolve the layered configuration for the current platform."""
    return _MODULE.load(Config)
