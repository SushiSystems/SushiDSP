"""Diagnostic command: inspect the resolved configuration.

`sd config` answers *what* the CLI resolved and *where each value came from* --
default, config.toml, or an environment override. It is
:class:`sushicore.diag.Diagnostics`, so SushiDSP gets the same command, the same
table and the same vocabulary as every other CLI in the stack without carrying
an implementation of it.

There is no `sd env` counterpart: SushiDSP snapshots no build environment (it
pins no toolchain and loads no vcvars), so ``build_env`` is left unset and the
command is not registered.
"""

from __future__ import annotations

from sushicore.diag import Diagnostics

from .. import config as config_module
from .. import console
from ..config import PROFILE

_DIAG = Diagnostics(
    profile=PROFILE,
    console=console,
    config=config_module,
)

config_show = _DIAG.config_show
