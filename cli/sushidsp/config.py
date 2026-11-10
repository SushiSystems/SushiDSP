# config.py
# SushiDSP - https://github.com/SushiSystems/SushiDSP
# Copyright (c) 2026-present Mustafa Garip & Sushi Systems
# Licensed under the Apache License 2.0. See LICENSE.
"""Loads the layered configuration of the SushiDSP CLI and finds the compiler it builds with.

The layer precedence and the compiler search order are in cli/README.md, "config.py".
"""

from __future__ import annotations

from dataclasses import dataclass
from pathlib import Path

from sushicore.module_config import ModuleConfig
from sushicore.profile import ModuleProfile
from sushicore.stack_config import StackConfig

from .host import Host
from .host_system import SystemHost

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


def system_host() -> Host:
    """Return the host of the machine `sd` runs on."""
    return SystemHost(workspace_home)

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

    def _clang_candidates(self, host: Host) -> list[tuple[Path, str]]:
        """Return every intel/llvm bundle root to probe, paired with its origin label."""
        candidates: list[tuple[Path, str]] = []
        if self.llvm_root:
            candidates.append((Path(self.expand(self.llvm_root)), "llvm_root (intel/llvm)"))
        candidates += [(root / "toolchains" / name, f"provision root {root} (intel/llvm)")
                       for root in host.provision_roots()
                       for name in _INTEL_LLVM_BUNDLE_DIRS]
        home = host.workspace_home()
        if home:
            candidates += [(home / "dependencies" / "toolchains" / name,
                            "SushiStack workspace (intel/llvm)")
                           for name in _INTEL_LLVM_BUNDLE_DIRS]
        return candidates

    def _find_bundled_clang(self, host: Host) -> tuple[str, str]:
        """Return the first bundled clang++ found and its origin, or ('', '')."""
        for root, origin in self._clang_candidates(host):
            exe = root / "bin" / self._clang_name()
            if host.is_file(exe):
                return str(exe), origin
        return "", ""

    def bundled_clang(self, host: Host | None = None) -> str:
        """Return the bundled intel/llvm clang++ found on *host*, or '' when none is installed."""
        return self._find_bundled_clang(host or system_host())[0]

    def resolved_compiler(self, root: Path | None = None, host: Host | None = None) -> str:
        """The C++ compiler to drive the build, or '' for CMake's platform default.

        @param root Unused; accepted so the shared `config` diagnostics, which
            call ``resolved_compiler(root)`` on every config that has one, work.
        @param host Where the search reads the machine; the system's when None.
        """
        host = host or system_host()
        if self.cxx:
            return self.expand(self.cxx)
        bundled = self.bundled_clang(host)
        if bundled:
            return bundled
        return host.which("clang++") or ""

    def compiler_source(self, host: Host | None = None) -> str:
        """Which step of the search produced resolved_compiler(), for diagnostics."""
        host = host or system_host()
        if self.cxx:
            return "pinned (cxx)"
        origin = self._find_bundled_clang(host)[1]
        if origin:
            return origin
        if host.which("clang++"):
            return "PATH (clang++)"
        return "platform default (no clang++ found; `sd setup --toolchain intel-llvm` provisions one)"

    def resolved_ninja(self, host: Host | None = None) -> str:
        """The ninja binary to hand CMake, or '' when none is available."""
        if self.ninja_exe:
            return self.expand(self.ninja_exe)
        return (host or system_host()).which("ninja") or ""

    def resolved_generator(self, host: Host | None = None) -> str:
        """The CMake generator: pinned, else Ninja with a found compiler, else CMake's default.

        A found clang++ must not be paired with the Visual Studio generator (which
        would silently build with MSVC anyway), so it takes Ninja when one is
        available; with no ninja the compiler is still passed and CMake's
        single-config Makefile/NMake default carries it.
        """
        if self.generator:
            return self.generator
        host = host or system_host()
        if self.resolved_compiler(host=host) and self.resolved_ninja(host):
            return "Ninja"
        return ""

    def is_multi_config(self, host: Host | None = None) -> bool:
        """Whether the generator in use bakes the configuration at build time
        (Visual Studio, Xcode) rather than at configure time (Ninja, Makefiles)."""
        generator = self.resolved_generator(host)
        return generator == "" and self.is_windows or generator.startswith(("Visual Studio", "Xcode"))


def load_config() -> Config:
    """Load and resolve the layered configuration for the current platform."""
    return _MODULE.load(Config)
