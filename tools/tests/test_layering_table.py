# test_layering_table.py
# SushiDSP - https://github.com/SushiSystems/SushiDSP
# Copyright (c) 2026-present Mustafa Garip & Sushi Systems
# Licensed under PolyForm Noncommercial 1.0.0. See LICENSE.
# Commercial use requires a licence from Sushi Systems.
"""Tests that the tier order in cmake/SushiDSPLayers.cmake matches the layering checker's copy."""

from __future__ import annotations

import re
import sys
import unittest
from pathlib import Path

REPOSITORY = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(REPOSITORY / "tools"))

from layering.check_layering import K_TIER_ORDER  # noqa: E402

K_CMAKE_LAYERS = REPOSITORY / "cmake" / "SushiDSPLayers.cmake"
K_TIER_LIST = re.compile(r"set\(SUSHIDSP_TIER_ORDER([^)]*)\)")


def cmake_tier_order(text: str) -> tuple[str, ...]:
    """Returns the tier names the CMake table lists, lowest first."""
    match = K_TIER_LIST.search(text)
    if match is None:
        raise AssertionError("cmake/SushiDSPLayers.cmake has no SUSHIDSP_TIER_ORDER list")
    return tuple(match.group(1).split())


class LayeringTableTest(unittest.TestCase):
    """Pins the tier order to one value in two places."""

    def test_checker_order_matches_cmake_table(self) -> None:
        """The checker and the CMake table name the same tiers in the same order."""
        cmake_order = cmake_tier_order(K_CMAKE_LAYERS.read_text(encoding="utf-8"))
        self.assertEqual(cmake_order, K_TIER_ORDER)


if __name__ == "__main__":
    unittest.main()
