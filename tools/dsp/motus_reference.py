# motus_reference.py
# SushiDSP - https://github.com/SushiSystems/SushiDSP
# Copyright (c) 2026-present Mustafa Garip & Sushi Systems
# Licensed under PolyForm Noncommercial 1.0.0. See LICENSE.
# Commercial use requires a licence from Sushi Systems.
"""Measure octave-band decay figures from a Motus spherical-harmonic room impulse response.

Usage:  python tools/dsp/motus_reference.py --rir 0001_1_sh_rirs.wav 0002_1_sh_rirs.wav \
            0003_1_sh_rirs.wav --csv tests/fixtures/motus_reference.csv
The input, the method and every fixed parameter are in tools/dsp/README.md, "motus_reference.py".
"""
from __future__ import annotations

import argparse
import csv
import math
import os
import sys
from dataclasses import dataclass

import numpy as np
from scipy.io import wavfile
from scipy.signal import butter, sosfiltfilt

K_OCTAVE_CENTRES_HZ = (125, 250, 500, 1000, 2000, 4000)
K_FILTER_ORDER = 4
K_NOISE_TAIL_FRACTION = 0.10
K_SMOOTHING_S = 0.020
K_DIRECT_HALF_WINDOW_S = 0.0025
K_CLARITY_SPLIT_S = 0.050
K_FIT_START_DB = -5.0
K_T20_END_DB = -25.0
K_T30_END_DB = -35.0
K_EDT_END_DB = -10.0
K_OMNI_CHANNEL = 0
K_CSV_COLUMNS = ("measurement", "source", "band_hz", "t20_s", "t30_s", "edt_s", "drr_db", "c50_db")


class MotusReferenceError(Exception):
    """Base class of every error this script raises on purpose."""


class UnreadableRirError(MotusReferenceError):
    """A RIR file is missing, malformed or not named like a Motus file."""


@dataclass(frozen=True, slots=True)
class BandFigures:
    """The decay figures of one octave band of one RIR.

    Attributes:
        measurement: Motus measurement number, such as "0001".
        source: Motus source number.
        band_hz: Octave band centre frequency.
        t20_s: T20 in seconds, or None when the decay lacks the range.
        t30_s: T30 in seconds, or None when the decay lacks the range.
        edt_s: Early decay time in seconds.
        drr_db: Direct-to-reverberant ratio in dB.
        c50_db: Clarity C50 in dB.
    """

    measurement: str
    source: int
    band_hz: int
    t20_s: float | None
    t30_s: float | None
    edt_s: float
    drr_db: float
    c50_db: float


@dataclass(frozen=True, slots=True)
class BandDecay:
    """The intermediate results of one band, kept so a check can inspect them.

    Attributes:
        decay_db: Schroeder decay curve in dB relative to its start, onset to truncation.
        truncation: Sample index one past the last integrated sample, relative to the onset.
        range_db: Level of the smoothed envelope at the onset above the noise level, in dB.
        figures: The figures written to the CSV.
    """

    decay_db: np.ndarray
    truncation: int
    range_db: float
    figures: BandFigures


def read_omni_channel(path: str) -> tuple[np.ndarray, int]:
    """Reads channel 0 of a multichannel WAV as floats.

    Args:
        path: File to read.

    Returns:
        The W channel scaled to full scale 1.0 and the sample rate in Hz.

    Raises:
        UnreadableRirError: When the file cannot be read or has one channel only.
    """
    try:
        rate, data = wavfile.read(path)
    except (OSError, ValueError) as error:
        raise UnreadableRirError(f"cannot read {path}: {error}") from error
    if data.ndim != 2 or data.shape[1] <= K_OMNI_CHANNEL:
        raise UnreadableRirError(f"{path} is not a multichannel WAV")
    omni = np.asarray(data[:, K_OMNI_CHANNEL], dtype=np.float64)
    if data.dtype == np.int32:
        omni /= float(1 << 31)
    elif data.dtype == np.int16:
        omni /= float(1 << 15)
    return omni, int(rate)


