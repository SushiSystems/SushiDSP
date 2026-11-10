# cli.py
# SushiDSP - https://github.com/SushiSystems/SushiDSP
# Copyright (c) 2026-present Mustafa Garip & Sushi Systems
# Licensed under the Apache License 2.0. See LICENSE.
"""Declares the commands of `sd`, the SushiDSP developer CLI.

A thin Typer layer: each command parses its arguments and delegates to ``sushidsp.services``,
handing it the context object as its workspace; `sd` leaves it unset, so the services work on
the checkout it runs in. The root options, `config`, `env`, the `docs` group, the provision
commands and the entry point come from sushicore.
"""

from __future__ import annotations

from collections.abc import Callable, Sequence
from typing import Any, NoReturn, Optional, Protocol

import typer
from sushicore.diag_commands import register_diagnostic_commands
from sushicore.backlog import register_backlog_commands
from sushicore.docs_bundle import ApiSource, register_docs_commands
from sushicore.design import register_design_commands
from sushicore.entry import run as run_app
from sushicore.provision.commands import ModuleProvision, register_provision_commands
from sushicore.provision.steps import MachineActions
from sushicore.root_options import register_root_options
from sushicore.testing import (TestLabel, all_option, filter_option, jobs_option, label_option,
                               repeat_option)
from sushicore.typer_help import help_group

from . import console
from .aliases import ALIASES
from .config import PROFILE, find_project_root, load_config
from .services import diag as diag_svc
from .services import project as project_svc
from .services.project import BuildType, Sanitizer

K_PROGRAM = "sd"
K_DISTRIBUTION = "sushidsp-cli"

K_BUILD = "Build"
K_APPLICATION = "Application"
K_ENVIRONMENT = "Environment"

K_TYPE_HELP = ("Build type: debug | release | relwithdebinfo. Each type has its own tree, "
               "build/<name>-<type>. Default: debug.")
K_BENCH_TYPE_HELP = ("Build type: debug | release | relwithdebinfo. Default: release, the type "
                     "a timing is taken on.")
K_DEFINE_HELP = ("Pass a CMake cache variable to configure; on a configured tree it "
                 "reconfigures. Repeatable.")
K_CLEAN_HELP = "Delete the build tree before configuring."

K_API_XML = "build/docs/api-site/xml"

K_FORWARDING = {"allow_extra_args": True, "ignore_unknown_options": True}
K_BARE_RUN = "sd run"

def _type_option(description: str = K_TYPE_HELP) -> Any:
    """Returns the default of a `--type/-t` parameter."""
    return typer.Option(None, "--type", "-t", case_sensitive=False,
                        help=description)


def _define_option() -> Any:
    """Returns the default of a `--define/-D` parameter."""
    return typer.Option(None, "--define", "-D", metavar="VAR=VALUE", help=K_DEFINE_HELP)


def _sanitize_option() -> Any:
    """Returns the default of a `--sanitize` parameter."""
    return typer.Option(None, "--sanitize", case_sensitive=False,
                        help="Sanitizer tree: address (ASan and UBSan, build/asan-<type>) | "
                             "thread (TSan, build/tsan-<type>).")


def _clean_option() -> Any:
    """Returns the default of a `--clean` parameter."""
    return typer.Option(False, "--clean", help=K_CLEAN_HELP)


def configure(
    ctx: typer.Context,
    type: Optional[BuildType] = _type_option(),
    define: Optional[list[str]] = _define_option(),
    sanitize: Optional[Sanitizer] = _sanitize_option(),
) -> None:
    """Run cmake's configure step against the tree of the type, build/default-debug by default."""
    raise typer.Exit(project_svc.configure(type, define, sanitizer=sanitize,
                                          workspace=ctx.obj))


def build(
    ctx: typer.Context,
    type: Optional[BuildType] = _type_option(),
    clean: bool = _clean_option(),
    define: Optional[list[str]] = _define_option(),
    sanitize: Optional[Sanitizer] = _sanitize_option(),
    jobs: Optional[int] = typer.Option(
        None, "--jobs", "-j", min=1,
        help="Compile jobs. Default: the hardware threads, or CMAKE_BUILD_PARALLEL_LEVEL."),
    report: bool = typer.Option(
        False, "--report", help="Print the edges this build compiled by kind, with their time "
                                "and peak memory, and save the report in the tree."),
) -> None:
    """Configure if needed, then build every CMake target behind the memory gate.

    A cache variable set with -D stays in the build tree, so a later run without the
    flag keeps it; pass VAR= to clear one, or --clean to start over.
    """
    raise typer.Exit(project_svc.build(type, clean=clean, defines=define, sanitizer=sanitize,
                                       jobs=jobs, report=report, workspace=ctx.obj))


