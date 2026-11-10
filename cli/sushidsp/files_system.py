# files_system.py
# SushiDSP - https://github.com/SushiSystems/SushiDSP
# Copyright (c) 2026-present Mustafa Garip & Sushi Systems
# Licensed under the Apache License 2.0. See LICENSE.
"""Removes folders from the disk through ``shutil``."""

from __future__ import annotations

import shutil
from collections.abc import Callable
from pathlib import Path
from typing import final


@final
class SystemFiles:
    """Answers the `Files` port on the disk."""

    def rmtree(self, path: Path, on_error: Callable[[Callable[[str], object], str, object], None]) -> None:
        """Removes *path* with ``shutil.rmtree``, handing *on_error* every file it fails on."""
        shutil.rmtree(path, onerror=on_error)
