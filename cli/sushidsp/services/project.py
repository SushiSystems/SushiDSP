# project.py
# SushiDSP - https://github.com/SushiSystems/SushiDSP
# Copyright (c) 2026-present Mustafa Garip & Sushi Systems
# Licensed under PolyForm Noncommercial 1.0.0. See LICENSE.
# Commercial use requires a licence from Sushi Systems.
"""Drives SushiDSP's configure, build, test, clean, docs, check, host and run steps.

Holds the build policy: the cache variables, the tree each build type and sanitizer owns, the test
sizes and their CTest labels, the checkers and the host GUI's place in the tree. cmake and ctest
run through ``sushicore.cmake_driver.CMakeDriver`` under the environment ``sushidsp.env``
assembles; every compile and link goes through sushicore's memory gate.
"""

from __future__ import annotations

import enum
import os
import shutil
import stat
import subprocess
import sys
from collections.abc import Sequence
from pathlib import Path

from sushicore import build_admission, build_report, cmake_cache
from sushicore.build_tree import K_DEFAULT_BUILD_TYPE, refuse_another_build_type, tree_dir
from sushicore.cmake_driver import CMakeDriver
from sushicore.proc import Runner
from sushicore.provision.sushi_packages import PackageError, PackageSet
from sushicore.toolchain_args import c_compiler_for

from .. import console
from ..config import Config, PROFILE, find_project_root, load_config
from ..env import load_build_env
from . import discovery, library_packages


class BuildType(str, enum.Enum):
    """Names the build types `--type` accepts."""

    debug = "debug"
    release = "release"
    relwithdebinfo = "relwithdebinfo"


class Size(str, enum.Enum):
    """Names the test sizes `--label` accepts; each is the CTest label tests/CMakeLists.txt sets."""

    small = "small"
    medium = "medium"
    large = "large"


class Sanitizer(str, enum.Enum):
    """Names the sanitizer builds `--sanitize` accepts; each is a SUSHIDSP_SANITIZE value."""

    address = "address"
    thread = "thread"


_BENCHMARK = "sushidsp_bench"


def size_label_regex(sizes: Sequence[Size]) -> str | None:
    """Returns the ctest -L regex that selects *sizes*, or None for every size."""
    if not sizes:
        return None
    return "^(" + "|".join(sorted({size.value for size in sizes})) + ")$"


def sanitizer_defines(sanitizer: Sanitizer | None) -> list[str]:
    """Returns the cache variable that turns *sanitizer* on, or nothing."""
    return [f"SUSHIDSP_SANITIZE={sanitizer.value}"] if sanitizer is not None else []

_CMAKE_BUILD_TYPE = {
    BuildType.debug: "Debug",
    BuildType.release: "Release",
    BuildType.relwithdebinfo: "RelWithDebInfo",
}

#: The type a command builds when --type is absent.
K_DEFAULT_TYPE = BuildType(K_DEFAULT_BUILD_TYPE.lower())

#: The type the benchmark builds when --type is absent: a timing is taken on what ships.
K_BENCHMARK_TYPE = BuildType.release

#: The name of the tree each sanitizer owns; the plain tree is `default`.
_TREE_NAME = {None: "default", Sanitizer.address: "asan", Sanitizer.thread: "tsan"}

_CHECKERS = (
    Path("tools", "documentation", "check_docs_layout.py"),
    Path("tools", "documentation", "check_changelog.py"),
    Path("tools", "documentation", "check_docs_truth.py"),
    Path("tools", "documentation", "check_source_comments.py"),
    Path("tools", "layering", "check_layering.py"),
    Path("tools", "testing", "check_test_mirror.py"),
    Path("tools", "testing", "check_test_sizes.py"),
    Path("tools", "logs", "check_logging.py"),
)

_HOST_GUI = "sushidsp_host_gui"
_HOST_GUI_FOLDERS = (Path("applications", "host_gui"), Path("bin"))