def test(
    ctx: typer.Context,
    type: Optional[BuildType] = _type_option(),
    label: Optional[list[TestLabel]] = label_option(),
    filter: Optional[str] = filter_option(),
    repeat: int = repeat_option(),
    jobs: Optional[int] = jobs_option(),
    all_sizes: bool = all_option(),
    clean: bool = _clean_option(),
    sanitize: Optional[Sanitizer] = _sanitize_option(),
) -> None:
    """Build if needed, then run the tests of the sizes and the filter, then print the budget line."""
    raise typer.Exit(project_svc.test(type, label or (), clean=clean, filter=filter,
                                      sanitizer=sanitize, jobs=jobs, repeat=repeat,
                                      every_size=all_sizes, workspace=ctx.obj))


def bench(
    ctx: typer.Context,
    group: Optional[list[str]] = typer.Argument(
        None, help="Substrings of the case groups to run. Default: every group."),
    type: Optional[BuildType] = _type_option(K_BENCH_TYPE_HELP),
    clean: bool = _clean_option(),
) -> None:
    """Build if needed, then run the benchmark program and print its table."""
    raise typer.Exit(project_svc.bench(type, group or (), clean=clean,
                                      workspace=ctx.obj))


def run(
    ctx: typer.Context,
    target: Optional[str] = typer.Argument(
        None, help="Executable name to run (exact, then substring match)."),
    sort: bool = typer.Option(
        False, "--sort", help="Interactively pick an executable."),
    type: Optional[BuildType] = _type_option(
        "Whose tree to run from: debug | release | relwithdebinfo. Default: debug."),
) -> None:
    """Run a built executable. Args after `--` are forwarded to it."""
    forwarded_only = target is not None and target.startswith("-")
    if forwarded_only or (target is None and not sort):
        table = ALIASES if ctx.obj is None else ctx.obj.aliases
        table.announce(table.alias_for(K_BARE_RUN))
        app_args = [target, *ctx.args] if forwarded_only else None
        raise typer.Exit(project_svc.host(type, app_args=app_args, workspace=ctx.obj))
    raise typer.Exit(project_svc.run(target=target, sort=sort, app_args=list(ctx.args),
                                     build_type=type, workspace=ctx.obj))


def clean(
    ctx: typer.Context,
    type: Optional[BuildType] = _type_option(
        "Which tree to remove: debug | release | relwithdebinfo. Default: debug."),
    sanitize: Optional[Sanitizer] = _sanitize_option(),
    everything: bool = typer.Option(
        False, "--all", help="Remove build/ with every tree, the gate's state and its cache."),
) -> None:
    """Remove one build tree, build/default-debug by default."""
    raise typer.Exit(project_svc.clean(type, sanitize, everything, workspace=ctx.obj))


def _api_source() -> ApiSource:
    """Returns how `sd` builds the API reference and where Doxygen leaves its XML."""
    return ApiSource(build=project_svc.docs, xml_dir=find_project_root() / K_API_XML)


def _docs_alias(ctx: typer.Context) -> None:
    """Builds the API reference; the callback behind the old spelling `sd doxygen`."""
    raise typer.Exit(project_svc.docs(workspace=ctx.obj))


def check(
    ctx: typer.Context,
    report: bool = typer.Option(
        False, "--report", "-r", help="Report issue counts without failing."),
    rule: Optional[str] = typer.Option(None, "--rule", help="Run only the specified rule."),
) -> None:
    """Run the repository's eight shared checkers under tools/."""
    raise typer.Exit(project_svc.check(report=report, rule=rule, workspace=ctx.obj))


def host(
    ctx: typer.Context,
    type: Optional[BuildType] = _type_option(),
    no_run: bool = typer.Option(
        False, "--no-run", help="Build the host GUI but do not launch it."),
) -> None:
    """Build if needed, then launch the standalone host GUI. Args after `--` reach it."""
    raise typer.Exit(project_svc.host(type, launch=not no_run, app_args=list(ctx.args),
                                      workspace=ctx.obj))


def module_provision(actions: MachineActions | None = None) -> ModuleProvision:
    """Returns what `sd` hands the shared provision commands.

    Args:
        actions: The calls the setup steps make that reach the machine; none makes the real ones.
    """
    return ModuleProvision(
        profile=PROFILE,
        project_root=find_project_root,
        load_config=load_config,
        console=lambda: console,
        panel=K_ENVIRONMENT,
        actions=actions,
    )


