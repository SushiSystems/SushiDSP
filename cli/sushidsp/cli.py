"""SushiDSP developer CLI (`sd`).

Thin Typer layer: commands only parse arguments and delegate to the service
layer in ``sushidsp.services``.
"""

from __future__ import annotations

from typing import Optional

import typer

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
    type: BuildType = typer.Option(
        BuildType.debug, "--type", "-t", case_sensitive=False,
        help="Build type: debug | release | relwithdebinfo."),
    define: Optional[list[str]] = typer.Option(
        None, "--define", "-D", metavar="VAR=VALUE",
        help="Pass a CMake cache variable through to configure. Repeatable."),
):
    """Runs cmake's configure step against build/."""
    raise typer.Exit(project_svc.configure(type, define))


@app.command("build")
def build(
    type: BuildType = typer.Option(
        BuildType.debug, "--type", "-t", case_sensitive=False,
        help="Build type: debug | release | relwithdebinfo."),
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
    type: BuildType = typer.Option(
        BuildType.debug, "--type", "-t", case_sensitive=False,
        help="Build type: debug | release | relwithdebinfo."),
    suite: Suite = typer.Option(
        Suite.unit, "--suite", "-s", case_sensitive=False,
        help="Test suite to run: unit | all."),
    clean: bool = typer.Option(
        False, "--clean", help="Delete the build tree before configuring."),
):
    """Builds (if needed) and runs ctest, optionally filtered by --suite."""
    raise typer.Exit(project_svc.test(type, suite, clean=clean))


@app.command("config")
def config():
    """Print the resolved configuration and where each value came from."""
    raise typer.Exit(diag_svc.config_show())


@app.command("doxygen")
def doxygen():
    """Generates the API reference (docs/api-site/html) from the headers."""
    raise typer.Exit(project_svc.doxygen())
