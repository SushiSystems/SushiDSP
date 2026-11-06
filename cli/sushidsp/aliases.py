# aliases.py
# SushiDSP - https://github.com/SushiSystems/SushiDSP
# Copyright (c) 2026-present Mustafa Garip & Sushi Systems
# Licensed under the Apache License 2.0. See LICENSE.
"""Holds the old spellings `sd` still answers to, each with the day it leaves."""

from __future__ import annotations

from datetime import date

from sushicore.aliases import Alias, AliasTable

K_REMOVED_AFTER = date(2027, 3, 25)

K_ROWS: tuple[Alias, ...] = (
    Alias("sd doxygen", "sd docs", K_REMOVED_AFTER),
    Alias("sd run", "sd host", K_REMOVED_AFTER),
)

ALIASES = AliasTable("sd", K_ROWS)
