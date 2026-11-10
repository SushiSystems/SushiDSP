# test_ports.py
# SushiDSP - https://github.com/SushiSystems/SushiDSP
# Copyright (c) 2026-present Mustafa Garip & Sushi Systems
# Licensed under the Apache License 2.0. See LICENSE.
"""The files and processes ports: the in-memory adapters and the system ones."""

from __future__ import annotations

import sys
from pathlib import Path

import pytest

from sushidsp.files_memory import MemoryFiles
from sushidsp.files_system import SystemFiles
from sushidsp.processes_memory import MemoryProcesses
from sushidsp.processes_system import SystemProcesses


def _ignore(function, target, exc_info) -> None:
    """Ignores a file that cannot be removed."""


@pytest.mark.small
def test_memory_files_record_a_removal_and_refuse_a_locked_folder():
    """A folder is recorded as removed unless it is locked, and then PermissionError."""
    locked = Path("locked")
    files = MemoryFiles(locked=frozenset({locked}))

    files.rmtree(Path("free"), _ignore)
    with pytest.raises(PermissionError):
        files.rmtree(locked, _ignore)

    assert files.removed == [Path("free")]


@pytest.mark.small
def test_memory_processes_answer_a_listed_command_and_fail_another():
    """A listed command returns its output; any other returns code 1 and nothing."""
    processes = MemoryProcesses({("a",): (0, "text")})

    assert processes.capture(["a"]) == (0, "text")
    assert processes.capture(["b"]) == (1, "")
    assert processes.asked == [("a",), ("b",)]


@pytest.mark.medium
def test_system_files_remove_a_folder_with_its_content(tmp_path):
    """The system adapter deletes the folder and what is in it."""
    folder = tmp_path / "tree"
    (folder / "inner").mkdir(parents=True)
    (folder / "inner" / "file.txt").write_text("x", encoding="utf-8")

    SystemFiles().rmtree(folder, _ignore)

    assert not folder.exists()


@pytest.mark.medium
def test_system_processes_return_the_code_and_the_output_of_the_command():
    """The system adapter starts the command and returns its exit code and standard output."""
    code, output = SystemProcesses().capture([sys.executable, "-c", "print('ok')"])

    assert (code, output.strip()) == (0, "ok")