_DOXYGEN_HINT = (
    "  - Windows: winget install DimitriVanHeesch.Doxygen\n"
    "  - Linux:   apt-get install -y doxygen graphviz\n"
    "  - macOS:   brew install doxygen graphviz\n"
    "  - or set doxygen_exe in cli/config.local.toml to an existing doxygen binary.")

_RUNNER = Runner(console, PROFILE.program)
_APP_RUNNER = Runner(console, PROFILE.program, catch_interrupt=True)
_DRIVER = CMakeDriver(console, _RUNNER)


def _build_dir(root: Path, build_type: BuildType, sanitizer: Sanitizer | None = None) -> Path:
    """Returns `build/<name>-<type>` of the checkout at *root*, the one tree of that pair."""
    return tree_dir(root, _TREE_NAME[sanitizer], _CMAKE_BUILD_TYPE[build_type])


def _env(cfg: Config, build_dir: Path) -> dict[str, str]:
    """Returns the environment cmake, ctest and the host GUI run under.

    The run-time folders of the library packages the tree linked go first on the search path.
    """
    env = load_build_env(cfg, build_dir)
    library_packages.prepend_run_dirs(cfg, env, library_packages.read_run_dirs(build_dir))
    return env


def _link_packages(cfg: Config, root: Path, build_dir: Path,
                   build_type: BuildType) -> PackageSet | None:
    """Returns the library packages of this build and records their run-time folders.

    Returns:
        None, after one error line, when a package cannot be chosen or built.
    """
    try:
        packages = library_packages.resolve(cfg, root, build_type.value, console.info,
                                            console.warn)
    except PackageError as error:
        console.error(str(error))
        return None
    library_packages.record_run_dirs(build_dir, packages.run_dirs(cfg.platform))
    return packages


def resolve_build_type(requested: BuildType | None,
                       default: BuildType = K_DEFAULT_TYPE) -> BuildType:
    """Settles which build type a command acts on: the requested one, else *default*."""
    return requested if requested is not None else default


def _generator_changed(cfg: Config, build_dir: Path) -> bool:
    """Tells whether the tree was configured with a generator other than the one resolved now."""
    cached = cmake_cache.cached_value(build_dir, "CMAKE_GENERATOR")
    if cached is None:
        return False
    wanted = cfg.resolved_generator()
    if wanted == "":
        return not cached.startswith(("Visual Studio", "Unix Makefiles", "Xcode"))
    return cached != wanted


def _remove_tree(build_dir: Path) -> bool:
    """Removes the build tree, read-only files included, and reports a failure in one line.

    Returns:
        False when a file in the tree could not be removed.
    """
    def make_writable_and_retry(func, target, _exc_info) -> None:
        """Clears the read-only bit FetchContent's git objects carry and retries once."""
        os.chmod(target, stat.S_IWRITE)
        func(target)

    console.info(f"Removing {build_dir} ...")
    try:
        shutil.rmtree(build_dir, onerror=make_writable_and_retry)
    except OSError as error:
        console.error(
            f"Could not remove {error.filename or build_dir} ({error.strerror}); {build_dir} "
            "is left partly removed. Close the program that holds the file and run again.")
        return False
    return True


def _needs_configure(cfg: Config, build_dir: Path) -> bool:
    """Tells whether the tree must be configured before it can be compiled.

    CMake's platform default, the generator left empty when no compiler is found, leaves no
    generator sentinel, so the driver cannot judge it and the cache file stands in.
    """
    generator = cfg.resolved_generator()
    if generator:
        return _DRIVER.needs_configure(build_dir, generator)
    return not (build_dir / "CMakeCache.txt").is_file()


def _refuse_a_wrong_start(build_dir: Path, build_type: BuildType,
                          defines: Sequence[str] | None) -> None:
    """Raises when *build_dir* holds another build type or the memory gate is stopped."""
    refuse_another_build_type(build_dir, _CMAKE_BUILD_TYPE[build_type], defines or ())
    build_admission.refuse_a_stopped_gate(build_dir)


