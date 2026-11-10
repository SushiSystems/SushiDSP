# host.py
# SushiDSP - https://github.com/SushiSystems/SushiDSP
# Copyright (c) 2026-present Mustafa Garip & Sushi Systems
# Licensed under the Apache License 2.0. See LICENSE.
"""Declares the port through which `sd` reads the machine it runs on when it picks a toolchain.

The system adapter is in host_system.py and the in-memory one in host_memory.py.
"""

from __future__ import annotations

from pathlib import Path
from typing import Protocol


class Host(Protocol):
    """Reads the environment, the executable search and the toolchain folders of the machine."""

    def environment(self) -> dict[str, str]:
        """Returns a copy of every environment variable; changing the copy changes nothing."""
        ...

    def which(self, name: str) -> str | None:
        """Returns the path of the executable *name* on the search path, or None."""
        ...

    def is_file(self, path: Path) -> bool:
        """Returns whether a file is at *path*."""
        ...

    def provision_roots(self) -> list[Path]:
        """Returns the provision roots a toolchain is looked up in, in lookup order."""
        ...

    def workspace_home(self) -> Path | None:
        """Returns the SushiStack workspace root, or None when the checkout is standalone."""
        ...