def make_app(console_provider: Callable[[], Any] = console.current) -> typer.Typer:
    """Builds the Typer application of `sd` with its help group drawn on a console.

    Args:
        console_provider: Returns the console a help page is drawn on; the module console, built on
            first use, when none is given.

    Returns:
        The application with every command registered.
    """
    group = help_group(console_provider)
    app = typer.Typer(
        name=K_PROGRAM,
        cls=group,
        help="SushiDSP developer CLI — configure, build, and test SushiDSP.",
        no_args_is_help=True,
        add_completion=False,
        rich_markup_mode="rich",
    )
    app.command(
        "configure",
        rich_help_panel=K_BUILD,
        epilog="sd configure\n"
               "sd configure --type release\n"
               "sd configure -D FOO=1  # Pass a CMake cache variable",
    )(configure)
    app.command(
        "build",
        rich_help_panel=K_BUILD,
        epilog="sd build\n"
               "sd build --type release\n"
               "sd build --clean  # Delete the build tree before configuring\n"
               "sd build -D FOO=1  # Reconfigure with a CMake cache variable, then build\n"
               "sd build --sanitize address  # ASan and UBSan in build/asan-debug; thread for TSan\n"
               "sd build --report  # Print the build's edges by kind and save the report",
    )(build)
    app.command(
        "test",
        rich_help_panel=K_BUILD,
        epilog="sd test  # Small and medium\n"
               "sd test --all  # Every size, the large tests included\n"
               "sd test --label small --label medium  # What a pull request runs\n"
               "sd test --filter 'Jcm800.*'  # Match Suite.Case test names\n"
               "sd test --label small --sanitize address  # Small tests under ASan and UBSan",
    )(test)
    app.command(
        "bench",
        rich_help_panel=K_BUILD,
        epilog="sd bench  # Every group\n"
               "sd bench Room Jcm800  # Groups whose name holds an argument",
    )(bench)
    app.command(
        "run",
        context_settings=K_FORWARDING,
        rich_help_panel=K_BUILD,
        epilog="sd run sushidsp_tests\n"
               "sd run --sort  # Pick an executable from a list\n"
               "sd run sushidsp_tests -- --gtest_filter='Jcm800.*'  # Flags after -- go to it",
    )(run)
    app.command(
        "clean",
        rich_help_panel=K_BUILD,
        epilog="sd clean  # build/default-debug\n"
               "sd clean --type release\n"
               "sd clean --sanitize address  # build/asan-debug\n"
               "sd clean --all  # The whole build/ directory",
    )(clean)
    register_docs_commands(
        app,
        program=K_PROGRAM,
        panel=K_BUILD,
        project_root=find_project_root,
        api=_api_source,
        report=lambda line: console.success(line),
        group_cls=group,
    )
    register_backlog_commands(
        app,
        program=K_PROGRAM,
        panel=K_BUILD,
        project_root=find_project_root,
        group_cls=group,
    )
    register_design_commands(
        app,
        program=K_PROGRAM,
        panel=K_BUILD,
        project_root=find_project_root,
        group_cls=group,
    )
    app.command(
        "check",
        rich_help_panel=K_BUILD,
        epilog="sd check\n"
               "sd check --report  # Report issue counts without failing\n"
               "sd check --rule <name>  # Run only the specified rule",
    )(check)
    register_diagnostic_commands(
        app, diag_svc.DIAGNOSTICS, program=K_PROGRAM, panel=K_ENVIRONMENT)
    app.command(
        "host",
        context_settings=K_FORWARDING,
        rich_help_panel=K_APPLICATION,
        epilog="sd host\n"
               "sd host --type release\n"
               "sd host --no-run  # Build only\n"
               "sd host -- --foo  # Arguments after -- reach the host GUI",
    )(host)
    ALIASES.command(app, "doxygen", _docs_alias, "sd doxygen")

    register_root_options(app, distribution=K_DISTRIBUTION)
    register_provision_commands(app, module_provision())
    return app


app = make_app()


class ErrorConsole(Protocol):
    """What `main` prints a failure through."""

    def error(self, text: str) -> None:
        """Prints an error line."""


def main(argv: Sequence[str] | None = None, error_console: ErrorConsole | None = None) -> NoReturn:
    """Runs `sd` and exits; a failure sushicore reports becomes one line and exit code 1.

    Args:
        argv: The arguments to parse; the process's own when None.
        error_console: Where the failure line is printed; the module console, looked up when
            the failure happens, when None.
    """
    run_app(app, lambda message: (error_console or console).error(message), argv=argv)


if __name__ == "__main__":
    main()
