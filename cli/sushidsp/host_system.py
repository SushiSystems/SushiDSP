# host_system.py
# SushiDSP - https://github.com/SushiSystems/SushiDSP
# Copyright (c) 2026-present Mustafa Garip & Sushi Systems
# Licensed under the Apache License 2.0. See LICENSE.
"""Reads the real environment, the real PATH, the real disk and the real workspace."""

from __future__ import annotations

import os
import shutil
from collections.abc import Callable
from pathlib import Path
from typing import final

from sushicore.provision import home as provision_home


@final
class SystemHost:
    """Answers the `Host` port from `os.environ`, `shutil.which`, the disk and sushicore.

    Args:
        workspace_home: Finds the workspace root; the module's own lookup in production.
    """

    def __init__(self, workspace_home: Callable[[], Path | None]) -> None:
        """Keeps the workspace lookup."""
        self._workspace_home = workspace_home

    def environment(self) -> dict[str, str]:
        """Returns a copy of `os.environ`."""
        return dict(os.environ)

    def which(self, name: str) -> str | None:
        """Returns the path `shutil.which` finds for *name*, or None."""
        return shutil.which(name)

    def is_file(self, path: Path) -> bool:
        """Returns whether a file is at *path* on the disk."""
        return path.is_file()

    def provision_roots(self) -> list[Path]:
        """Returns the provision root and its legacy roots."""
        return provision_home.search_roots()

    def workspace_home(self) -> Path | None:
        """Returns the workspace root the module finds."""
        return self._workspace_home()
