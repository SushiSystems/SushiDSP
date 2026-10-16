# settings.py
# SushiDSP - https://github.com/SushiSystems/SushiDSP
# Copyright (c) 2026-present Mustafa Garip & Sushi Systems
# Licensed under PolyForm Noncommercial 1.0.0. See LICENSE.
# Commercial use requires a licence from Sushi Systems.
"""Holds the settings of one repository that the shared checkers read.

copy_shared_tools.py writes this file only when a repository has none, and never overwrites it.
"""

from __future__ import annotations

K_TIER_ORDER: tuple[str, ...] = ("foundation", "nodes", "catalog", "session")
K_DOMAIN_TIERS: tuple[str, ...] = ()
K_SAME_TIER_EDGES: frozenset[tuple[str, str]] = frozenset({
    ("circuits", "math"),
    ("io", "math"),
    ("core", "io"),
    ("device", "core"),
    ("device", "audio_format"),
    ("signal_measure", "core"),
    ("signal_measure", "math"),
    ("spice_reference", "math"),
    ("circuit_blocks", "core"),
    ("circuit_blocks", "math"),
    ("circuit_blocks", "circuits"),
    ("benchmark_harness", "core"),
    ("room_scenes", "room"),
})
K_MODULE_FUNCTION = ""
K_PUBLIC_DEPENDS = "DEPENDS"
K_PRIVATE_DEPENDS = "PRIVATE_DEPENDS"
K_INTERNAL_MARK = "INTERNAL"
K_CLOSED = False
K_MACRO_PREFIXES: tuple[str, ...] = ()
K_LOG_PREFIX = "SD"
K_CONSOLE_PATHS: tuple[str, ...] = ("modules/foundation/benchmark_harness",)
