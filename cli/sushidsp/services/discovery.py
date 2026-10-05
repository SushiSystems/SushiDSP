# discovery.py
# SushiDSP - https://github.com/SushiSystems/SushiDSP
# Copyright (c) 2026 Sushi Systems
# Licensed under PolyForm Noncommercial 1.0.0. See LICENSE.
# Commercial use requires a licence from Sushi Systems.
"""Locates built executables under SushiDSP's build tree.

The walk is ``sushicore.discovery.ExecutableIndex``; this module supplies the directories to
prune, which are the sibling checkouts PROFILE names.
"""

from __future__ import annotations

from sushicore.discovery import ExecutableIndex

from ..config import PROFILE

_INDEX = ExecutableIndex(skip_dirs=PROFILE.sibling_skip_dirs())

resolve = _INDEX.resolve
