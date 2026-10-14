# bras_reference.py
# SushiDSP - https://github.com/SushiSystems/SushiDSP
# Copyright (c) 2026-present Mustafa Garip & Sushi Systems
# Licensed under PolyForm Noncommercial 1.0.0. See LICENSE.
# Commercial use requires a licence from Sushi Systems.
"""Measure the mean octave-band T30 of a BRAS room from its dodecahedron impulse responses.

Usage: python tools/dsp/bras_reference.py --room CR2 --rir <wav>... --csv <path>
Source, licence and method: tests/nodes/room/fixtures/built_room_loss/bras_cr2_reference.README.md and tools/dsp/README.md,
"motus_reference.py", whose band analysis every file goes through.
"""

from __future__ import annotations

import argparse
import csv
import os
import sys
from dataclasses import dataclass

import numpy as np
from scipy.io import wavfile

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

import motus_reference  # noqa: E402

K_CSV_COLUMNS = ("room", "band_hz", "rir_count", "t30_mean_s", "t30_sd_s", "min_range_db")


class BrasReferenceError(Exception):
    """A RIR file is missing, malformed or yields no T30 in some band."""


@dataclass(frozen=True, slots=True)
class BandMean:
    """The averaged T30 of one octave band over every RIR of one room.

    Attributes:
        room: BRAS room name, such as "CR2".
        band_hz: Octave band centre frequency.
        rir_count: Number of RIRs averaged.
        t30_mean_s: Mean T30 in seconds.
        t30_sd_s: Population standard deviation of the T30 values, in seconds.
        min_range_db: Smallest dynamic range above the noise seen in this band, in dB.
    """

    room: str
    band_hz: int
    rir_count: int
    t30_mean_s: float
    t30_sd_s: float
    min_range_db: float


def read_mono(path: str) -> tuple[np.ndarray, int]:
    """Reads a WAV file's first channel as floats.

    Args:
        path: File to read.

    Returns:
        The samples and the sample rate in Hz.

    Raises:
        BrasReferenceError: When the file cannot be read.
    """
    try:
        rate, data = wavfile.read(path)
    except (OSError, ValueError) as error:
        raise BrasReferenceError(f"cannot read {path}: {error}") from error
    samples = np.asarray(data if data.ndim == 1 else data[:, 0], dtype=np.float64)
    return samples, int(rate)


def analyse_room(room: str, paths: list[str]) -> list[BandMean]:
    """Averages the T30 of every band over the given RIRs.

    Args:
        room: BRAS room name written to each row.
        paths: The room's RIR files.

    Returns:
        One BandMean per octave band, in ascending band order.

    Raises:
        BrasReferenceError: When a file is unreadable or a band decays too little for a T30.
    """
    values: dict[int, list[float]] = {centre: [] for centre in motus_reference.K_OCTAVE_CENTRES_HZ}
    ranges: dict[int, list[float]] = {centre: [] for centre in motus_reference.K_OCTAVE_CENTRES_HZ}
    for path in paths:
        samples, rate = read_mono(path)
        onset = int(np.argmax(np.abs(samples)))
        for centre in motus_reference.K_OCTAVE_CENTRES_HZ:
            band = motus_reference.analyse_band(samples, rate, onset, centre, room, 0)
            if band.figures.t30_s is None or not np.isfinite(band.figures.t30_s):
                raise BrasReferenceError(f"{path} has no T30 at {centre} Hz")
            values[centre].append(band.figures.t30_s)
            ranges[centre].append(band.range_db)
    return [BandMean(room, centre, len(values[centre]), float(np.mean(values[centre])),
                     float(np.std(values[centre])), float(np.min(ranges[centre])))
            for centre in motus_reference.K_OCTAVE_CENTRES_HZ]


def write_csv(path: str, rows: list[BandMean]) -> None:
    """Writes the band means to a CSV file with a header row and Unix line endings.

    Args:
        path: File to write.
        rows: One entry per band.
    """
    with open(path, "w", newline="", encoding="utf-8") as stream:
        writer = csv.writer(stream, lineterminator="\n")
        writer.writerow(K_CSV_COLUMNS)
        for row in rows:
            writer.writerow([row.room, row.band_hz, row.rir_count, f"{row.t30_mean_s:.3f}",
                             f"{row.t30_sd_s:.3f}", f"{row.min_range_db:.1f}"])


def parse_arguments(argv: list[str]) -> argparse.Namespace:
    """Parses the command line.

    Args:
        argv: The arguments after the program name.

    Returns:
        The parsed namespace with room, rir (list of paths) and csv (path or None).
    """
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("--room", required=True, help="BRAS room name, such as CR2")
    parser.add_argument("--rir", nargs="+", required=True, metavar="WAV",
                        help="the room's single-channel RIR files")
    parser.add_argument("--csv", metavar="PATH", help="write the band means to this CSV file")
    return parser.parse_args(argv)


def main(argv: list[str]) -> int:
    """Runs the analysis and prints or writes the result.

    Args:
        argv: The arguments after the program name.

    Returns:
        The process exit status.
    """
    arguments = parse_arguments(argv)
    try:
        rows = analyse_room(arguments.room, arguments.rir)
    except (BrasReferenceError, motus_reference.MotusReferenceError) as error:
        print(f"bras_reference: {error}", file=sys.stderr)
        return 1
    for row in rows:
        print(f"{row.room} {row.band_hz:>5} Hz  n={row.rir_count}  T30 {row.t30_mean_s:.3f} s"
              f"  sd {row.t30_sd_s:.3f} s  range {row.min_range_db:.1f} dB")
    if arguments.csv:
        write_csv(arguments.csv, rows)
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
