# env.py
# SushiDSP - https://github.com/SushiSystems/SushiDSP
# Copyright (c) 2026 Sushi Systems
# Licensed under PolyForm Noncommercial 1.0.0. See LICENSE.
# Commercial use requires a licence from Sushi Systems.
"""Assembles the environment that cmake, ctest and the bench run under for the found compiler.

What a found clang++ needs and how the snapshot is cached are in cli/README.md, "env.py".
"""

from __future__ import annotations

import hashlib
import json
import os
from pathlib import Path

from sushicore.build_env import (
    merge_env,
    prepend_path,
    read_cache,
    snapshot_windows,
    write_cache,
)

from . import console
from .config import Config

CACHE_NAME = ".sushidsp_env.json"


def _cache_key(cfg: Config) -> str:
    material = json.dumps(
        {
            "platform": cfg.platform,
            "vs_vcvars": cfg.vs_vcvars,
            "compiler": cfg.resolved_compiler(),
        },
        sort_keys=True,
    )
    return hashlib.sha256(material.encode()).hexdigest()


def _inject_toolchain_paths(cfg: Config, env: dict[str, str]) -> None:
    compiler = cfg.resolved_compiler()
    if not compiler:
        return
    bin_dir = Path(compiler).parent
    prepend_path(env, "PATH", [str(bin_dir)])
    lib_dir = bin_dir.parent / "lib"
    if not cfg.is_windows and lib_dir.is_dir():
        prepend_path(env, "LD_LIBRARY_PATH", [str(lib_dir)])


def load_build_env(cfg: Config, build_dir: Path) -> dict[str, str]:
    """The environment for cmake/ctest/bench subprocesses.

    @param cfg The resolved configuration.
    @param build_dir Where the vcvars snapshot cache lives.
    @return A full environment mapping; ``os.environ`` unchanged when no
        compiler was found.
    """
    if not cfg.resolved_compiler():
        return dict(os.environ)

    build_dir.mkdir(parents=True, exist_ok=True)
    cache_file = build_dir / CACHE_NAME
    key = _cache_key(cfg)

    snapshot = read_cache(cache_file, key)
    if snapshot is None and cfg.is_windows:
        snapshot = snapshot_windows(cfg, console)
        if snapshot:
            write_cache(cache_file, key, snapshot)

    env = merge_env(os.environ, snapshot) if snapshot else dict(os.environ)
    _inject_toolchain_paths(cfg, env)
    return env
