# test_config.py
# SushiDSP - https://github.com/SushiSystems/SushiDSP
# Copyright (c) 2026-present Mustafa Garip & Sushi Systems
# Licensed under the Apache License 2.0. See LICENSE.
"""Tests for project-root resolution and config-dir selection."""

from __future__ import annotations

import pytest

from sushidsp import config


@pytest.mark.medium
def test_find_project_root_raises_outside_checkout(tmp_path):
    """No CMakeLists.txt anywhere above -> a clear SystemExit, not a crash."""
    with pytest.raises(SystemExit):
        config.find_project_root(tmp_path)


@pytest.mark.medium
def test_find_project_root_walks_up_to_marker(tmp_path):
    """The root is the nearest ancestor holding CMakeLists.txt."""
    (tmp_path / "CMakeLists.txt").write_text("project(x)\n")
    nested = tmp_path / "a" / "b"
    nested.mkdir(parents=True)
    assert config.find_project_root(nested) == tmp_path


@pytest.mark.medium
@pytest.mark.parametrize("config_toml", [True, False])
def test_config_dir_is_cli_with_or_without_a_config_file(tmp_path, config_toml):
    """config_dir() returns <root>/cli whether or not it holds config.toml."""
    (tmp_path / "CMakeLists.txt").write_text("project(x)\n")
    cli_dir = tmp_path / "cli"
    if config_toml:
        cli_dir.mkdir()
        (cli_dir / "config.toml").write_text("")

    assert config.config_dir(tmp_path) == cli_dir


@pytest.mark.small
def test_every_env_field_has_an_environment_variable():
    """Each field the profile lists as overridable maps to a real SD_* variable."""
    overrides = config.PROFILE.env_overrides()
    assert set(overrides) == set(config.PROFILE.env_fields)
    assert all(name.startswith("SD_") for name in overrides.values())
