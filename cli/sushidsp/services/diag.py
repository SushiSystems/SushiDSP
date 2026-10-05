# diag.py
# SushiDSP - https://github.com/SushiSystems/SushiDSP
# Copyright (c) 2026 Sushi Systems
# Licensed under PolyForm Noncommercial 1.0.0. See LICENSE.
# Commercial use requires a licence from Sushi Systems.
"""Builds the diagnostics object behind `sd config` and `sd env`.

Both reports are :class:`sushicore.diag.Diagnostics`; `sd config` adds which step of the
compiler search won and which generator it led to.
"""

from __future__ import annotations

from typing import Callable, final

from sushicore.diag import Diagnostics

from .. import config as config_module
from .. import console
from ..config import PROFILE, Config, load_config
from ..env import load_build_env


@final
class CompilerDiagnostics:
    """Prints sushicore's two reports and names the compiler search's result.

    Args:
        shared: The diagnostics sushicore provides for this module.
        console: The CLI's console module.
        load_config: Returns the resolved configuration.
    """

    __slots__ = ("_shared", "_console", "_load_config")

    def __init__(self, shared: Diagnostics, console, load_config: Callable[[], Config]) -> None:
        """Stores the shared diagnostics, the console and the configuration loader."""
        self._shared = shared
        self._console = console
        self._load_config = load_config

    def config_show(self) -> int:
        """Prints the resolved configuration, then how the compiler and generator were chosen."""
        rc = self._shared.config_show()
        cfg = self._load_config()
        generator = cfg.resolved_generator() or "(CMake platform default)"
        self._console.info(f"Compiler search: {cfg.compiler_source()}")
        self._console.info(f"Generator:       {generator}")
        return rc

    def env_dump(self, show_all: bool = False) -> int:
        """Prints the environment the build subprocesses run under."""
        return self._shared.env_dump(show_all=show_all)


DIAGNOSTICS = CompilerDiagnostics(
    Diagnostics(
        profile=PROFILE,
        console=console,
        config=config_module,
        build_env=load_build_env,
    ),
    console,
    load_config,
)
