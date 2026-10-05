# test_build_type_resolution.py
# SushiDSP - https://github.com/SushiSystems/SushiDSP
# Copyright (c) 2026 Sushi Systems
# Licensed under PolyForm Noncommercial 1.0.0. See LICENSE.
# Commercial use requires a licence from Sushi Systems.
"""The build type a command acts on when --type is absent."""

from __future__ import annotations

import pytest

from sushidsp.services import project as project_svc
from sushidsp.services.project import BuildType


@pytest.fixture
def tree(tmp_path, monkeypatch):
    """A project root whose build/ holds a cache naming one build type."""
    def make(cached: str | None) -> None:
        monkeypatch.setattr(project_svc, "find_project_root", lambda: tmp_path)
        build_dir = tmp_path / "build"
        build_dir.mkdir(exist_ok=True)
        if cached is not None:
            (build_dir / "CMakeCache.txt").write_text(
                f"CMAKE_BUILD_TYPE:STRING={cached}\n", encoding="utf-8")
    return make


def test_an_explicit_type_wins_over_the_tree(tree):
    tree("Release")
    assert project_svc.resolve_build_type(BuildType.debug) is BuildType.debug


def test_a_release_tree_keeps_release(tree):
    tree("Release")
    assert project_svc.resolve_build_type(None) is BuildType.release


def test_a_relwithdebinfo_tree_keeps_relwithdebinfo(tree):
    tree("RelWithDebInfo")
    assert project_svc.resolve_build_type(None) is BuildType.relwithdebinfo


def test_an_unconfigured_tree_falls_back_to_debug(tree):
    tree(None)
    assert project_svc.resolve_build_type(None) is BuildType.debug


def test_an_unknown_cached_type_falls_back_to_debug(tree):
    tree("MinSizeRel")
    assert project_svc.resolve_build_type(None) is BuildType.debug


def test_outside_a_checkout_the_shared_message_ends_the_command(monkeypatch):
    """No type is guessed outside a checkout; find_project_root's exit goes through."""
    def raise_outside():
        """Raises what find_project_root raises outside a checkout."""
        raise SystemExit("Not inside a SushiDSP project")
    monkeypatch.setattr(project_svc, "find_project_root", raise_outside)
    with pytest.raises(SystemExit):
        project_svc.resolve_build_type(None)
