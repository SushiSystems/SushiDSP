# processes_memory.py
# SushiDSP - https://github.com/SushiSystems/SushiDSP
# Copyright (c) 2026-present Mustafa Garip & Sushi Systems
# Licensed under the Apache License 2.0. See LICENSE.
"""A processes port that starts nothing, for tests of code that reads a command's output."""

from __future__ import annotations

from collections.abc import Sequence
from dataclasses import dataclass, field
from typing import final


@final
@dataclass
class MemoryProcesses:
    """Answers the `Processes` port from its fields and starts no process.

    Attributes:
        outputs: The exit code and standard output of each command, by its argv.
        asked: Every command asked, in order.
    """

    outputs: dict[tuple[str, ...], tuple[int, str]] = field(default_factory=dict)
    asked: list[tuple[str, ...]] = field(default_factory=list)

    def capture(self, argv: Sequence[str]) -> tuple[int, str]:
        """Records *argv* and returns its output; a command not listed fails with code 1."""
        key = tuple(argv)
        self.asked.append(key)
        return self.outputs.get(key, (1, ""))
