# Copyright (c) 2026-present Mustafa Garip & Sushi Systems
# Licensed under the Apache License, Version 2.0. See LICENSE.
"""Enable `python3 -m sushidsp` as an entry point."""

from .cli import app

if __name__ == "__main__":
    app()