def parse_motus_name(path: str) -> tuple[str, int]:
    """Reads the measurement and source numbers out of a Motus file name.

    Args:
        path: A path whose base name looks like 0001_1_sh_rirs.wav.

    Returns:
        The measurement number as text and the source number.

    Raises:
        UnreadableRirError: When the base name has no measurement and source part.
    """
    parts = os.path.basename(path).split("_")
    if len(parts) < 2 or not parts[0].isdigit() or not parts[1].isdigit():
        raise UnreadableRirError(f"{path} is not named <measurement>_<source>_...")
    return parts[0], int(parts[1])


def band_pass(signal: np.ndarray, rate: int, centre_hz: float) -> np.ndarray:
    """Filters a signal to one octave band with a zero-phase Butterworth band-pass.

    Args:
        signal: The unfiltered response.
        rate: Sample rate in Hz.
        centre_hz: Octave band centre; the edges are centre / sqrt(2) and centre * sqrt(2).

    Returns:
        The filtered response, the same length as the input.
    """
    edges = (centre_hz / math.sqrt(2.0), centre_hz * math.sqrt(2.0))
    sos = butter(K_FILTER_ORDER, edges, btype="bandpass", fs=rate, output="sos")
    return sosfiltfilt(sos, signal)


def find_truncation(energy: np.ndarray, rate: int) -> tuple[int, float, float]:
    """Finds where the decay of a squared response meets its noise floor.

    Args:
        energy: Squared band signal from the onset on.
        rate: Sample rate in Hz.

    Returns:
        The truncation index, the noise level (mean square of the last 10 percent) and the
        level of the smoothed envelope at its start above that noise, in dB.
    """
    tail = max(1, int(len(energy) * K_NOISE_TAIL_FRACTION))
    noise = float(np.mean(energy[-tail:]))
    window = max(1, int(round(K_SMOOTHING_S * rate)))
    smooth = np.convolve(energy, np.full(window, 1.0 / window), mode="same")
    below = np.nonzero(smooth <= noise)[0]
    truncation = int(below[0]) if below.size else len(energy)
    range_db = 10.0 * math.log10(max(float(np.max(smooth)), 1e-300) / max(noise, 1e-300))
    return truncation, noise, range_db


def schroeder_decay_db(energy: np.ndarray) -> np.ndarray:
    """Integrates a squared response backward and expresses it in dB below its start.

    Args:
        energy: Squared response, already truncated.

    Returns:
        The decay curve, 0 dB at index 0 and non-increasing.
    """
    integral = np.cumsum(energy[::-1])[::-1]
    return 10.0 * np.log10(np.maximum(integral, 1e-300) / integral[0])


def fit_decay_time(decay_db: np.ndarray, rate: int, end_db: float,
                   start_db: float = K_FIT_START_DB) -> float | None:
    """Fits a line to a stretch of the decay curve and returns the time for a 60 dB fall.

    Args:
        decay_db: Decay curve from schroeder_decay_db, non-increasing from 0 dB.
        rate: Sample rate in Hz.
        end_db: Lower end of the fitted stretch, in dB.
        start_db: Upper end of the fitted stretch, in dB.

    Returns:
        Seconds for a 60 dB fall along the fitted line, or None when the curve never reaches
        end_db or the stretch holds fewer than two samples.
    """
    first = np.nonzero(decay_db <= start_db)[0]
    last = np.nonzero(decay_db <= end_db)[0]
    if first.size == 0 or last.size == 0 or last[0] - first[0] < 2:
        return None
    index = np.arange(first[0], last[0] + 1)
    slope, _ = np.polyfit(index / rate, decay_db[index], 1)
    return -60.0 / slope if slope < 0.0 else None


def energy_ratio_db(numerator: float, denominator: float) -> float:
    """Returns 10 log10 of an energy ratio, guarding a zero denominator.

    Args:
        numerator: Energy in the top of the ratio.
        denominator: Energy in the bottom of the ratio.

    Returns:
        The ratio in dB; positive infinity or negative infinity when a side is zero.
    """
    if denominator <= 0.0:
        return math.inf
    if numerator <= 0.0:
        return -math.inf
    return 10.0 * math.log10(numerator / denominator)


