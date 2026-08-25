"""Layered configuration loading for the SushiDSP CLI.

Precedence (lowest to highest):
    built-in defaults -> config.toml -> config.local.toml -> SD_* env vars

SushiDSP builds with plain CMake + a default-detected host compiler (MSVC via
the Visual Studio generator on Windows, the system compiler elsewhere) — no
SYCL toolchain, no sibling-runtime checkout, no vcpkg-provisioned dependency
tree (GoogleTest and SDL2 are both CMake FetchContent). So this config carries
only the generic host build-tool fields from :class:`ToolConfig`; it does not
subclass in any SushiDSP-specific fields the way sushiblas/sushiengine do for
their SYCL/sibling-runtime schemas.
"""

from __future__ import annotations

import platform
from dataclasses import dataclass
from pathlib import Path

# Domain-agnostic config plumbing shared by every Sushi* CLI. The generic
# build-tool schema (cmake/ninja/vcpkg paths) and the layered-load skeleton
# live in sushicore; SushiDSP needs nothing beyond that schema.
from sushicore.config_base import ToolConfig, load_tool_config
from sushicore.workspace import has_marker, walk_up


def find_project_root(start: Path | None = None) -> Path:
    """Locate the repo root by walking up from CWD until CMakeLists.txt is found.

    The CLI is installed (pip/pipx) outside the repo, so the package location
    tells us nothing about where the project lives — the invocation directory
    does. Run any `sd` command from anywhere inside the checkout.
    """
    root = walk_up(start or Path.cwd(), has_marker("CMakeLists.txt"))
    if root is None:
        raise SystemExit(
            "Not inside a SushiDSP project: no CMakeLists.txt found in the "
            "current directory or any parent. cd into the repo and try again."
        )
    return root


def config_dir(root: Path | None = None) -> Path:
    """Directory holding config.toml / config.local.toml (the repo's cli/)."""
    root = root or find_project_root()
    return root / "cli"


# Maps a Config field to the SD_* env var that overrides it.
_ENV_OVERRIDES = {
    "cxx": "SD_CXX",
    "generator": "SD_CMAKE_GENERATOR",
    "cmake_exe": "SD_CMAKE",
    "ctest_exe": "SD_CTEST",
    "target_bin": "SD_TARGET_BIN",
}


@dataclass
class Config(ToolConfig):
    """Resolved, platform-specific tool configuration for the SushiDSP build.

    Inherits every field (cmake/ninja/vcpkg paths, generator, etc.) from
    :class:`ToolConfig` unchanged — only the ``target_bin`` default differs,
    naming the standalone host app this repo actually ships.
    """

    target_bin: str = "sushidsp_host"

    # ToolConfig defaults to "Ninja", which needs a vcvars-loaded environment
    # for MSVC's cl.exe to be reachable — SushiDSP has no such environment
    # setup (no SYCL toolchain to snapshot it for), so it leaves the generator
    # unset by default and lets CMake pick the platform default (Visual
    # Studio's multi-config generator on Windows). Set explicitly in
    # config.local.toml or SD_CMAKE_GENERATOR if Ninja is wanted instead.
    generator: str = ""


def load_config() -> Config:
    """Load and resolve the layered configuration for the current platform.

    Precedence (low to high): repo config.toml -> repo config.local.toml -> env.
    """
    plat = platform.system().lower()  # 'windows' | 'linux' | 'darwin'
    cfg_dir = config_dir()
    sources = [cfg_dir / "config.toml", cfg_dir / "config.local.toml"]
    return load_tool_config(Config, sources, plat, _ENV_OVERRIDES)
