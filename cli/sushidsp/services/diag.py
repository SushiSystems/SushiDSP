# Copyright (c) 2026-present Mustafa Garip & Sushi Systems
# Licensed under the Apache License, Version 2.0. See LICENSE.
"""Diagnostic command: inspect the resolved configuration.

`sd config` answers *what* the CLI resolved and *where each value came from* --
default, config.toml, or an environment override. It is
:class:`sushicore.diag.Diagnostics`, so SushiDSP gets the same command, the same
table and the same vocabulary as every other CLI in the stack without carrying
an implementation of it.

`sd env` shows the environment the build subprocesses get (the vcvars snapshot
plus the detected toolchain's bin, see ``sushidsp.env``), and `sd config`
additionally names which step of the compiler search won.
"""

from __future__ import annotations

from sushicore.diag import Diagnostics

from .. import config as config_module
from .. import console
from ..config import PROFILE, find_project_root, load_config
from ..env import load_build_env

_DIAG = Diagnostics(
    profile=PROFILE,
    console=console,
    config=config_module,
    build_env=load_build_env,
)

env_dump = _DIAG.env_dump


def config_show() -> int:
    """Prints the resolved configuration, then how the compiler was chosen."""
    rc = _DIAG.config_show()
    cfg = load_config()
    console.info(f"Compiler search: {cfg.compiler_source()}")
    console.info(f"Generator:       {cfg.resolved_generator() or '(CMake platform default)'}")
    return rc
