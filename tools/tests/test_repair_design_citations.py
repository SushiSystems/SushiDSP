# test_repair_design_citations.py
# SushiSkills - https://github.com/SushiSystems/SushiSkills
# Copyright (c) 2026-present Mustafa Garip & Sushi Systems
# Licensed under PolyForm Noncommercial 1.0.0. See LICENSE.
# Commercial use requires a licence from Sushi Systems.
"""Tests the repair of citations of design documents that moved."""

from __future__ import annotations

import contextlib
import io
import sys
import tempfile
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))

from documentation.repair_design_citations import main, run  # noqa: E402

K_TOPIC = "# Topic\n\n**Status:** Open — under way\n"

K_TOPICS = {
    "docs/design/AUDIO/README.md": K_TOPIC,
    "docs/design/AUDIO/MIXER.md": "# Mixer\n",
    "docs/design/ECS.md": K_TOPIC,
    "docs/design/README.md": "# Design\n",
}

K_FILES = {
    **K_TOPICS,
    "docs/guides/use.md": "See `docs/design/AUDIO.md` and [the ecs](../design/ECS/README.md#phases).\n",
    "modules/core/source/mixer.cpp": "// The rule is in docs/design/ECS/README.md.\n",
}


def _root(folder: str, files: dict[str, str] | None = None) -> Path:
    """Writes the given files, or the default ones, under a root and returns it."""
    root = Path(folder) / "alpha"
    for name, text in (K_FILES if files is None else files).items():
        path = root / name
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_text(text, encoding="utf-8")
    return root


class RepairTest(unittest.TestCase):
    """Covers what the tool rewrites and what it leaves."""

    def test_points_a_file_citation_at_the_folder_that_replaced_it(self) -> None:
        """Rewrites a citation of AUDIO.md to AUDIO/README.md."""
        with tempfile.TemporaryDirectory() as folder:
            root = _root(folder)
            repairs = run(root, True)
            self.assertIn("docs/design/AUDIO/README.md", (root / "docs/guides/use.md").read_text(encoding="utf-8"))
            self.assertTrue(any(item.old == "docs/design/AUDIO.md" for item in repairs))

    def test_points_a_folder_citation_at_the_file_that_replaced_it(self) -> None:
        """Rewrites ECS/README.md to ECS.md in a link, keeping its anchor, and in a source comment."""
        with tempfile.TemporaryDirectory() as folder:
            root = _root(folder)
            run(root, True)
            self.assertIn("[the ecs](../design/ECS.md#phases)", (root / "docs/guides/use.md").read_text(encoding="utf-8"))
            self.assertIn("docs/design/ECS.md.", (root / "modules/core/source/mixer.cpp").read_text(encoding="utf-8"))

    def test_writes_nothing_without_apply(self) -> None:
        """Lists the repairs and leaves every file as it was."""
        with tempfile.TemporaryDirectory() as folder:
            root = _root(folder)
            before = (root / "docs/guides/use.md").read_text(encoding="utf-8")
            self.assertTrue(run(root, False))
            self.assertEqual((root / "docs/guides/use.md").read_text(encoding="utf-8"), before)

    def test_reports_a_page_the_topic_does_not_have(self) -> None:
        """Reports a citation of a missing page and does not rewrite it."""
        with tempfile.TemporaryDirectory() as folder:
            root = _root(folder, {**K_TOPICS, "docs/guides/use.md": "See docs/design/AUDIO/GONE.md.\n"})
            repairs = run(root, True)
            self.assertEqual([(item.old, item.new) for item in repairs], [("docs/design/AUDIO/GONE.md", None)])
            self.assertIn("AUDIO/GONE.md", (root / "docs/guides/use.md").read_text(encoding="utf-8"))

    def test_reports_a_topic_that_does_not_exist(self) -> None:
        """Reports a citation of a topic nobody wrote."""
        with tempfile.TemporaryDirectory() as folder:
            root = _root(folder, {**K_TOPICS, "docs/guides/use.md": "See `docs/design/VIDEO.md`.\n"})
            self.assertEqual([item.new for item in run(root, False)], [None])

    def test_leaves_a_citation_that_resolves_and_the_archive_alone(self) -> None:
        """Reads no archive file and changes no citation that already points at a file."""
        files = {**K_TOPICS, "docs/archive/old.md": "See docs/design/AUDIO.md.\n", "docs/guides/use.md": "See docs/design/AUDIO/MIXER.md.\n"}
        with tempfile.TemporaryDirectory() as folder:
            root = _root(folder, files)
            self.assertEqual([item for item in run(root, True) if "archive" in item.path.parts], [])
            self.assertEqual((root / "docs/archive/old.md").read_text(encoding="utf-8"), "See docs/design/AUDIO.md.\n")
            self.assertEqual((root / "docs/guides/use.md").read_text(encoding="utf-8"), "See docs/design/AUDIO/MIXER.md.\n")


class CommandTest(unittest.TestCase):
    """Covers the exit code."""

    def test_exits_one_only_for_a_citation_it_cannot_place(self) -> None:
        """Returns 0 after a plain repair and 1 while a citation is unplaced."""
        with tempfile.TemporaryDirectory() as folder:
            root = _root(folder)
            with contextlib.redirect_stdout(io.StringIO()):
                self.assertEqual(main([str(root), "--apply"]), 0)
                (root / "docs/guides/other.md").write_text("docs/design/GHOST.md\n", encoding="utf-8")
                self.assertEqual(main([str(root)]), 1)


if __name__ == "__main__":
    unittest.main()
