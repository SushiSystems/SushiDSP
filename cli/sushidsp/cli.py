# cli.py
# SushiDSP - https://github.com/SushiSystems/SushiDSP
# Copyright (c) 2026 Sushi Systems
# Licensed under PolyForm Noncommercial 1.0.0. See LICENSE.
# Commercial use requires a licence from Sushi Systems.
"""Declares the commands of `sd`, the SushiDSP developer CLI.

A thin Typer layer: each command parses its arguments and delegates to
``sushidsp.services``. The root options, `config`, `env`, the provision commands and
the entry point come from sushicore.
"""

from __future__ import annotations

from typing import Any, Optional

import typer
from sushicore.diag_commands import register_diagnostic_commands
from sushicore.entry import run as run_app
from sushicore.provision.commands import ModuleProvision, register_provision_commands
from sushicore.root_options import register_root_options
from sushicore.typer_help import help_group

from . import console
from .aliases import ALIASES
from .config import PROFILE, find_project_root, load_config
from .services import diag as diag_svc
from .services import project as project_svc
from .services.project import BuildType, Suite

K_PROGRAM = "sd"
K_DISTRIBUTION = "sushidsp-cli"

K_BUILD = "Build"
K_APPLICATION = "Application"
K_ENVIRONMENT = "Environment"

K_TYPE_HELP = ("Build type: debug | release | relwithdebinfo. "
               "Defaults to the type the build tree already carries, else debug.")
K_DEFINE_HELP = ("Pass a CMake cache variable to configure; on a configured tree it "
                 "reconfigures. Repeatable.")
K_CLEAN_HELP = "Delete the build tree before configuring."

K_FORWARDING = {"allow_extra_args": True, "ignore_unknown_options": True}
K_BARE_RUN = "sd run"

_help_group = help_group(console.current)

app = typer.Typer(
    name=K_PROGRAM,
    cls=_help_group,
    help="SushiDSP developer CLI — configure, build, and test SushiDSP.",
    no_args_is_help=True,
    add_completion=False,
    rich_markup_mode="rich",
)


def _type_option() -> Any:
    """Returns the default of a `--type/-t` parameter."""
    return typer.Option(None, "--type", "-t", case_sensitive=False, help=K_TYPE_HELP)


def _define_option() -> Any:
    """Returns the default of a `--define/-D` parameter."""
    return typer.Option(None, "--define", "-D", metavar="VAR=VALUE", help=K_DEFINE_HELP)


def _clean_option() -> Any:
    """Returns the default of a `--clean` parameter."""
    return typer.Option(False, "--clean", help=K_CLEAN_HELP)


@app.command(
    "configure",
    rich_help_panel=K_BUILD,
    epilog="sd configure\n"
           "sd configure --type release\n"
           "sd configure -D FOO=1  # Pass a CMake cache variable",
)
def configure(
    type: Optional[BuildType] = _type_option(),
    define: Optional[list[str]] = _define_option(),
) -> None:
    """Run cmake's configure step against build/."""
    raise typer.Exit(project_svc.configure(type, define))


@app.command(
    "build",
    rich_help_panel=K_BUILD,
    epilog="sd build\n"
           "sd build --type release\n"
           "sd build --clean  # Delete the build tree before configuring\n"
           "sd build -D FOO=1  # Reconfigure with a CMake cache variable, then build",
)
def build(
    type: Optional[BuildType] = _type_option(),
    clean: bool = _clean_option(),
    define: Optional[list[str]] = _define_option(),
) -> None:
    """Configure if needed, then build every CMake target.

    A cache variable set with -D stays in the build tree, so a later run without the
    flag keeps it; pass VAR= to clear one, or --clean to start over.
    """
    raise typer.Exit(project_svc.build(type, clean=clean, defines=define))


@app.command(
    "test",
    rich_help_panel=K_BUILD,
    epilog="sd test\n"
           "sd test --suite regression\n"
           "sd test --filter 'Unit_Dsp.*'  # Match Suite.Case test names\n"
           "sd test --clean  # Delete the build tree before configuring",
)
def test(
    type: Optional[BuildType] = _type_option(),
    suite: Suite = typer.Option(
        Suite.unit, "--suite", "-s", case_sensitive=False,
        help="Test suite to run: unit | regression | benchmark | all."),
    filter: Optional[str] = typer.Option(
        None, "--filter", "-f", help="ctest -R regex over 'Suite.Case' test names."),
    clean: bool = _clean_option(),
) -> None:
    """Build if needed, then run ctest over the suite and the filter."""
    raise typer.Exit(project_svc.test(type, suite, clean=clean, filter=filter))


