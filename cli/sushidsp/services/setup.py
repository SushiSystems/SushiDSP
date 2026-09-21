# Copyright (c) 2026-present Mustafa Garip & Sushi Systems
# Licensed under the Apache License, Version 2.0. See LICENSE.
"""Getting this checkout's dependencies, with or without a SushiStack workspace.

Two provisioning paths, one command. Inside a workspace with `hub` on PATH the
job is `hub install`'s: it aggregates every linked checkout's fragment and
downloads nothing twice. Outside one, this repository's own fragment is read
through :mod:`sushicore.deps_fragment` and each entry reported. Which path ran
is named every time.
"""

from __future__ import annotations

import os
from pathlib import Path

from sushicore import deps_fragment
from sushicore.deps_fragment import Dependency, Status
from sushicore.proc import Runner

from .. import console
from ..config import PROFILE, find_project_root, workspace_home

#: The workspace-wide installer this command hands off to.
HUB = "hub"

#: This repository's dependency fragment, relative to the project root.
FRAGMENT = Path("cli") / "sushistack.deps.toml"

_RUNNER = Runner(console, PROFILE.program)


def _platform() -> str:
    """Names the fragment column this machine installs from."""
    return deps_fragment.WINDOWS if os.name == "nt" else "linux"


def setup(install: bool = False) -> int:
    """Reports what this checkout needs to build, and installs it under *install*.

    Returns:
        0 when nothing was run or everything run succeeded, else the exit code
        of the first command that failed.
    """
    root = find_project_root()
    home = workspace_home()
    if home is not None:
        hub = _RUNNER.resolve_exe(HUB)
        if hub != HUB:
            return _delegate_to_hub(home, install)
        console.warn(
            f"A SushiStack workspace sits at {home}, but `{HUB}` is not on PATH, "
            f"so this checkout provisions itself.")
    return _provision_alone(root, install)


def _delegate_to_hub(home: Path, install: bool) -> int:
    """Hands provisioning to `hub install` in the workspace at *home*."""
    command = [HUB, "install"]
    console.info(
        f"Path: the SushiStack workspace at {home}. It owns one dependency tree for "
        f"every checkout under it, and this repository's fragment is part of it.")
    if not install:
        console.info(f"Reporting only. `{PROFILE.program} setup --install` runs: "
                     f"{' '.join(command)}")
        return 0
    return _RUNNER.run(command, home)


def _provision_alone(root: Path, install: bool) -> int:
    """Reads this repository's own fragment and reports, or installs, each entry."""
    console.info(f"Path: this repository alone, from {FRAGMENT.as_posix()}.")
    fragment = deps_fragment.read(root / FRAGMENT)
    platform = _platform()
    code = 0
    pending: list[list[str]] = []
    for dep in fragment.dependencies:
        command = _report(dep, platform)
        if command is not None:
            pending.append(command)
    if not install:
        if pending:
            console.info(f"Reporting only. `{PROFILE.program} setup --install` runs the "
                         f"{len(pending)} command(s) above.")
        return 0
    for command in pending:
        code = _RUNNER.run(command, root) or code
    return code


def _report(dep: Dependency, platform: str) -> list[str] | None:
    """Says what is known about *dep*, and returns the command that would install it.

    Returns:
        The install command when one exists and the entry cannot be called
        satisfied, else None.
    """
    need = "required" if dep.required else "optional"
    state = deps_fragment.status(dep, platform)
    if state is Status.SATISFIED:
        console.success(f"{dep.name} ({need}): its check passed.")
        return None
    if state is Status.NOT_APPLICABLE:
        console.info(f"{dep.name} ({need}): names no package on this platform, "
                     f"so there is nothing to install.")
        return None
    command = deps_fragment.install_command(dep, platform)
    if command is None:
        console.warn(f"{dep.name} ({need}): its check did not pass, and it names no "
                     f"package to install here.")
        return None
    console.warn(f"{dep.name} ({need}): unknown, and a package manager answers whether it "
                 f"is installed. Install it with: {' '.join(command)}")
    return command
