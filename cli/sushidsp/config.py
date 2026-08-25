"""Layered configuration loading for the SushiDSP CLI.

Precedence (low to high):

    built-in defaults -> config.toml -> config.local.toml -> SD_* env vars

SushiDSP is the plainest module in the stack: it selects no SYCL toolchain, has
no sibling checkout to resolve, and takes cmake/ctest and its compiler from
PATH. So it uses the shared loader for locating the project and layering files
and nothing else -- no :class:`sushicore.stack_config.StackConfig`, because
there is no shared dependency tree for it to resolve against.
"""

from __future__ import annotations

from dataclasses import dataclass

from sushicore.config_base import ToolConfig
from sushicore.module_config import ModuleConfig
from sushicore.profile import ModuleProfile

#: How SushiDSP describes itself to the shared machinery.
PROFILE = ModuleProfile(
    name="SushiDSP",
    program="sd",
    env_prefix="SD",
    root_marker="CMakeLists.txt",
    default_target="sushidsp_host",
    # A deliberate subset: the knobs SushiDSP actually has. It pins no
    # toolchain and uses no vcpkg, so SD_VCPKG_ROOT and friends would name
    # nothing.
    env_fields=("cxx", "generator", "cmake_exe", "ctest_exe", "target_bin"),
)

# use_workspace_config=False: SushiDSP resolves everything from PATH, so the
# workspace-shared config.local.toml that `ss install` writes holds nothing for
# it to consume.
_MODULE = ModuleConfig(PROFILE, use_workspace_config=False)

find_project_root = _MODULE.find_project_root
config_dir = _MODULE.config_dir

#: Field -> environment variable, derived from the profile's prefix. Exposed as
#: a module attribute because `sd config` reports which values came from the
#: environment and needs the reverse map.
_ENV_OVERRIDES = PROFILE.env_overrides()


@dataclass
class Config(ToolConfig):
    """Resolved, platform-specific tool configuration for the SushiDSP build.

    Inherits every field (cmake/ninja/vcpkg paths, etc.) from :class:`ToolConfig`;
    only two defaults differ: ``target_bin`` (names the standalone host app this
    repo actually ships) and ``generator`` (see below).
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
    """Load and resolve the layered configuration for the current platform."""
    return _MODULE.load(Config)
