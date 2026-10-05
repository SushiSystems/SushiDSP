# console.py
# SushiDSP - https://github.com/SushiSystems/SushiDSP
# Copyright (c) 2026 Sushi Systems
# Licensed under PolyForm Noncommercial 1.0.0. See LICENSE.
# Commercial use requires a licence from Sushi Systems.
"""Builds the SushiDSP CLI's console on first use, through sushicore's LazyConsole.

The reason the console is built lazily is in cli/README.md, "console.py".
"""

from __future__ import annotations

from sushicore.cli_console import LazyConsole

from .config import config_dir

_lazy = LazyConsole(config_dir)


def current():
    """Return the sushicore Console this run prints through, building it on first use."""
    return _lazy.get()


# PEP 562: attributes resolve on first access, so an import never reads the project layout.
__getattr__ = _lazy.attribute
