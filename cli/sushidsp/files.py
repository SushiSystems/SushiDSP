# files.py
# SushiDSP - https://github.com/SushiSystems/SushiDSP
# Copyright (c) 2026-present Mustafa Garip & Sushi Systems
# Licensed under the Apache License 2.0. See LICENSE.
"""Declares the port through which the project service removes a build tree.

The system adapter is in files_system.py and the in-memory one in files_memory.py.
"""

from __future__ import annotations

from collections.abc import Callable
from pathlib import Path
from typing import Protocol


class Files(Protocol):
    """Removes folders from the disk."""

    def rmtree(self, path: Path, on_error: Callable[[Callable[[str], object], str, object], None]) -> None:
        """Removes the folder *path* with everything in it.

        Args:
            path: The folder to remove.
            on_error: Called as ``on_error(function, target, exc_info)`` for a file that could
                not be removed, as ``shutil.rmtree`` calls its ``onerror``.

        Raises:
            OSError: A file could not be removed and *on_error* did not clear the cause.
        """
        ...
