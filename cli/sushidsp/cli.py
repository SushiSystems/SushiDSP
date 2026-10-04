# Copyright (c) 2026-present Mustafa Garip & Sushi Systems
# Licensed under the Apache License, Version 2.0. See LICENSE.
"""SushiDSP developer CLI (`sd`).

Thin Typer layer: commands only parse arguments and delegate to the service
layer in ``sushidsp.services``.
"""

from __future__ import annotations

from typing import Optional

import typer
from sushicore.provision.commands import ModuleProvision, register_provision_commands
from sushicore.typer_help import help_group

from . import console
from .config import PROFILE, find_project_root, load_config
from .services import diag as diag_svc
from .services import project as project_svc
from .services.project import BuildType, Suite

K_BUILD = "Build"
K_APPLICATION = "Application"
K_ENVIRONMENT = "Environment"

_help_group = help_group(console.current)

app = typer.Typer(
    name="sd",
    cls=_help_group,
    help="SushiDSP developer CLI — configure, build, and test SushiDSP.",
    no_args_is_help=True,
    add_completion=False,
    rich_markup_mode="rich",
)


@app.command(
    "configure",
    rich_help_panel=K_BUILD,
    epilog="sd configure\n"
           "sd configure --type release\n"
           "sd configure -D FOO=1  # Pass a CMake cache variable",
)
def configure(
    type: Optional[BuildType] = typer.Option(
        None, "--type", "-t", case_sensitive=False,
        help="Build type: debug | release | relwithdebinfo. "
             "Defaults to the type the build tree already carries, else debug."),
    define: Optional[list[str]] = typer.Option(
        None, "--define", "-D", metavar="VAR=VALUE",
        help="Pass a CMake cache variable through to configure. Repeatable."),
):
    """Runs cmake's configure step against build/."""
    raise typer.Exit(project_svc.configure(type, define))


@app.command(
    "build",
    rich_help_panel=K_BUILD,
    epilog="sd build\n"
           "sd build --type release\n"
           "sd build --clean  # Delete the build tree before configuring",
)
def build(
    type: Optional[BuildType] = typer.Option(
        None, "--type", "-t", case_sensitive=False,
        help="Build type: debug | release | relwithdebinfo. "
             "Defaults to the type the build tree already carries, else debug."),
    clean: bool = typer.Option(
        False, "--clean", help="Delete the build tree before configuring."),
    define: Optional[list[str]] = typer.Option(
        None, "--define", "-D", metavar="VAR=VALUE",
        help="Pass a CMake cache variable through to configure. Repeatable."),
):
    """Configures (if needed) and builds every CMake target."""
    raise typer.Exit(project_svc.build(type, clean=clean, defines=define))


@app.command(
    "test",
    rich_help_panel=K_BUILD,
    epilog="sd test\n"
           "sd test --suite regression\n"
           "sd test --clean  # Delete the build tree before configuring",
)
def test(
    type: Optional[BuildType] = typer.Option(
        None, "--type", "-t", case_sensitive=False,
        help="Build type: debug | release | relwithdebinfo. "
             "Defaults to the type the build tree already carries, else debug."),
    suite: Suite = typer.Option(
        Suite.unit, "--suite", "-s", case_sensitive=False,
        help="Test suite to run: unit | regression | benchmark | all."),
    clean: bool = typer.Option(
        False, "--clean", help="Delete the build tree before configuring."),
):
    """Builds (if needed) and runs ctest, optionally filtered by --suite."""
    raise typer.Exit(project_svc.test(type, suite, clean=clean))


@app.command(
    "config",
    rich_help_panel=K_ENVIRONMENT,
    epilog="sd config",
)
def config():
    """Print the resolved configuration and where each value came from."""
    raise typer.Exit(diag_svc.config_show())


@app.command(
    "env",
    rich_help_panel=K_ENVIRONMENT,
    epilog="sd env\n"
           "sd env --all  # Show every variable, not just the build-relevant ones",
)
def env(
    all: bool = typer.Option(False, "--all", "-a", help="Show every variable, not just the build-relevant ones."),
):
    """Print the environment the build subprocesses run with."""
    raise typer.Exit(diag_svc.env_dump(show_all=all))


@app.command(
    "doxygen",
    rich_help_panel=K_BUILD,
    epilog="sd doxygen",
)
def doxygen():
    """Generates the API reference (docs/api-site/html) from the headers."""
    raise typer.Exit(project_svc.doxygen())


@app.command(
    "check",
    rich_help_panel=K_BUILD,
    epilog="sd check\n"
           "sd check --report  # Report issue counts without failing\n"
           "sd check --rule <name>  # Run only the specified rule",
)
def check(
    report: bool = typer.Option(False, "--report", "-r", help="Report issue counts without failing."),
    rule: Optional[str] = typer.Option(None, "--rule", help="Run only the specified rule."),
):
    """Runs repository layout, changelog, and layering checkers."""
    raise typer.Exit(project_svc.check(report=report, rule=rule))


@app.command(
    "run",
    rich_help_panel=K_APPLICATION,
    epilog="sd run\n"
           "sd run --type release\n"
           "sd run -- --foo  # Pass extra arguments to the host GUI",
)
def run(
    type: Optional[BuildType] = typer.Option(
        None, "--type", "-t", case_sensitive=False,
        help="Build type: debug | release | relwithdebinfo. "
             "Defaults to the type the build tree already carries, else debug."),
    extra_args: Optional[list[str]] = typer.Argument(
        None, help="Arguments passed through directly to sushidsp_host_gui."),
):
    """Runs the SushiDSP standalone host GUI application."""
    raise typer.Exit(project_svc.run(type, args=extra_args))


register_provision_commands(app, ModuleProvision(
    profile=PROFILE,
    project_root=find_project_root,
    load_config=load_config,
    console=lambda: console,
    panel=K_ENVIRONMENT,
))
