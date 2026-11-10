# package_resolver.py
# SushiDSP - https://github.com/SushiSystems/SushiDSP
# Copyright (c) 2026-present Mustafa Garip & Sushi Systems
# Licensed under the Apache License 2.0. See LICENSE.
"""Declares the port through which the project service obtains the library packages to link.

The in-memory adapter is in package_resolver_memory.py; the system one is the resolver of
``Config``, which ``library_packages.resolve`` uses when the workspace names no other.
"""

from __future__ import annotations

from collections.abc import Callable, Mapping
from pathlib import Path
from typing import Protocol

from sushicore.provision.sushi_packages import PackageSet


class PackageResolver(Protocol):
    """Chooses, and builds when missing, a package of every library the project links."""

    def resolve(self, root: Path, build_type: str, options: Mapping[str, str],
                report: Callable[[str], None], warn: Callable[[str], None],
                compiler: str) -> PackageSet:
        """Returns the package set for *build_type* built with *compiler*.

        Raises:
            sushicore.provision.sushi_packages.PackageError: A package cannot be chosen.
        """
        ...