def _print_report(build_dir: Path, offset: int) -> None:
    """Prints the report of the edges the build wrote after *offset* and saves it in the tree."""
    report = build_report.of_build(build_dir, offset, build_admission.state_dir(build_dir))
    for line in report.lines():
        console.info(line)
    console.info(f"Report: {report.write(build_dir)}")


def configure(build_type: BuildType | None, defines: Sequence[str] | None = None,
              packages: PackageSet | None = None, sanitizer: Sanitizer | None = None) -> int:
    """Runs `cmake -S . -B build/<name>-<type>` with the detected compiler and generator.

    SushiLog is linked as a built package: its config folder reaches CMake as
    `-DSushiLog_DIR`, and no checkout directory does. Every compile and link of the tree
    goes through the memory gate, which the launcher entries below name.

    Args:
        defines: `VAR=VALUE` cache variables, passed after every flag this function sets.
        packages: The packages `build` already resolved; resolved here when None.
        sanitizer: Configures the tree that sanitizer owns.

    Raises:
        sushicore.errors.ConfigError: The tree holds another build type, or *defines* name one.
        sushicore.build_admission.BuildStoppedError: The gate's stop file exists.
    """
    build_type = resolve_build_type(build_type)
    root = find_project_root()
    cfg = load_config()
    build_dir = _build_dir(root, build_type, sanitizer)
    _refuse_a_wrong_start(build_dir, build_type, defines)
    build_dir.mkdir(parents=True, exist_ok=True)
    if packages is None:
        packages = _link_packages(cfg, root, build_dir, build_type)
        if packages is None:
            return 1

    cmd = [
        _DRIVER.cmake(cfg), "-S", str(root), "-B", str(build_dir),
        f"-DCMAKE_BUILD_TYPE={_CMAKE_BUILD_TYPE[build_type]}",
    ]
    generator = cfg.resolved_generator()
    if generator:
        cmd += ["-G", generator]
    if generator == "Ninja":
        cmd += [f"-DCMAKE_MAKE_PROGRAM={cfg.resolved_ninja()}"]
    compiler = cfg.resolved_compiler()
    if compiler:
        console.info(f"Compiler: {compiler}  [{cfg.compiler_source()}]")
        cmd += [f"-DCMAKE_CXX_COMPILER={compiler}",
                f"-DCMAKE_C_COMPILER={c_compiler_for(compiler)}"]
    else:
        console.info(f"Compiler: {cfg.compiler_source()}")
    cmd += library_packages.configure_arguments(cfg, root, packages)
    gate = build_admission.launcher_defines(
        build_dir, build_admission.compiler_cache(cfg.ccache_exe))
    for define in (*gate, *sanitizer_defines(sanitizer), *(defines or ())):
        cmd += ["-D", define]

    return _DRIVER.configure(cmd, cwd=root, env=_env(cfg, build_dir))


def build(build_type: BuildType | None, clean: bool = False,
          defines: Sequence[str] | None = None, sanitizer: Sanitizer | None = None,
          jobs: int | None = None, report: bool = False) -> int:
    """Configures when the tree needs it or *defines* are given, then compiles every target.

    Args:
        clean: Removes the build tree first.
        defines: `VAR=VALUE` cache variables; any of them reconfigures a configured tree.
        sanitizer: Builds the tree that sanitizer owns.
        jobs: Compile jobs; none uses the hardware threads.
        report: Prints the edges this build compiled and saves the report in the tree.

    Raises:
        sushicore.errors.ConfigError: The tree holds another build type, or *defines* name one.
        sushicore.build_admission.BuildStoppedError: The gate's stop file exists.
    """
    build_type = resolve_build_type(build_type)
    root = find_project_root()
    cfg = load_config()
    build_dir = _build_dir(root, build_type, sanitizer)
    _refuse_a_wrong_start(build_dir, build_type, defines)

    if build_dir.exists() and (clean or _generator_changed(cfg, build_dir)):
        if not clean:
            console.warn(
                "Build tree was configured with a different generator; rebuilding from scratch.")
        if not _remove_tree(build_dir):
            return 1

    # The packages are chosen on every build, so a changed library reaches the link and the path.
    packages = _link_packages(cfg, root, build_dir, build_type)
    if packages is None:
        return 1

    if defines or _needs_configure(cfg, build_dir):
        rc = configure(build_type, defines, packages, sanitizer)
        if rc != 0:
            return rc

    offset = build_report.ninja_log_offset(build_dir)
    rc = _DRIVER.compile(cfg, build_dir, cwd=root, env=_env(cfg, build_dir),
                         config=_CMAKE_BUILD_TYPE[build_type],
                         jobs=jobs or build_admission.hardware_threads())
    if report:
        _print_report(build_dir, offset)
    return rc


