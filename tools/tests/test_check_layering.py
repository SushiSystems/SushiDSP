# test_check_layering.py
# SushiDSP - https://github.com/SushiSystems/SushiDSP
# Copyright (c) 2026-present Mustafa Garip & Sushi Systems
# Licensed under PolyForm Noncommercial 1.0.0. See LICENSE.
# Commercial use requires a licence from Sushi Systems.
"""Tests the include rules of the layering checker on a small repository of two tiers."""

from __future__ import annotations

import sys
import tempfile
import unittest
from pathlib import Path

REPOSITORY = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(REPOSITORY / "tools"))

from layering.check_layering import collect, rule_upward_includes  # noqa: E402


def write(path: Path, text: str) -> None:
    """Creates a file and its folders with the given text."""
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(text, encoding="utf-8")


class UpwardIncludeTest(unittest.TestCase):
    """Checks the tier form of includes and the rule against reaching up a tier."""

    def setUp(self) -> None:
        self._dir = tempfile.TemporaryDirectory()
        self.root = Path(self._dir.name)
        write(self.root / "modules/foundation/core/include/SushiDSP/foundation/core/base.hpp", "")
        write(self.root / "modules/nodes/room/include/SushiDSP/nodes/room/room.hpp", "")

    def tearDown(self) -> None:
        self._dir.cleanup()

    def findings(self, source: str) -> list[str]:
        """Returns the messages the upward-include rule reports for one source file."""
        write(self.root / "modules/foundation/core/source/user.cpp", source)
        tree = next(iter(collect([self.root])))
        return [issue.message for issue in rule_upward_includes(tree)]

    def test_tier_form_of_a_lower_tier_module_passes(self) -> None:
        """An include of a module in the same tier, named by its tier, is accepted."""
        self.assertEqual(self.findings("#include <SushiDSP/foundation/core/base.hpp>\n"), [])

    def test_include_that_reaches_up_a_tier_is_reported(self) -> None:
        """A foundation module that includes a nodes module is reported."""
        messages = self.findings("#include <SushiDSP/nodes/room/room.hpp>\n")
        self.assertEqual(len(messages), 1)
        self.assertIn("includes up into", messages[0])

    def test_include_that_names_the_wrong_tier_is_reported(self) -> None:
        """An include whose tier segment differs from the module's own tier is reported."""
        messages = self.findings("#include <SushiDSP/nodes/core/base.hpp>\n")
        self.assertEqual(len(messages), 1)
        self.assertIn("names tier 'nodes'", messages[0])

    def test_third_party_include_is_not_a_module(self) -> None:
        """A system or third-party include is never read as a module."""
        self.assertEqual(self.findings("#include <gtest/gtest.h>\n#include <vector>\n"), [])


if __name__ == "__main__":
    unittest.main()
