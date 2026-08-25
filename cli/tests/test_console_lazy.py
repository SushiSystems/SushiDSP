"""Regression tests for the lazy console initialisation.

The bug this pins: building the console at import time calls
config_dir() -> find_project_root(), which raises SystemExit outside a
checkout. Because cli.py imports console transitively, *every* `sd`
invocation -- including `sd --help`, which Typer can answer with no project --
would die with "Not inside a SushiDSP project". These tests confirm importing
the module does no filesystem work, and producing output outside a project
degrades to a default console instead of raising.
"""

from __future__ import annotations

import importlib

import pytest


def _fresh_console_module():
    """Import (or re-import) sushidsp.console with a clean module state."""
    import sushidsp.console as console_mod
    importlib.reload(console_mod)
    return console_mod


def test_import_does_not_build_console():
    """Importing the module must not materialise the console."""
    console_mod = _fresh_console_module()
    assert console_mod._console is None


def test_import_outside_project_does_not_raise(monkeypatch, tmp_path):
    """Simulate running outside a checkout: import + reload must not SystemExit."""
    monkeypatch.chdir(tmp_path)  # tmp_path has no CMakeLists.txt ancestor
    _fresh_console_module()


def test_output_helper_falls_back_outside_project(monkeypatch, tmp_path):
    """console.info(...) outside a project builds a config-less console, no crash."""
    monkeypatch.chdir(tmp_path)
    console_mod = _fresh_console_module()

    seen = []
    real_build = console_mod.build_console

    def spy(sources):
        seen.append(list(sources))
        return real_build(sources)

    monkeypatch.setattr(console_mod, "build_console", spy)

    console_mod.info("hello from outside a checkout")

    assert console_mod._console is not None
    assert seen == [[]], seen


def test_console_attribute_resolves(monkeypatch, tmp_path):
    """The raw Rich `console` attribute resolves lazily too."""
    monkeypatch.chdir(tmp_path)
    console_mod = _fresh_console_module()
    raw = console_mod.console
    assert raw is not None


def test_unknown_attribute_raises_attribute_error():
    console_mod = _fresh_console_module()
    with pytest.raises(AttributeError):
        _ = console_mod.does_not_exist
