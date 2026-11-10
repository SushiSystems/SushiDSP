# processes_system.py
# SushiDSP - https://github.com/SushiSystems/SushiDSP
# Copyright (c) 2026-present Mustafa Garip & Sushi Systems
# Licensed under the Apache License 2.0. See LICENSE.
"""Runs commands as processes of the machine."""

from __future__ import annotations

import subprocess
from collections.abc import Sequence
from typing import final


@final
class SystemProcesses:
    """Answers the `Processes` port by starting the command."""

    def capture(self, argv: Sequence[str]) -> tuple[int, str]:
        """Starts *argv* and returns its exit code and standard output."""
        finished = subprocess.run(list(argv), capture_output=True, text=True, check=False)
        return finished.returncode, finished.stdout