def analyse_band(response: np.ndarray, rate: int, onset: int, centre_hz: int,
                 measurement: str, source: int) -> BandDecay:
    """Computes the decay figures of one octave band.

    Args:
        response: The unfiltered W channel.
        rate: Sample rate in Hz.
        onset: Sample index of the direct sound in the unfiltered channel.
        centre_hz: Octave band centre.
        measurement: Motus measurement number.
        source: Motus source number.

    Returns:
        The band's decay curve, truncation point, dynamic range and figures.
    """
    band = band_pass(response, rate, centre_hz)
    squared = band * band
    tail = squared[onset:]
    truncation, _, range_db = find_truncation(tail, rate)
    energy = tail[:truncation]
    decay_db = schroeder_decay_db(energy)
    half = int(round(K_DIRECT_HALF_WINDOW_S * rate))
    direct = float(np.sum(squared[max(0, onset - half):onset + half + 1]))
    late = float(np.sum(energy[half + 1:]))
    split = min(int(round(K_CLARITY_SPLIT_S * rate)), len(energy))
    edt = fit_decay_time(decay_db, rate, K_EDT_END_DB, 0.0)
    figures = BandFigures(
        measurement=measurement,
        source=source,
        band_hz=centre_hz,
        t20_s=fit_decay_time(decay_db, rate, K_T20_END_DB),
        t30_s=fit_decay_time(decay_db, rate, K_T30_END_DB),
        edt_s=math.nan if edt is None else edt,
        drr_db=energy_ratio_db(direct, late),
        c50_db=energy_ratio_db(float(np.sum(energy[:split])), float(np.sum(energy[split:]))),
    )
    return BandDecay(decay_db, truncation, range_db, figures)


def analyse_file(path: str) -> list[BandDecay]:
    """Analyses one Motus RIR in every octave band.

    Args:
        path: A Motus 0001_1_sh_rirs.wav style file.

    Returns:
        One BandDecay per octave band, in ascending band order.
    """
    measurement, source = parse_motus_name(path)
    omni, rate = read_omni_channel(path)
    onset = int(np.argmax(np.abs(omni)))
    return [analyse_band(omni, rate, onset, centre, measurement, source)
            for centre in K_OCTAVE_CENTRES_HZ]


def format_cell(value: float | int | str | None, digits: int) -> str:
    """Formats one CSV or table cell; None and NaN become an empty cell.

    Args:
        value: The value to format.
        digits: Digits after the decimal point for a float.

    Returns:
        The text of the cell.
    """
    if value is None or (isinstance(value, float) and math.isnan(value)):
        return ""
    if isinstance(value, float):
        return f"{value:.{digits}f}"
    return str(value)


def write_csv(path: str, rows: list[BandFigures]) -> None:
    """Writes the figures to a CSV file with a header row and Unix line endings.

    Args:
        path: File to write.
        rows: One entry per band per RIR.
    """
    with open(path, "w", newline="", encoding="utf-8") as stream:
        writer = csv.writer(stream, lineterminator="\n")
        writer.writerow(K_CSV_COLUMNS)
        for row in rows:
            writer.writerow([format_cell(getattr(row, name), 3) for name in K_CSV_COLUMNS])


def print_table(rows: list[BandFigures]) -> None:
    """Prints the figures as an aligned text table.

    Args:
        rows: One entry per band per RIR.
    """
    print("".join(f"{name:>12}" for name in K_CSV_COLUMNS))
    for row in rows:
        print("".join(f"{format_cell(getattr(row, name), 3):>12}" for name in K_CSV_COLUMNS))


def parse_arguments(argv: list[str]) -> argparse.Namespace:
    """Parses the command line.

    Args:
        argv: The arguments after the program name.

    Returns:
        The parsed namespace with rir (list of paths) and csv (path or None).
    """
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("--rir", nargs="+", required=True, metavar="WAV",
                        help="Motus SH RIR files, such as 0001_1_sh_rirs.wav")
    parser.add_argument("--csv", metavar="PATH", help="write the figures to this CSV file")
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
        rows = [band.figures for path in arguments.rir for band in analyse_file(path)]
    except MotusReferenceError as error:
        print(f"motus_reference: {error}", file=sys.stderr)
        return 1
    print_table(rows)
    if arguments.csv:
        write_csv(arguments.csv, rows)
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