@app.command(
    "run",
    context_settings=K_FORWARDING,
    rich_help_panel=K_BUILD,
    epilog="sd run sushidsp_tests\n"
           "sd run --sort  # Pick an executable from a list\n"
           "sd run sushidsp_tests -- --gtest_filter='Unit_Dsp.*'  # Flags after -- go to it",
)
def run(
    ctx: typer.Context,
    target: Optional[str] = typer.Argument(
        None, help="Executable name to run (exact, then substring match)."),
    sort: bool = typer.Option(
        False, "--sort", help="Interactively pick an executable."),
    type: Optional[BuildType] = typer.Option(
        None, "--type", "-t", case_sensitive=False, hidden=True,
        help="The old spelling of `sd host --type`."),
) -> None:
    """Run a built executable. Args after `--` are forwarded to it."""
    forwarded_only = target is not None and target.startswith("-")
    if forwarded_only or (target is None and not sort):
        ALIASES.announce(ALIASES.alias_for(K_BARE_RUN))
        app_args = [target, *ctx.args] if forwarded_only else None
        raise typer.Exit(project_svc.host(type, app_args=app_args))
    if type is not None:
        raise typer.BadParameter(
            "it selects what `sd host` builds; `sd run TARGET` runs what the tree holds.",
            param_hint="--type")
    raise typer.Exit(project_svc.run(target=target, sort=sort, app_args=list(ctx.args)))


@app.command(
    "clean",
    rich_help_panel=K_BUILD,
    epilog="sd clean",
)
def clean() -> None:
    """Remove the build/ tree."""
    raise typer.Exit(project_svc.clean())


@app.command(
    "docs",
    rich_help_panel=K_BUILD,
    epilog="sd docs",
)
def docs() -> None:
    """Generate the API reference (build/docs/api-site/html) from the headers."""
    raise typer.Exit(project_svc.docs())


@app.command(
    "check",
    rich_help_panel=K_BUILD,
    epilog="sd check\n"
           "sd check --report  # Report issue counts without failing\n"
           "sd check --rule <name>  # Run only the specified rule",
)
def check(
    report: bool = typer.Option(
        False, "--report", "-r", help="Report issue counts without failing."),
    rule: Optional[str] = typer.Option(None, "--rule", help="Run only the specified rule."),
) -> None:
    """Run the repository's layout, changelog and layering checkers."""
    raise typer.Exit(project_svc.check(report=report, rule=rule))


register_diagnostic_commands(
    app, diag_svc.DIAGNOSTICS, program=K_PROGRAM, panel=K_ENVIRONMENT)


@app.command(
    "host",
    context_settings=K_FORWARDING,
    rich_help_panel=K_APPLICATION,
    epilog="sd host\n"
           "sd host --type release\n"
           "sd host --no-run  # Build only\n"
           "sd host -- --foo  # Arguments after -- reach the host GUI",
)
def host(
    ctx: typer.Context,
    type: Optional[BuildType] = _type_option(),
    no_run: bool = typer.Option(
        False, "--no-run", help="Build the host GUI but do not launch it."),
) -> None:
    """Build if needed, then launch the standalone host GUI. Args after `--` reach it."""
    raise typer.Exit(project_svc.host(type, launch=not no_run, app_args=list(ctx.args)))


ALIASES.command(app, "doxygen", docs, "sd doxygen")

register_root_options(app, distribution=K_DISTRIBUTION)

register_provision_commands(app, ModuleProvision(
    profile=PROFILE,
    project_root=find_project_root,
    load_config=load_config,
    console=lambda: console,
    panel=K_ENVIRONMENT,
))


def _report_error(message: str) -> None:
    """Prints one error line through the console, which is built only at this call."""
    console.error(message)


def main() -> None:
    """Runs `sd` and exits; a failure sushicore reports becomes one line and exit code 1."""
    run_app(app, _report_error)


if __name__ == "__main__":
    main()
