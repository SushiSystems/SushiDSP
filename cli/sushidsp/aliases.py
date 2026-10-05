# aliases.py
# SushiDSP - https://github.com/SushiSystems/SushiDSP
# Copyright (c) 2026 Sushi Systems
# Licensed under PolyForm Noncommercial 1.0.0. See LICENSE.
# Commercial use requires a licence from Sushi Systems.
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
