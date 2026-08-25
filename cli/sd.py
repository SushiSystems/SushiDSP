# sd.py
#
# Copyright 2026 SushiDSP Authors
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

import subprocess
import sys
from pathlib import Path

import typer

app = typer.Typer()
ROOT = Path(__file__).resolve().parent.parent
BUILD_DIR = ROOT / "build"


def run(cmd: list[str]) -> None:
    result = subprocess.run(cmd, cwd=ROOT)
    if result.returncode != 0:
        raise typer.Exit(result.returncode)


@app.command()
def configure() -> None:
    run(["cmake", "-S", str(ROOT), "-B", str(BUILD_DIR), "-DCMAKE_BUILD_TYPE=Debug"])


@app.command()
def build() -> None:
    if not BUILD_DIR.exists():
        configure()
    run(["cmake", "--build", str(BUILD_DIR)])


@app.command()
def test() -> None:
    build()
    run(["ctest", "--test-dir", str(BUILD_DIR), "--output-on-failure"])


if __name__ == "__main__":
    app()