def test(build_type: BuildType | None, sizes: Sequence[Size] = (), clean: bool = False,
         filter: str | None = None, sanitizer: Sanitizer | None = None,
         jobs: int | None = None) -> int:
    """Builds when needed, then runs ctest over the tests *sizes* and *filter* select.

    Args:
        sizes: The sizes to run; none runs every size.
        filter: A ctest -R regex over the `Suite.Case` names the test build registers.
        sanitizer: Builds with this sanitizer first.
        jobs: Tests run at once; none leaves the count to ctest.
    """
    build_type = resolve_build_type(build_type)
    rc = build(build_type, clean=clean, sanitizer=sanitizer)
    if rc != 0:
        return rc

    root = find_project_root()
    build_dir = _build_dir(root, build_type, sanitizer)
    cfg = load_config()
    return _DRIVER.ctest_run(cfg, build_dir, _env(cfg, build_dir),
                             label_regex=size_label_regex(sizes), filter=filter,
                             jobs=jobs, config=_CMAKE_BUILD_TYPE[build_type])


def bench(build_type: BuildType | None, groups: Sequence[str] = (), clean: bool = False) -> int:
    """Builds when needed, then runs the benchmark program over the groups *groups* name.

    Args:
        groups: Substrings of the case groups to run; none runs every group.

    Returns:
        The build's code when it fails, 1 when the tree holds no benchmark program, else the
        program's own code.
    """
    build_type = resolve_build_type(build_type, K_BENCHMARK_TYPE)
    rc = build(build_type, clean=clean)
    if rc != 0:
        return rc

    root = find_project_root()
    cfg = load_config()
    build_dir = _build_dir(root, build_type)
    exe = discovery.resolve(build_dir, console, target=_BENCHMARK, sort=False, default=None)
    if exe is None:
        return 1
    console.header(f"Benchmark ({_CMAKE_BUILD_TYPE[build_type]})")
    return _RUNNER.run([str(exe), *groups], root, _env(cfg, build_dir))

def clean(build_type: BuildType | None = None, sanitizer: Sanitizer | None = None,
          everything: bool = False) -> int:
    """Removes one build tree, or build/ whole, and reports what happened.

    Args:
        build_type: The tree's type; none removes the default type's tree.
        sanitizer: Removes the tree that sanitizer owns.
        everything: Removes build/ with every tree, the gate's state and its cache.

    Returns:
        1 when a file in the tree could not be removed, else 0.
    """
    console.header("Project Clean")
    root = find_project_root()
    build_dir = (root / "build" if everything
                 else _build_dir(root, resolve_build_type(build_type), sanitizer))
    if not build_dir.exists():
        console.info(f"{build_dir} does not exist, nothing to clean.")
        return 0
    if not _remove_tree(build_dir):
        return 1
    console.success(f"{build_dir} removed.")
    return 0


def docs() -> int:
    """Generates the API reference from the headers through .config/doxygen/Doxyfile."""
    console.header("Doxygen Generation")
    root = find_project_root()
    cfg = load_config()
    doxyfile = root / ".config" / "doxygen" / "Doxyfile"
    return _DRIVER.doxygen(cfg, doxyfile, root, env=_env(cfg, _build_dir(root, K_DEFAULT_TYPE)),
                           install_hint=_DOXYGEN_HINT)


