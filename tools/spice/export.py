#!/usr/bin/env python3
# export.py
# SushiDSP - https://github.com/SushiSystems/SushiDSP
# Copyright (c) 2026-present Mustafa Garip & Sushi Systems
# Licensed under PolyForm Noncommercial 1.0.0. See LICENSE.
# Commercial use requires a licence from Sushi Systems.
"""Regenerate the SushiDSP LTspice reference CSVs from the committed schematics.

Each block's ``<block>.asc`` runs through LTspice in batch mode, once with its ``;ac`` line
live and once with its ``;tran`` line live, and ``<block>_ac.csv`` and ``<block>_tran.csv``
land next to the schematic. CI never runs this script; it reads the committed CSVs. The steps
and the CSV columns are in tools/spice/README.md.
"""

import argparse
import math
import os
import re
import shutil
import subprocess
import sys
import tempfile

import numpy as np

BLOCKS = [
    "ts_clip",
    "ts_tone",
    "jcm_v1a",
    "jcm_v1b_cold",
    "jcm_cf",
    "jcm_fmv",
    "jcm_ltp",
    "jcm_pp_ot_nfb",
]

# The stimulus node is read from the schematic's V1 line; see tools/spice/README.md.
STIMULUS_RE = re.compile(r"(?:^|\\n)\s*V1\s+(\S+)\s+0\s", re.IGNORECASE | re.MULTILINE)

OUTPUT_NODE = "V(out)"

AC_POINTS = 200
AC_F_MIN = 10.0
AC_F_MAX = 20000.0

TRAN_RATE = 192000.0
TRAN_SECONDS = 0.020

DEFAULT_LTSPICE = os.path.join(
    os.environ.get("LOCALAPPDATA", ""), "Programs", "ADI", "LTspice", "LTspice.exe"
)

REPO_ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
CORPUS_DIR = os.path.join(REPO_ROOT, "docs", "reference", "spice")

DIRECTIVE_PREFIX = "Left 2 "


def enable_analysis(asc_text, kind):
    """Turn the ``;ac`` or ``;tran`` comment line into a live SPICE directive.

    Exactly one of the two is enabled; the other one is left commented out.

    :param asc_text: Full text of the committed ``.asc`` file.
    :param kind: Either ``"ac"`` or ``"tran"``.
    :return: The rewritten schematic text, CRLF terminated as LTspice writes it.
    """
    marker = DIRECTIVE_PREFIX + ";" + kind + " "
    out_lines = []
    found = False
    for line in asc_text.splitlines():
        index = line.find(marker)
        if index >= 0 and not found:
            head = line[: index + len(DIRECTIVE_PREFIX)]
            tail = line[index + len(DIRECTIVE_PREFIX) + 1 :]
            out_lines.append(head + "!." + tail)
            found = True
        else:
            out_lines.append(line)
    if not found:
        raise RuntimeError("no ';" + kind + "' analysis line in the schematic")
    return "\r\n".join(out_lines) + "\r\n"


def find_stimulus_node(asc_text):
    """Return the node ``V1`` drives, as it is spelled in the schematic.

    Every block in the corpus defines its stimulus as ``V1 <node> 0 SINE(...) AC 1``, so the
    schematic is the authority on which node carries the input. Reading it here means a node
    rename can never silently shift the ``input_v`` column onto some other net.

    :param asc_text: Full text of the ``.asc`` file. Directive lines inside a ``TEXT``
        block are separated by a literal backslash-n, which the pattern accounts for.
    :return: The node name, e.g. ``"src"``.
    """
    match = STIMULUS_RE.search(asc_text)
    if match is None:
        raise RuntimeError("no 'V1 <node> 0 ...' stimulus line in the schematic")
    return match.group(1)


