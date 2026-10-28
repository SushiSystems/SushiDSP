# config.py
# SushiDSP - https://github.com/SushiSystems/SushiDSP
# Copyright (c) 2026-present Mustafa Garip & Sushi Systems
# Licensed under PolyForm Noncommercial 1.0.0. See LICENSE.
# Commercial use requires a licence from Sushi Systems.
"""Loads the layered configuration of the SushiDSP CLI and finds the compiler it builds with.

The layer precedence and the compiler search order are in cli/README.md, "config.py".
"""

from __future__ import annotations

import shutil
from dataclasses import dataclass
from pathlib import Path

from sushicore.module_config import ModuleConfig
from sushicore.profile import ModuleProfile
from sushicore.provision import home as provision_home
from sushicore.stack_config import StackConfig

#: How SushiDSP describes itself to the shared machinery.
PROFILE = ModuleProfile(
    name="SushiDSP",
    program="sd",
    env_prefix="SD",
    root_marker="CMakeLists.txt",
    default_target="sushidsp_host",
    # Every name here has a suffix in sushicore's table, so each one is a real SD_* variable.
    env_fields=("cxx", "generator", "cmake_exe", "ctest_exe", "ninja_exe",
                "vs_vcvars", "ccache_exe", "target_bin"),
)

# Layers the workspace-shared config.local.toml, which holds the intel/llvm root from the workspace.
_MODULE = ModuleConfig(PROFILE, use_workspace_config=True)

find_project_root = _MODULE.find_project_root
config_dir = _MODULE.config_dir
workspace_home = _MODULE.workspace_home

#: Maps each field to its SD_* variable; `sd config` reads the reverse map (cli/README.md).
_ENV_OVERRIDES = PROFILE.env_overrides()

#: Directory names searched for the intel/llvm bundle, in preference order.
_INTEL_LLVM_BUNDLE_DIRS = ("llvm-sycl-nightly", "llvm-sycl")


@dataclass
class Config(StackConfig):
    """Resolved, platform-specific tool configuration for the SushiDSP build.

    Inherits every field (cmake/ninja paths, cxx, generator, vs_vcvars, ...) from
    :class:`StackConfig`, including the intel/llvm bundle root ``llvm_root``.
    """

    target_bin: str = "sushidsp_host"

    # Left unset so CMake picks the platform default when no compiler is found; see cli/README.md.
    generator: str = ""

    def _clang_name(self) -> str:
        """Return the platform's clang++ executable name."""
        return "clang++.exe" if self.is_windows else "clang++"

    def _clang_candidates(self) -> list[tuple[Path, str]]:
        """Return every intel/llvm bundle root to probe, paired with its origin label."""
        candidates: list[tuple[Path, str]] = []
        if self.llvm_root:
            candidates.append((Path(self.expand(self.llvm_root)), "llvm_root (intel/llvm)"))
        candidates += [(root / "toolchains" / name, f"provision root {root} (intel/llvm)")
                       for root in provision_home.search_roots()
                       for name in _INTEL_LLVM_BUNDLE_DIRS]
        home = workspace_home()
        if home:
            candidates += [(home / "dependencies" / "toolchains" / name,
                            "SushiStack workspace (intel/llvm)")
                           for name in _INTEL_LLVM_BUNDLE_DIRS]
        return candidates

    def _find_bundled_clang(self) -> tuple[str, str]:
        """Return the first bundled clang++ found and its origin, or ('', '')."""
        for root, origin in self._clang_candidates():
            exe = root / "bin" / self._clang_name()
            if exe.is_file():
                return str(exe), origin
        return "", ""

    def bundled_clang(self) -> str:
        """Return the bundled intel/llvm clang++, or '' when none is installed."""
        return self._find_bundled_clang()[0]

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
        origin = self._find_bundled_clang()[1]
        if origin:
            return origin
        if shutil.which("clang++"):
            return "PATH (clang++)"
        return "platform default (no clang++ found; `sd setup --toolchain intel-llvm` provisions one)"

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
