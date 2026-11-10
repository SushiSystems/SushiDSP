# package_resolver_memory.py
# SushiDSP - https://github.com/SushiSystems/SushiDSP
# Copyright (c) 2026-present Mustafa Garip & Sushi Systems
# Licensed under the Apache License 2.0. See LICENSE.
"""A package resolver that builds nothing, for tests of code that links library packages."""

from __future__ import annotations

from collections.abc import Callable, Mapping
from pathlib import Path


class MemoryPackages:
    """Answers the `PackageResolver` port with itself as the SushiLog and SushiProfiler packages.

    Attributes:
        asked: One entry per request: its root, build type, options and compiler.
    """

    config_dir = "/store/sushilog/abc/prefix/lib/cmake/SushiLog"
    profiler_config_dir = "/store/sushiprofiler/def/prefix/lib/cmake/SushiProfiler"
    run_dir = "/store/sushilog/abc/prefix/lib"
    left_out: frozenset[str] = frozenset()

    def __init__(self) -> None:
        """Starts with no request recorded."""
        self.asked: list[dict] = []

    def names(self) -> list[str]:
        """Returns the library names."""
        return ["sushilog", "sushiprofiler"]

    def definitions(self) -> list[str]:
        """Returns the config folder definitions."""
        return [f"-DSushiLog_DIR={self.config_dir}",
                f"-DSushiProfiler_DIR={self.profiler_config_dir}"]

    def run_dirs(self, platform: str) -> list[str]:
        """Returns the run-time folder."""
        return [self.run_dir]

    def resolve(self, root: Path, build_type: str, options: Mapping[str, str],
                report: Callable[[str], None], warn: Callable[[str], None],
                compiler: str) -> "MemoryPackages":
        """Records the request and returns this object as the package set."""
        self.asked.append({"root": root, "build_type": build_type,
                           "options": dict(options), "compiler": compiler})
        return self