def run_ltspice(ltspice_exe, asc_path):
    """Run LTspice in batch mode with ASCII raw output and return the ``.raw`` path.

    On failure (nonzero exit, or no ``.raw`` produced), the LTspice ``.log`` sitting next
    to ``asc_path`` is printed in full along with the work-dir path, so the cause is never
    silently discarded -- LTspice batch mode reports nothing useful on stdout/stderr, only
    in the log file.
    """
    log_path = os.path.splitext(asc_path)[0] + ".log"
    result = subprocess.run(
        [ltspice_exe, "-b", "-ascii", asc_path],
        stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT,
    )
    raw_path = os.path.splitext(asc_path)[0] + ".raw"

    def dump_log_and_fail(message):
        print("LTspice failed on " + asc_path + ": " + message)
        print("work dir: " + os.path.dirname(asc_path))
        if os.path.exists(log_path):
            print("----- " + log_path + " -----")
            with open(log_path, "r", errors="replace") as handle:
                print(handle.read())
            print("----- end log -----")
        else:
            print("(no .log file was written)")
        raise RuntimeError(message + " for " + asc_path)

    if result.returncode != 0:
        dump_log_and_fail("LTspice exited with status " + str(result.returncode))
    if not os.path.exists(raw_path):
        dump_log_and_fail("LTspice produced no raw file")
    return raw_path


def parse_raw(raw_path):
    """Parse an LTspice ASCII raw file.

    :param raw_path: Path to the ``.raw`` file LTspice wrote.
    :return: ``(variable_names, is_complex, steps)``, where ``steps`` is a list of numpy
        arrays shaped ``(points, variables)``. A ``.step`` boundary is taken where the
        independent variable (column 0) stops increasing.
    """
    with open(raw_path, "r", errors="replace") as handle:
        text = handle.read()

    header, _, values_text = text.partition("Values:")
    if not values_text:
        raise RuntimeError("no Values: section in " + raw_path)

    flags_match = re.search(r"Flags:(.*)", header)
    is_complex = flags_match is not None and "complex" in flags_match.group(1)

    names = []
    for line in header.partition("Variables:")[2].splitlines():
        fields = line.split()
        if len(fields) >= 3 and fields[0].isdigit():
            names.append(fields[1])
    if not names:
        raise RuntimeError("no Variables: entries in " + raw_path)

    def to_number(token):
        if is_complex:
            real, _, imag = token.partition(",")
            return complex(float(real), float(imag))
        return float(token)

    points = []
    current = None
    for line in values_text.splitlines():
        stripped = line.strip()
        if not stripped:
            continue
        fields = stripped.split()
        starts_point = (
            not line[:1].isspace() and fields[0].isdigit() and len(fields) >= 2
        )
        if starts_point:
            if current is not None:
                points.append(current)
            # A point's values may all share the index line; LTspice writes one per line.
            current = [to_number(field) for field in fields[1:]]
        elif current is not None:
            current.extend(to_number(field) for field in fields)
    if current is not None:
        points.append(current)

    table = np.array([row for row in points if len(row) == len(names)])
    if table.size == 0:
        raise RuntimeError("no usable data points in " + raw_path)

    sweep = table[:, 0].real
    boundaries = [0]
    for index in range(1, len(sweep)):
        if sweep[index] <= sweep[index - 1]:
            boundaries.append(index)
    boundaries.append(len(sweep))

    steps = []
    for start, end in zip(boundaries[:-1], boundaries[1:]):
        if end > start:
            steps.append(table[start:end, :])
    return names, is_complex, steps


def find_column(names, candidates):
    """Return the index of the first of ``candidates`` present in ``names``."""
    lowered = [name.lower() for name in names]
    for candidate in candidates:
        if candidate.lower() in lowered:
            return lowered.index(candidate.lower())
    raise RuntimeError("none of " + str(candidates) + " in " + str(names))


def write_ac_csv(path, names, steps, stimulus_node):
    """Resample every step onto 200 log-spaced points and write the AC CSV.

    :param stimulus_node: Unused; accepted so both CSV writers share one call shape. The AC
        response is already relative to the ``AC 1`` source magnitude.
    """
    del stimulus_node
    out_column = find_column(names, [OUTPUT_NODE])
    grid = np.logspace(math.log10(AC_F_MIN), math.log10(AC_F_MAX), AC_POINTS)

    with open(path, "w", newline="\n") as handle:
        handle.write("frequency_hz,setting,magnitude_db,phase_degrees\n")
        for setting, step in enumerate(steps):
            frequency = step[:, 0].real
            response = step[:, out_column]
            magnitude_db = 20.0 * np.log10(np.maximum(np.abs(response), 1e-30))
            phase_degrees = np.degrees(np.unwrap(np.angle(response)))
            magnitude_grid = np.interp(grid, frequency, magnitude_db)
            phase_grid = np.interp(grid, frequency, phase_degrees)
            for index in range(AC_POINTS):
                handle.write(
                    "%.6g,%d,%.6f,%.6f\n"
                    % (grid[index], setting, magnitude_grid[index], phase_grid[index])
                )


