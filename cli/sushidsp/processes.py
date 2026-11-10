# processes.py
# SushiDSP - https://github.com/SushiSystems/SushiDSP
# Copyright (c) 2026-present Mustafa Garip & Sushi Systems
# Licensed under the Apache License 2.0. See LICENSE.
"""Declares the port through which the project service reads the output of a command.

The system adapter is in processes_system.py and the in-memory one in processes_memory.py.
"""

from __future__ import annotations

from collections.abc import Sequence
from typing import Protocol


class Processes(Protocol):
    """Runs a command to the end and returns what it printed."""

    def capture(self, argv: Sequence[str]) -> tuple[int, str]:
        """Returns the exit code and the standard output of the command *argv*."""
        ...
