# host_memory.py
# SushiDSP - https://github.com/SushiSystems/SushiDSP
# Copyright (c) 2026-present Mustafa Garip & Sushi Systems
# Licensed under the Apache License 2.0. See LICENSE.
"""A host that answers from the values it was built with, for tests of code that reads the host."""

from __future__ import annotations

from dataclasses import dataclass, field
from pathlib import Path
from typing import final


@final
@dataclass(frozen=True, slots=True)
class MemoryHost:
    """Answers the `Host` port from its fields and touches neither the process nor the disk.

    Attributes:
        variables: The environment variables, name to value.
        executables: The executables on the search path, name to path.
        files: The files that exist.
        roots: The provision roots, in lookup order.
        workspace: The workspace root, or None for a standalone checkout.
    """

    variables: dict[str, str] = field(default_factory=dict)
    executables: dict[str, str] = field(default_factory=dict)
    files: frozenset[Path] = frozenset()
    roots: tuple[Path, ...] = ()
    workspace: Path | None = None

    def environment(self) -> dict[str, str]:
        """Returns a copy of the variables."""
        return dict(self.variables)

    def which(self, name: str) -> str | None:
        """Returns the path the executables map gives *name*, or None."""
        return self.executables.get(name)

    def is_file(self, path: Path) -> bool:
        """Returns whether *path* is one of the files."""
        return path in self.files

    def provision_roots(self) -> list[Path]:
        """Returns the provision roots."""
        return list(self.roots)

    def workspace_home(self) -> Path | None:
        """Returns the workspace root."""
        return self.workspace
