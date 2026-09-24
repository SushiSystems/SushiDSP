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

from . import console
from .config import PROFILE, find_project_root, load_config
from .services import diag as diag_svc
from .services import project as project_svc
from .services.project import BuildType, Suite

app = typer.Typer(
    name="sd",
    help="SushiDSP developer CLI — configure, build, and test SushiDSP.",
    no_args_is_help=True,
    add_completion=False,
    rich_markup_mode="rich",
)


@app.command("configure")
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


@app.command("build")
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


@app.command("test")
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


@app.command("config")
def config():
    """Print the resolved configuration and where each value came from."""
    raise typer.Exit(diag_svc.config_show())


@app.command("env")
def env(
    all: bool = typer.Option(False, "--all", "-a", help="Show every variable, not just the build-relevant ones."),
):
    """Print the environment the build subprocesses run with."""
    raise typer.Exit(diag_svc.env_dump(show_all=all))


@app.command("doxygen")
def doxygen():
    """Generates the API reference (docs/api-site/html) from the headers."""
    raise typer.Exit(project_svc.doxygen())


@app.command("check")
def check(
    report: bool = typer.Option(False, "--report", "-r", help="Report issue counts without failing."),
    rule: Optional[str] = typer.Option(None, "--rule", help="Run only the specified rule."),
):
    """Runs repository layout, changelog, and layering checkers."""
    raise typer.Exit(project_svc.check(report=report, rule=rule))


@app.command("run")
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
))
