# test_console_lazy.py
# SushiDSP - https://github.com/SushiSystems/SushiDSP
# Copyright (c) 2026-present Mustafa Garip & Sushi Systems
# Licensed under the Apache License 2.0. See LICENSE.
"""Tests that the console module is wired to sushicore's LazyConsole and builds on first use.

The failure these tests pin is in cli/README.md, "tests/test_console_lazy.py".
"""

from __future__ import annotations

import os
import subprocess
import sys
from pathlib import Path

import pytest
import sushicore
from rich.console import Console as RichConsole
from sushicore.cli_console import LazyConsole

from sushidsp import console as console_module

K_REPOSITORY = Path(__file__).resolve().parents[2]
K_IMPORT_PATH = os.pathsep.join([str(Path(console_module.__file__).resolve().parents[1]),
                                 str(Path(sushicore.__file__).resolve().parents[1])])


def _outside_a_checkout() -> Path:
    """Returns a folder with no CMakeLists.txt above it."""
    raise SystemExit("Not inside a SushiDSP project")


@pytest.mark.small
def test_the_module_wires_a_lazy_console():
    """The module prints through a LazyConsole sushicore owns."""
    assert isinstance(console_module.wired(), LazyConsole)


@pytest.mark.small
def test_a_module_attribute_the_console_lacks_raises_attribute_error():
    """An unknown name on the module raises AttributeError without building the console."""
    with pytest.raises(AttributeError):
        getattr(console_module, "does_not_exist")


@pytest.mark.small
def test_a_console_not_yet_used_is_not_built():
    """A fresh lazy console builds nothing, and asks nothing of its config folder."""
    asked: list[str] = []

    def resolve() -> Path:
        asked.append("asked")
        return Path("unused")

    assert not LazyConsole(resolve).built
    assert asked == []


@pytest.mark.small
def test_a_console_outside_a_checkout_has_no_config_sources():
    """A config folder that raises SystemExit leaves the sources empty instead of failing."""
    assert list(LazyConsole(_outside_a_checkout).sources()) == []


@pytest.mark.small
def test_a_console_outside_a_checkout_prints_without_failing():
    """The first print outside a checkout builds a console with no sources and does not raise."""
    lazy = LazyConsole(_outside_a_checkout)

    lazy.attribute("info")("hello from outside a checkout")

    assert lazy.built


@pytest.mark.small
def test_the_raw_console_attribute_resolves_to_a_rich_console():
    """The `console` attribute resolves lazily to the Rich console."""
    assert isinstance(LazyConsole(_outside_a_checkout).attribute("console"), RichConsole)


@pytest.mark.small
def test_an_attribute_the_console_lacks_raises_attribute_error():
    """The lazy console raises AttributeError for a name it does not carry."""
    with pytest.raises(AttributeError):
        LazyConsole(_outside_a_checkout).attribute("does_not_exist")


@pytest.mark.medium
def test_importing_the_module_does_not_build_the_console():
    """A fresh interpreter that only imports the module has not built the console."""
    script = "from sushidsp import console; print(console.wired().built)"

    finished = subprocess.run([sys.executable, "-c", script], cwd=K_REPOSITORY,
                              env={**os.environ, "PYTHONPATH": K_IMPORT_PATH},
                              capture_output=True, text=True, check=False)

    assert finished.stdout.split() == ["False"], finished.stderr


@pytest.mark.medium
def test_importing_the_module_outside_a_checkout_does_not_raise(tmp_path):
    """A fresh interpreter outside a checkout imports the module and prints through it."""
    script = "from sushidsp import console; console.info('outside'); print(console.wired().built)"

    finished = subprocess.run([sys.executable, "-c", script], cwd=tmp_path,
                              env={**os.environ, "PYTHONPATH": K_IMPORT_PATH},
                              capture_output=True, text=True, check=False)

    assert finished.returncode == 0, finished.stderr
    assert finished.stdout.split()[-1] == "True"


@pytest.mark.medium
def test_the_sources_inside_a_checkout_are_this_repositorys_config(monkeypatch):
    """Inside a checkout the wired console layers this CLI's own two config files."""
    monkeypatch.chdir(K_REPOSITORY)

    sources = list(console_module.wired().sources())

    assert sources == [K_REPOSITORY / "cli" / "config.toml",
                       K_REPOSITORY / "cli" / "config.local.toml"]
