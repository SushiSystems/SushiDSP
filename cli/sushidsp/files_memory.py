# files_memory.py
# SushiDSP - https://github.com/SushiSystems/SushiDSP
# Copyright (c) 2026-present Mustafa Garip & Sushi Systems
# Licensed under the Apache License 2.0. See LICENSE.
"""A files port that removes nothing from the disk, for tests of code that removes a tree."""

from __future__ import annotations

from collections.abc import Callable
from dataclasses import dataclass, field
from pathlib import Path
from typing import final


@final
@dataclass
class MemoryFiles:
    """Answers the `Files` port from its fields and touches no file.

    Attributes:
        locked: The folders whose removal fails the way a file held open on Windows fails it.
        removed: The folders removed, in order.
    """

    locked: frozenset[Path] = frozenset()
    removed: list[Path] = field(default_factory=list)

    def rmtree(self, path: Path, on_error: Callable[[Callable[[str], object], str, object], None]) -> None:
        """Records *path* as removed, or raises PermissionError when it is locked."""
        if path in self.locked:
            raise PermissionError(13, "The process cannot access the file", str(path))
        self.removed.append(path)