def check(report: bool = False, rule: str | None = None) -> int:
    """Runs the eight shared checkers under tools/, the set check_all.py runs before its tests.

    Args:
        report: Makes each checker print issue counts and exit zero.
        rule: The one rule to run; only the checkers whose rule table names it run.

    Returns:
        The highest exit code among the checkers that ran, or 1 when no checker names *rule*.
    """
    root = find_project_root()
    checkers = [checker for checker in _CHECKERS
                if rule is None or rule in _rules_of(root / checker)]
    if not checkers:
        console.error(f"no checker under tools/ has the rule {rule}")
        return 1
    arguments = (["--report"] if report else []) + (["--rule", rule] if rule else [])
    return max(_RUNNER.run([sys.executable, str(root / checker), *arguments], root)
               for checker in checkers)


def _rules_of(checker: Path) -> frozenset[str]:
    """Returns the rule names the checker at *checker* prints for --list-rules."""
    listed = subprocess.run([sys.executable, str(checker), "--list-rules"],
                            capture_output=True, text=True, check=False)
    return frozenset(listed.stdout.split()) if listed.returncode == 0 else frozenset()

def _locate_host_gui(cfg: Config, build_dir: Path, config: str) -> Path | None:
    """Returns the host GUI binary the tree holds for *config*, or None.

    A multi-config generator keeps each configuration in a folder of its own; a single-config
    tree holds one binary, of the type it was configured with.
    """
    name = f"{_HOST_GUI}.exe" if os.name == "nt" else _HOST_GUI
    suffix = Path(config) if cfg.is_multi_config() else Path()
    for folder in _HOST_GUI_FOLDERS:
        candidate = build_dir / folder / suffix / name
        if candidate.is_file():
            return candidate
    return None


def host(build_type: BuildType | None = None, launch: bool = True,
         app_args: Sequence[str] | None = None) -> int:
    """Builds the requested type, then launches the standalone host GUI of that type.

    Args:
        launch: False stops after the build.
        app_args: Arguments handed to the host GUI unchanged.

    Returns:
        The build's code when it fails or *launch* is off, 1 when the tree holds no host GUI,
        130 after Ctrl+C, else the host GUI's own exit code.
    """
    build_type = resolve_build_type(build_type)
    rc = build(build_type)
    if rc != 0 or not launch:
        return rc

    root = find_project_root()
    cfg = load_config()
    build_dir = _build_dir(root, build_type)
    config = _CMAKE_BUILD_TYPE[build_type]

    exe = _locate_host_gui(cfg, build_dir, config)
    if exe is None:
        console.error(
            f"{_HOST_GUI} was not found under {build_dir} after a {config} build. "
            "It is built only when SUSHIDSP_BUILD_GUI is ON.")
        return 1

    console.header(f"Launching {exe.name} ({config})")
    return _APP_RUNNER.run([str(exe), *(app_args or ())], root, _env(cfg, build_dir))


def run(target: str | None = None, sort: bool = False,
        app_args: Sequence[str] | None = None, build_type: BuildType | None = None) -> int:
    """Runs an executable the tree of *build_type* already holds, without building first.

    Args:
        target: The executable's name, matched exactly and then as a substring; None runs
            the configured `target_bin`.
        sort: Prompts for one executable from a list and ignores *target*.
        app_args: Arguments handed to the executable unchanged.
        build_type: Whose tree to look in; none is the default type's.

    Returns:
        1 when there is no build tree or no match, 130 after Ctrl+C, else the executable's code.
    """
    root = find_project_root()
    cfg = load_config()
    build_type = resolve_build_type(build_type)
    build_dir = _build_dir(root, build_type)
    if not build_dir.is_dir():
        console.error(
            f"{build_dir} does not exist. Run `sd build --type {build_type.value}` first.")
        return 1

    exe = discovery.resolve(build_dir, console, target=target, sort=sort, default=cfg.target_bin)
    if exe is None:
        return 1

    console.info(f"Executing: {exe}")
    return _APP_RUNNER.run([str(exe), *(app_args or ())], root, _env(cfg, build_dir))