def write_tran_csv(path, names, steps, stimulus_node):
    """Resample every step onto a uniform 192 kHz grid and write the transient CSV.

    :param stimulus_node: Node name from :func:`find_stimulus_node`; its voltage becomes the
        ``input_v`` column. A raw file that does not carry it is an error, not a fallback.
    """
    in_column = find_column(names, ["V(" + stimulus_node + ")"])
    out_column = find_column(names, [OUTPUT_NODE])
    count = int(round(TRAN_RATE * TRAN_SECONDS))

    with open(path, "w", newline="\n") as handle:
        handle.write("time_s,setting,input_v,output_v\n")
        for setting, step in enumerate(steps):
            time_s = step[:, 0].real
            start = time_s[-1] - TRAN_SECONDS
            grid = start + np.arange(count) / TRAN_RATE
            input_v = np.interp(grid, time_s, step[:, in_column].real)
            output_v = np.interp(grid, time_s, step[:, out_column].real)
            for index in range(count):
                handle.write(
                    "%.9g,%d,%.6f,%.6f\n"
                    % (grid[index], setting, input_v[index], output_v[index])
                )


def export_block(block, ltspice_exe):
    """Simulate one block twice (AC then transient) and write both of its CSVs."""
    block_dir = os.path.join(CORPUS_DIR, block)
    asc_path = os.path.join(block_dir, block + ".asc")
    if not os.path.exists(asc_path):
        raise RuntimeError("missing schematic " + asc_path)

    with open(asc_path, "r") as handle:
        asc_text = handle.read()

    stimulus_node = find_stimulus_node(asc_text)

    work_dir = tempfile.mkdtemp(prefix="sushidsp_spice_" + block + "_")
    keep_work_dir = False
    try:
        # The schematics .include ../models/*.sub, so the temp tree needs the same shape.
        shutil.copytree(
            os.path.join(CORPUS_DIR, "models"), os.path.join(work_dir, "models")
        )
        run_dir = os.path.join(work_dir, block)
        os.makedirs(run_dir)

        for kind, writer, suffix in (
            ("ac", write_ac_csv, "_ac.csv"),
            ("tran", write_tran_csv, "_tran.csv"),
        ):
            run_path = os.path.join(run_dir, block + "_" + kind + ".asc")
            with open(run_path, "w", newline="") as handle:
                handle.write(enable_analysis(asc_text, kind))
            try:
                raw_path = run_ltspice(ltspice_exe, run_path)
            except Exception:
                keep_work_dir = True
                raise
            names, _, steps = parse_raw(raw_path)
            writer(os.path.join(block_dir, block + suffix), names, steps, stimulus_node)
            print("  wrote " + block + suffix + " (" + str(len(steps)) + " steps)")
    finally:
        if not keep_work_dir:
            shutil.rmtree(work_dir, ignore_errors=True)


def main(argv):
    """Command-line entry point."""
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("blocks", nargs="*", help="block names to export")
    parser.add_argument("--all", action="store_true", help="export every block")
    parser.add_argument("--ltspice", default=DEFAULT_LTSPICE, help="path to LTspice.exe")
    args = parser.parse_args(argv)

    selected = BLOCKS if args.all else args.blocks
    if not selected:
        parser.error("name at least one block, or pass --all")
    unknown = [block for block in selected if block not in BLOCKS]
    if unknown:
        parser.error("unknown block(s): " + ", ".join(unknown))
    if not os.path.exists(args.ltspice):
        parser.error("LTspice not found at " + args.ltspice + " (use --ltspice)")

    for block in selected:
        print(block)
        export_block(block, args.ltspice)
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
