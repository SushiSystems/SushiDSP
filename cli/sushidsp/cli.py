# cli.py
# SushiDSP - https://github.com/SushiSystems/SushiDSP
# Copyright (c) 2026-present Mustafa Garip & Sushi Systems
# Licensed under PolyForm Noncommercial 1.0.0. See LICENSE.
# Commercial use requires a licence from Sushi Systems.
"""Declares the commands of `sd`, the SushiDSP developer CLI.

A thin Typer layer: each command parses its arguments and delegates to
``sushidsp.services``. The root options, `config`, `env`, the `docs` group, the provision
commands and the entry point come from sushicore.
"""

from __future__ import annotations

from typing import Any, Optional

import typer
from sushicore.diag_commands import register_diagnostic_commands
from sushicore.docs_bundle import ApiSource, register_docs_commands
from sushicore.entry import run as run_app
from sushicore.provision.commands import ModuleProvision, register_provision_commands
from sushicore.root_options import register_root_options
from sushicore.typer_help import help_group

from . import console
from .aliases import ALIASES
from .config import PROFILE, find_project_root, load_config
from .services import diag as diag_svc
from .services import project as project_svc
from .services.project import BuildType, Sanitizer, Size

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

K_API_XML = "build/docs/api-site/xml"

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


def _sanitize_option() -> Any:
    """Returns the default of a `--sanitize` parameter."""
    return typer.Option(None, "--sanitize", case_sensitive=False,
                        help="Sanitizer build: address (ASan and UBSan) | thread (TSan).")


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
           "sd build -D FOO=1  # Reconfigure with a CMake cache variable, then build\n"
           "sd build --sanitize address  # ASan and UBSan; thread for TSan",
)
def build(
    type: Optional[BuildType] = _type_option(),
    clean: bool = _clean_option(),
    define: Optional[list[str]] = _define_option(),
    sanitize: Optional[Sanitizer] = _sanitize_option(),
) -> None:
    """Configure if needed, then build every CMake target.

    A cache variable set with -D stays in the build tree, so a later run without the
    flag keeps it; pass VAR= to clear one, or --clean to start over.
    """
    raise typer.Exit(project_svc.build(type, clean=clean, defines=define, sanitizer=sanitize))


@app.command(
    "test",
    rich_help_panel=K_BUILD,
    epilog="sd test  # Every size\n"
           "sd test --label small --label medium  # What a pull request runs\n"
           "sd test --filter 'Jcm800.*'  # Match Suite.Case test names\n"
           "sd test --label small --sanitize address  # Small tests under ASan and UBSan",
)
def test(
    type: Optional[BuildType] = _type_option(),
    label: Optional[list[Size]] = typer.Option(
        None, "--label", "-l", case_sensitive=False,
        help="Test size to run: small | medium | large; repeat for more. Default: every size."),
    filter: Optional[str] = typer.Option(
        None, "--filter", "-f", help="ctest -R regex over 'Suite.Case' test names."),
    jobs: Optional[int] = typer.Option(
        None, "--jobs", "-j", min=1,
        help="Tests run at once. When omitted ctest decides, and CTEST_PARALLEL_LEVEL is "
             "honoured."),
    clean: bool = _clean_option(),
    sanitize: Optional[Sanitizer] = _sanitize_option(),
) -> None:
    """Build if needed, then run ctest over the sizes and the filter."""
    raise typer.Exit(project_svc.test(type, label or (), clean=clean, filter=filter,
                                      sanitizer=sanitize, jobs=jobs))


@app.command(
    "bench",
    rich_help_panel=K_BUILD,
    epilog="sd bench  # Every group\n"
           "sd bench Room Jcm800  # Groups whose name holds an argument",
)
def bench(
    group: Optional[list[str]] = typer.Argument(
        None, help="Substrings of the case groups to run. Default: every group."),
    type: Optional[BuildType] = _type_option(),
    clean: bool = _clean_option(),
) -> None:
    """Build if needed, then run the benchmark program and print its table."""
    raise typer.Exit(project_svc.bench(type, group or (), clean=clean))


@app.command(
    "run",
    context_settings=K_FORWARDING,
    rich_help_panel=K_BUILD,
    epilog="sd run sushidsp_tests\n"
           "sd run --sort  # Pick an executable from a list\n"
           "sd run sushidsp_tests -- --gtest_filter='Jcm800.*'  # Flags after -- go to it",
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


def _api_source() -> ApiSource:
    """Returns how `sd` builds the API reference and where Doxygen leaves its XML."""
    return ApiSource(build=project_svc.docs, xml_dir=find_project_root() / K_API_XML)


def _docs_alias() -> None:
    """Builds the API reference; the callback behind the old spelling `sd doxygen`."""
    raise typer.Exit(project_svc.docs())


register_docs_commands(
    app,
    program=K_PROGRAM,
    panel=K_BUILD,
    project_root=find_project_root,
    api=_api_source,
    report=lambda line: console.success(line),
    group_cls=_help_group,
)


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
    """Run the repository's eight shared checkers under tools/."""
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


ALIASES.command(app, "doxygen", _docs_alias, "sd doxygen")

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
