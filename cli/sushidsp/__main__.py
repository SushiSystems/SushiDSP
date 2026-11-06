# __main__.py
# SushiDSP - https://github.com/SushiSystems/SushiDSP
# Copyright (c) 2026-present Mustafa Garip & Sushi Systems
# Licensed under the Apache License 2.0. See LICENSE.
"""Enables `python -m sushidsp` as an entry point."""

from __future__ import annotations

from .cli import main

if __name__ == "__main__":
    main()
