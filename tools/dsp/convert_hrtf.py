"""Convert a measured HRTF database into the flat binary the room module loads.

Usage:  python tools/dsp/convert_hrtf.py --input <SOFA file or WAV directory> \
            --output assets/hrtf/sadie2_ku100_48k.hrtf --readme assets/hrtf/README.md
        python tools/dsp/convert_hrtf.py --self-test build/hrtf_fixture.hrtf

The reference input is SADIE II, University of York, Neumann KU100 subject, 48 kHz. Nothing
in the C++ tree reads SOFA or HDF5; this script runs once on a developer's machine and the
module reads only the file it writes. The byte layout is fixed by the DE plan's Task 2 and by
docs/superpowers/specs/2026-09-08-room-acoustics-design.md section 9.3.

Each direction is stored as a minimum-phase pair plus a separate interaural delay, so the
loader can blend neighbouring directions without comb filtering. The licence and citation
block written into the asset README is copied verbatim out of the downloaded database's own
documentation; with no such text in the download the script refuses to write the block.
"""
import argparse
import hashlib
import os
import re
import struct
import sys
from dataclasses import dataclass
from typing import List, Optional, Sequence, Tuple

import numpy as np
import scipy
from scipy.special import lpmv

CONVERTER_VERSION = "1"

MAGIC = b"SDHRTF\0\0"
FORMAT_VERSION = 1
TAP_COUNT = 128
DECODER_ORDER = 3
DECODER_TAPS = 256
HEADER_BYTES = 32
FLAG_SYMMETRIC_HEAD = 1

HEAD_RADIUS_M = 0.0875
SPEED_OF_SOUND_MPS = 343.0
ONSET_THRESHOLD = 10.0 ** (-20.0 / 20.0)
TRUNCATION_LIMIT_DB = -50.0
MAGLS_CUTOFF_HZ = 2000.0
MAGLS_REGULARISATION = 1e-6
ERROR_BAND_LOW_HZ = 200.0
ERROR_BAND_HIGH_HZ = 8000.0
SYMMETRY_LIMIT_DB = -30.0
ANALYSIS_FFT = 4096
SPHERE_TERMS = 64

LICENCE_MARKER_BEGIN = "<!-- convert_hrtf.py: licence block begin -->"
LICENCE_MARKER_END = "<!-- convert_hrtf.py: licence block end -->"


class ConversionError(Exception):
    """Raised when the input cannot be converted and the tool must not write a file."""


@dataclass
class Direction:
    """One measured or modelled direction: its position, its delay and its two ears.

    Attributes:
        azimuth_rad: 0 in front, increasing to the left.
        elevation_rad: 0 on the horizon, positive upwards.
        itd_frames: Right-ear onset minus left-ear onset, in samples at the set's rate.
        left: Minimum-phase left-ear impulse response, TAP_COUNT samples.
        right: Minimum-phase right-ear impulse response, TAP_COUNT samples.
    """

    azimuth_rad: float
    elevation_rad: float
    itd_frames: float
    left: np.ndarray
    right: np.ndarray


@dataclass
class HrtfSet:
    """Everything the asset file holds.

    Attributes:
        sample_rate_hz: Rate every impulse response in the set was sampled at.
        symmetric: True when the head measures left-right symmetric.
        directions: The measured or modelled grid.
        decoder: 2*(DECODER_ORDER+1)^2 filters of DECODER_TAPS, left ear then right.
        diffuse: Two DECODER_TAPS//2-length diffuse-field averages, left then right.
    """

    sample_rate_hz: int
    symmetric: bool
    directions: List[Direction]
    decoder: np.ndarray
    diffuse: np.ndarray


def spherical_harmonics(azimuth: np.ndarray, elevation: np.ndarray, order: int) -> np.ndarray:
    """Evaluate the real spherical harmonics in ACN order and SN3D normalisation.

    Args:
        azimuth: Azimuths in radians, 0 in front and increasing to the left.
        elevation: Elevations in radians, 0 on the horizon.
        order: Highest harmonic order to evaluate.

    Returns:
        An array of shape (len(azimuth), (order+1)**2); column ACN(l, m) is harmonic (l, m).
    """
    sin_elevation = np.sin(elevation)
    channels = (order + 1) ** 2
    out = np.zeros((len(azimuth), channels))
    for degree in range(order + 1):
        for index in range(-degree, degree + 1):
            absolute = abs(index)
            # scipy's lpmv carries the Condon-Shortley phase, which Ambisonics excludes.
            legendre = ((-1.0) ** absolute) * lpmv(absolute, degree, sin_elevation)
            factorial_ratio = 1.0
            for step in range(degree - absolute + 1, degree + absolute + 1):
                factorial_ratio /= float(step)
            norm = np.sqrt((2.0 if absolute > 0 else 1.0) * factorial_ratio)
            trig = np.cos(absolute * azimuth) if index >= 0 else np.sin(absolute * azimuth)
            out[:, degree * degree + degree + index] = norm * legendre * trig
    return out


def check_harmonic_orthonormality(order: int) -> float:
    """Integrate every harmonic pair over a dense grid and return the worst deviation.

    Args:
        order: Highest harmonic order to check.

    Returns:
        The largest absolute difference between the measured Gram matrix and the SN3D
        identity diag(1 / (2l+1)).
    """
    nodes, node_weights = np.polynomial.legendre.leggauss(4 * order + 8)
    azimuth_axis = np.linspace(0.0, 2.0 * np.pi, 4 * order + 8, endpoint=False)
    azimuth_grid, node_grid = np.meshgrid(azimuth_axis, nodes, indexing="ij")
    _, weight_grid = np.meshgrid(azimuth_axis, node_weights, indexing="ij")
    weight = weight_grid.ravel()
    harmonics = spherical_harmonics(azimuth_grid.ravel(), np.arcsin(node_grid.ravel()), order)
    gram = (harmonics * weight[:, None]).T @ harmonics / weight.sum()
    expected = np.diag([1.0 / (2 * int(np.floor(np.sqrt(n))) + 1)
                        for n in range((order + 1) ** 2)])
    return float(np.abs(gram - expected).max())


def woodworth_itd_frames(azimuth: float, elevation: float, sample_rate_hz: float) -> float:
    """Return Woodworth's interaural delay for one direction, in samples.

    Args:
        azimuth: Azimuth in radians, 0 in front and increasing to the left.
        elevation: Elevation in radians.
        sample_rate_hz: Rate the delay is expressed in.

    Returns:
        The right-ear onset minus the left-ear onset, positive when the source is to the left.
    """
    lateral = np.arcsin(np.clip(np.sin(azimuth) * np.cos(elevation), -1.0, 1.0))
    seconds = (HEAD_RADIUS_M / SPEED_OF_SOUND_MPS) * (lateral + np.sin(lateral))
    return float(seconds * sample_rate_hz)


def rigid_sphere_magnitude(incidence_rad: float, frequencies_hz: np.ndarray) -> np.ndarray:
    """Evaluate the magnitude of the rigid-sphere transfer function at one incidence angle.

    The series is the far-field limit of Duda and Martens, JASA 104(5), 1998: a plane wave
    scattered off a rigid sphere, measured at the sphere's surface and referred to the free
    field at its centre. Only the magnitude is returned, because the asset stores each ear as
    a minimum-phase response with the delay held separately, so the series' phase is discarded.

    Args:
        incidence_rad: Angle between the source direction and the ear's outward normal.
        frequencies_hz: Frequencies to evaluate, in hertz.

    Returns:
        The magnitude relative to the free field at the head's centre, one entry per frequency.
    """
    from scipy.special import spherical_jn, spherical_yn

    mu = 2.0 * np.pi * np.asarray(frequencies_hz, dtype=np.float64) * HEAD_RADIUS_M \
        / SPEED_OF_SOUND_MPS
    mu = np.maximum(mu, 1e-6)
    # The series' polar axis points along the wave's travel, so the ear facing the source is
    # at Legendre argument -cos(incidence): that is the illuminated face, +6 dB at high mu.
    cos_incidence = -float(np.cos(incidence_rad))
    legendre_previous = np.ones_like(mu)
    legendre_current = np.full_like(mu, cos_incidence)
    total = np.zeros_like(mu, dtype=complex)
    with np.errstate(over="ignore", invalid="ignore", divide="ignore"):
        for degree in range(SPHERE_TERMS):
            if degree == 0:
                legendre = legendre_previous
            elif degree == 1:
                legendre = legendre_current
            else:
                legendre = ((2 * degree - 1) * cos_incidence * legendre_current
                            - (degree - 1) * legendre_previous) / degree
                legendre_previous, legendre_current = legendre_current, legendre
            bessel = spherical_jn(degree, mu, derivative=True)
            neumann = spherical_yn(degree, mu, derivative=True)
            # y_m' overflows below the sphere's cut-on for its order, where 1 / h_m' is zero.
            finite = np.isfinite(neumann) & np.isfinite(bessel)
            derivative = np.where(finite, bessel - 1j * neumann, 1.0)
            reciprocal = np.where(finite, 1.0 / derivative, 0.0)
            total += (2 * degree + 1) * ((-1j) ** degree) * legendre * reciprocal
    return np.abs(total / (mu * mu))


def minimum_phase_from_magnitude(magnitude: np.ndarray) -> np.ndarray:
    """Fold a magnitude spectrum into its minimum-phase impulse response.

    Args:
        magnitude: Non-negative magnitude over a full FFT bin set, length even and symmetric.

    Returns:
        The real minimum-phase impulse response, the same length as the input.
    """
    length = len(magnitude)
    log_magnitude = np.log(np.maximum(magnitude, 1e-12))
    cepstrum = np.fft.ifft(log_magnitude).real
    folded = np.zeros(length)
    folded[0] = cepstrum[0]
    half = length // 2
    folded[1:half] = 2.0 * cepstrum[1:half]
    folded[half] = cepstrum[half]
    return np.fft.ifft(np.exp(np.fft.fft(folded))).real


def minimum_phase_of(impulse: np.ndarray, fft_size: int = ANALYSIS_FFT) -> np.ndarray:
    """Return the minimum-phase part of an impulse response by cepstral folding.

    Args:
        impulse: The measured impulse response.
        fft_size: Transform length, at least four times the input for cepstral accuracy.

    Returns:
        The minimum-phase impulse response, fft_size samples long.
    """
    if len(impulse) > fft_size:
        raise ConversionError(f"impulse of {len(impulse)} samples exceeds the {fft_size}-point "
                              "analysis transform")
    padded = np.zeros(fft_size)
    padded[:len(impulse)] = impulse
    return minimum_phase_from_magnitude(np.abs(np.fft.fft(padded)))


def onset_frames(impulse: np.ndarray) -> float:
    """Locate the onset of an impulse response at 20 dB below its peak.

    Args:
        impulse: The measured impulse response.

    Returns:
        The crossing point in samples, linearly interpolated between the two bracketing taps.
    """
    envelope = np.abs(impulse)
    threshold = ONSET_THRESHOLD * envelope.max()
    if threshold <= 0.0:
        raise ConversionError("an impulse response is all zeros and has no onset")
    crossings = np.flatnonzero(envelope >= threshold)
    first = int(crossings[0])
    if first == 0:
        return 0.0
    previous, current = envelope[first - 1], envelope[first]
    return (first - 1) + float((threshold - previous) / (current - previous))


def truncate_with_error(impulse: np.ndarray, taps: int = TAP_COUNT) -> Tuple[np.ndarray, float]:
    """Cut an impulse response to its first taps and measure what was discarded.

    Args:
        impulse: The impulse response to truncate.
        taps: Number of samples to keep.

    Returns:
        A tuple (truncated, error_db) where error_db is the discarded energy relative to the
        whole, in decibels.
    """
    total = float(np.sum(impulse * impulse))
    kept = impulse[:taps]
    discarded = total - float(np.sum(kept * kept))
    if total <= 0.0:
        raise ConversionError("an impulse response carries no energy")
    error_db = 10.0 * np.log10(max(discarded, 1e-300) / total)
    return kept.astype(np.float64), float(error_db)


def unit_vector(azimuth: float, elevation: float) -> np.ndarray:
    """Turn a direction into a Cartesian unit vector with x forward, y left and z up.

    Args:
        azimuth: Azimuth in radians, 0 in front and increasing to the left.
        elevation: Elevation in radians, 0 on the horizon.

    Returns:
        A length-3 array.
    """
    return np.array([np.cos(elevation) * np.cos(azimuth),
                     np.cos(elevation) * np.sin(azimuth),
                     np.sin(elevation)])


def build_spherical_head_set(directions_deg: Sequence[Tuple[float, float]],
                             sample_rate_hz: int) -> HrtfSet:
    """Build a set from the Duda-Martens rigid-sphere model with Woodworth delays.

    Args:
        directions_deg: Azimuth and elevation pairs to model, in degrees.
        sample_rate_hz: Rate of the resulting impulse responses.

    Returns:
        An HrtfSet whose directions carry Woodworth delays and minimum-phase sphere responses.
    """
    frequencies = np.abs(np.fft.fftfreq(ANALYSIS_FFT, 1.0 / sample_rate_hz))
    ear_normals = [unit_vector(np.pi / 2.0, 0.0), unit_vector(-np.pi / 2.0, 0.0)]
    directions: List[Direction] = []
    for azimuth_deg, elevation_deg in directions_deg:
        azimuth, elevation = np.deg2rad(azimuth_deg), np.deg2rad(elevation_deg)
        source = unit_vector(azimuth, elevation)
        ears = []
        for normal in ear_normals:
            incidence = np.arccos(np.clip(float(source @ normal), -1.0, 1.0))
            magnitude = rigid_sphere_magnitude(incidence, frequencies)
            kept, _ = truncate_with_error(minimum_phase_from_magnitude(magnitude))
            ears.append(kept)
        directions.append(Direction(
            azimuth_rad=float(azimuth),
            elevation_rad=float(elevation),
            itd_frames=woodworth_itd_frames(float(azimuth), float(elevation), sample_rate_hz),
            left=ears[0],
            right=ears[1]))
    decoder = design_decoder(directions, sample_rate_hz)
    return HrtfSet(sample_rate_hz=sample_rate_hz, symmetric=True, directions=directions,
                   decoder=decoder, diffuse=diffuse_field_average(directions))


def diffuse_field_average(directions: Sequence[Direction]) -> np.ndarray:
    """Average the set's power spectra per ear and fold each into a minimum-phase filter.

    Args:
        directions: The set's directions.

    Returns:
        An array of shape (2, TAP_COUNT), left ear then right.
    """
    out = np.zeros((2, TAP_COUNT))
    for ear, pick in enumerate((lambda d: d.left, lambda d: d.right)):
        power = np.zeros(ANALYSIS_FFT)
        for direction in directions:
            padded = np.zeros(ANALYSIS_FFT)
            padded[:TAP_COUNT] = pick(direction)
            power += np.abs(np.fft.fft(padded)) ** 2
        magnitude = np.sqrt(power / len(directions))
        out[ear] = minimum_phase_from_magnitude(magnitude)[:TAP_COUNT]
    return out


def direction_spectra(directions: Sequence[Direction], bins: int) -> np.ndarray:
    """Transform every direction's ear pair onto a half-spectrum of the given length.

    Args:
        directions: The set's directions.
        bins: Number of positive-frequency bins, DECODER_TAPS//2 + 1 for the decoder design.

    Returns:
        A complex array of shape (len(directions), bins, 2), left ear then right.
    """
    fft_size = 2 * (bins - 1)
    out = np.zeros((len(directions), bins, 2), dtype=complex)
    for index, direction in enumerate(directions):
        for ear, impulse in enumerate((direction.left, direction.right)):
            padded = np.zeros(fft_size)
            padded[:min(TAP_COUNT, fft_size)] = impulse[:fft_size]
            out[index, :, ear] = np.fft.rfft(padded)
    return out


def design_decoder(directions: Sequence[Direction], sample_rate_hz: int) -> np.ndarray:
    """Design the magnitude-least-squares Ambisonic decoder from a set of directions.

    Plain least squares runs below MAGLS_CUTOFF_HZ. Above it the target keeps each measured
    magnitude and takes its phase from the previous bin's rendered result, which is the
    phase-continuation of Schoerkhuber, Zaunschirm and Hoeldrich, DAGA 2018.

    Args:
        directions: The set's directions.
        sample_rate_hz: Rate the cutoff frequency is read against.

    Returns:
        An array of shape (2*(DECODER_ORDER+1)**2, DECODER_TAPS); the first half is the left
        ear's filters in ACN order, the second half the right ear's.
    """
    bins = DECODER_TAPS // 2 + 1
    channels = (DECODER_ORDER + 1) ** 2
    harmonics = spherical_harmonics(
        np.array([d.azimuth_rad for d in directions]),
        np.array([d.elevation_rad for d in directions]),
        DECODER_ORDER)
    gram = harmonics.T @ harmonics + MAGLS_REGULARISATION * np.eye(channels)
    solve = np.linalg.solve(gram, harmonics.T)
    spectra = direction_spectra(directions, bins)
    frequencies = np.fft.rfftfreq(DECODER_TAPS, 1.0 / sample_rate_hz)

    coefficients = np.zeros((bins, channels, 2), dtype=complex)
    previous = np.zeros((len(directions), 2), dtype=complex)
    for bin_index in range(bins):
        target = spectra[:, bin_index, :]
        if frequencies[bin_index] > MAGLS_CUTOFF_HZ:
            phase = np.exp(1j * np.angle(previous))
            phase[previous == 0.0] = 1.0
            target = np.abs(target) * phase
        coefficients[bin_index] = solve @ target
        previous = harmonics @ coefficients[bin_index]

    filters = np.zeros((2 * channels, DECODER_TAPS))
    for ear in range(2):
        for channel in range(channels):
            spectrum = coefficients[:, channel, ear].copy()
            spectrum[0] = spectrum[0].real
            spectrum[-1] = spectrum[-1].real
            impulse = np.fft.irfft(spectrum, n=DECODER_TAPS)
            filters[ear * channels + channel] = np.roll(impulse, DECODER_TAPS // 2)
    return filters


def decoder_band_error_db(directions: Sequence[Direction], decoder: np.ndarray,
                          sample_rate_hz: int) -> float:
    """Measure the decoder's worst third-octave magnitude error against the measured set.

    Args:
        directions: The directions to test, which are the set's own grid.
        decoder: The filters design_decoder returned.
        sample_rate_hz: Rate the band edges are read against.

    Returns:
        The largest absolute band error in decibels over ERROR_BAND_LOW_HZ to
        ERROR_BAND_HIGH_HZ.
    """
    bins = DECODER_TAPS // 2 + 1
    channels = (DECODER_ORDER + 1) ** 2
    harmonics = spherical_harmonics(
        np.array([d.azimuth_rad for d in directions]),
        np.array([d.elevation_rad for d in directions]),
        DECODER_ORDER)
    spectra = direction_spectra(directions, bins)
    frequencies = np.fft.rfftfreq(DECODER_TAPS, 1.0 / sample_rate_hz)
    rendered = np.zeros_like(spectra)
    for ear in range(2):
        block = np.fft.rfft(decoder[ear * channels:(ear + 1) * channels], n=DECODER_TAPS, axis=1)
        rendered[:, :, ear] = harmonics @ block

    # The base-10 third-octave series of IEC 61260, so the top band is the nominal 8 kHz.
    centres = 1000.0 * 10.0 ** (np.arange(-20, 21) / 10.0)
    centres = centres[(centres >= ERROR_BAND_LOW_HZ * 0.99)
                      & (centres <= ERROR_BAND_HIGH_HZ * 1.01)]
    worst = 0.0
    for centre in centres:
        low, high = centre / 2.0 ** (1.0 / 6.0), centre * 2.0 ** (1.0 / 6.0)
        mask = (frequencies >= low) & (frequencies < high)
        if not mask.any():
            continue
        target = np.sqrt(np.mean(np.abs(spectra[:, mask, :]) ** 2, axis=1))
        actual = np.sqrt(np.mean(np.abs(rendered[:, mask, :]) ** 2, axis=1))
        error = 20.0 * np.log10(np.maximum(actual, 1e-12) / np.maximum(target, 1e-12))
        worst = max(worst, float(np.abs(error).max()))
    return worst


def measure_symmetry_db(directions: Sequence[Direction]) -> float:
    """Compare each direction's left ear with its mirror direction's right ear.

    Args:
        directions: The set's directions.

    Returns:
        The worst mismatch energy relative to the response energy, in decibels; -inf when the
        grid holds no mirror pairs.
    """
    index = {(round(np.sin(d.azimuth_rad), 4), round(np.cos(d.azimuth_rad), 4),
              round(d.elevation_rad, 4)): d for d in directions}
    worst = -np.inf
    for direction in directions:
        key = (round(-np.sin(direction.azimuth_rad), 4), round(np.cos(direction.azimuth_rad), 4),
               round(direction.elevation_rad, 4))
        mirror = index.get(key)
        if mirror is None:
            continue
        difference = direction.left - mirror.right
        energy = float(np.sum(direction.left ** 2))
        if energy <= 0.0:
            continue
        worst = max(worst, 10.0 * np.log10(max(float(np.sum(difference ** 2)), 1e-300) / energy))
    return float(worst)


def read_sofa(path: str) -> Tuple[np.ndarray, np.ndarray, np.ndarray, int]:
    """Read impulse responses and source positions from a SOFA file.

    Args:
        path: Path to a SimpleFreeFieldHRIR SOFA file.

    Returns:
        A tuple (azimuth_rad, elevation_rad, impulses, sample_rate_hz) where impulses has shape
        (directions, 2, taps).

    Raises:
        ConversionError: No installed package can read HDF5, or the file is not the expected
            convention.
    """
    dataset = None
    for module_name in ("netCDF4", "h5py"):
        try:
            module = __import__(module_name)
        except ImportError:
            continue
        dataset = (module, module_name)
        break
    if dataset is None:
        raise ConversionError("reading SOFA needs netCDF4 or h5py: pip install netCDF4")
    module, module_name = dataset
    if module_name == "netCDF4":
        with module.Dataset(path, "r") as handle:
            impulses = np.array(handle.variables["Data.IR"][:], dtype=np.float64)
            positions = np.array(handle.variables["SourcePosition"][:], dtype=np.float64)
            rate = float(np.array(handle.variables["Data.SamplingRate"][:]).ravel()[0])
            units = getattr(handle.variables["SourcePosition"], "Units", "degree, degree, metre")
    else:
        with module.File(path, "r") as handle:
            impulses = np.array(handle["Data.IR"], dtype=np.float64)
            positions = np.array(handle["SourcePosition"], dtype=np.float64)
            rate = float(np.array(handle["Data.SamplingRate"]).ravel()[0])
            units = handle["SourcePosition"].attrs.get("Units", b"degree").decode(errors="ignore")
    if impulses.ndim != 3 or impulses.shape[1] != 2:
        raise ConversionError(f"Data.IR has shape {impulses.shape}; a two-ear HRIR set is "
                              "required")
    if "metre" in units and "degree" not in units:
        raise ConversionError("SourcePosition is Cartesian; convert the file to spherical "
                              "degrees first")
    azimuth = np.deg2rad(positions[:, 0])
    elevation = np.deg2rad(positions[:, 1])
    return azimuth, elevation, impulses, int(round(rate))


WAV_NAME = re.compile(r"azi[_\- ]*(-?\d+(?:[.,]\d+)?).*?ele[_\- ]*(-?\d+(?:[.,]\d+)?)",
                      re.IGNORECASE)


def read_wav(path: str) -> Tuple[np.ndarray, int]:
    """Read one PCM WAV file into channel-major floating point samples.

    Args:
        path: A 16, 24 or 32-bit PCM WAV file.

    Returns:
        A tuple (samples, sample_rate_hz) where samples has shape (channels, frames) and is
        scaled to full scale at 1.0.

    Raises:
        ConversionError: The sample width is not 16, 24 or 32-bit PCM.
    """
    import wave

    with wave.open(path, "rb") as handle:
        channels = handle.getnchannels()
        rate = handle.getframerate()
        width = handle.getsampwidth()
        raw = handle.readframes(handle.getnframes())
    if width == 2:
        samples = np.frombuffer(raw, dtype="<i2").astype(np.float64) / 32768.0
    elif width == 4:
        samples = np.frombuffer(raw, dtype="<i4").astype(np.float64) / 2147483648.0
    elif width == 3:
        packed = np.frombuffer(raw, dtype=np.uint8).reshape(-1, 3).astype(np.int32)
        words = packed[:, 0] | (packed[:, 1] << 8) | (packed[:, 2] << 16)
        words = np.where(words >= 1 << 23, words - (1 << 24), words)
        samples = words.astype(np.float64) / 8388608.0
    else:
        raise ConversionError(f"{os.path.basename(path)} has {width * 8}-bit samples; 16, 24 "
                              "or 32-bit PCM is required")
    return samples.reshape(-1, channels).T, rate


def read_wav_directory(path: str) -> Tuple[np.ndarray, np.ndarray, np.ndarray, int]:
    """Read a directory of two-channel WAV files whose names carry azimuth and elevation.

    The name pattern is SADIE II's, `azi_<degrees>_ele_<degrees>.wav`, with either a comma or
    a full stop as the decimal mark.

    Args:
        path: Directory holding one WAV file per measured direction.

    Returns:
        A tuple (azimuth_rad, elevation_rad, impulses, sample_rate_hz) where impulses has shape
        (directions, 2, taps).

    Raises:
        ConversionError: A file does not match the name pattern, is not stereo, or the rates
            or lengths disagree.
    """
    names = sorted(n for n in os.listdir(path) if n.lower().endswith(".wav"))
    if not names:
        raise ConversionError(f"{path} holds no WAV files")
    azimuths, elevations, responses, rates = [], [], [], set()
    for name in names:
        match = WAV_NAME.search(name)
        if match is None:
            raise ConversionError(f"{name} does not match the azi/ele name pattern; rename the "
                                  "release's files or extend WAV_NAME")
        azimuths.append(np.deg2rad(float(match.group(1).replace(",", "."))))
        elevations.append(np.deg2rad(float(match.group(2).replace(",", "."))))
        samples, rate = read_wav(os.path.join(path, name))
        if samples.shape[0] != 2:
            raise ConversionError(f"{name} has {samples.shape[0]} channels; a two-ear "
                                  "measurement is required")
        rates.add(rate)
        responses.append(samples)
    if len(rates) != 1:
        raise ConversionError(f"the directory mixes sample rates {sorted(rates)}")
    lengths = {r.shape[1] for r in responses}
    if len(lengths) != 1:
        raise ConversionError(f"the directory mixes impulse lengths {sorted(lengths)}")
    return (np.array(azimuths), np.array(elevations), np.array(responses), rates.pop())


def load_measurements(path: str) -> Tuple[np.ndarray, np.ndarray, np.ndarray, int]:
    """Read a database from either a SOFA file or a directory of WAV files.

    Args:
        path: A `.sofa` file or a directory.

    Returns:
        A tuple (azimuth_rad, elevation_rad, impulses, sample_rate_hz).
    """
    if os.path.isdir(path):
        return read_wav_directory(path)
    if path.lower().endswith(".sofa"):
        return read_sofa(path)
    raise ConversionError(f"{path} is neither a directory of WAV files nor a .sofa file")


def build_measured_set(path: str, decoder_from_file: Optional[str]) -> Tuple[HrtfSet, dict]:
    """Convert a measured database into the set the writer stores.

    Args:
        path: The database, a SOFA file or a WAV directory.
        decoder_from_file: Path to a published decoder set to use instead of designing one, or
            None to design it.

    Returns:
        A tuple (set, report) where report carries the truncation error, the symmetry margin
        and the decoder's worst band error.

    Raises:
        ConversionError: The truncation error exceeds TRUNCATION_LIMIT_DB.
    """
    azimuth, elevation, impulses, rate = load_measurements(path)
    directions: List[Direction] = []
    worst_truncation = -np.inf
    for index in range(len(azimuth)):
        left_raw, right_raw = impulses[index, 0], impulses[index, 1]
        itd = onset_frames(right_raw) - onset_frames(left_raw)
        ears = []
        for raw in (left_raw, right_raw):
            kept, error_db = truncate_with_error(minimum_phase_of(raw))
            worst_truncation = max(worst_truncation, error_db)
            ears.append(kept)
        directions.append(Direction(azimuth_rad=float(azimuth[index]),
                                    elevation_rad=float(elevation[index]),
                                    itd_frames=float(itd), left=ears[0], right=ears[1]))
    if worst_truncation > TRUNCATION_LIMIT_DB:
        raise ConversionError(f"truncation to {TAP_COUNT} taps loses {worst_truncation:.1f} dB, "
                              f"over the {TRUNCATION_LIMIT_DB:.0f} dB the spec allows")
    if decoder_from_file is not None:
        decoder = read_decoder_file(decoder_from_file)
    else:
        decoder = design_decoder(directions, rate)
    symmetry_db = measure_symmetry_db(directions)
    hrtf = HrtfSet(sample_rate_hz=rate, symmetric=symmetry_db <= SYMMETRY_LIMIT_DB,
                   directions=directions, decoder=decoder,
                   diffuse=diffuse_field_average(directions))
    report = {"truncation_db": worst_truncation, "symmetry_db": symmetry_db,
              "band_error_db": decoder_band_error_db(directions, decoder, rate)}
    return hrtf, report


def read_decoder_file(path: str) -> np.ndarray:
    """Read a published Ambisonic decoder set from a directory of WAV files.

    The layout the tool requires is one stereo WAV per Ambisonic channel, left ear on channel
    0 and right on channel 1, whose names sort into ACN order. A published set laid out any
    other way is renamed before it is passed here; the tool reads names, not conventions it
    cannot see.

    Args:
        path: Directory holding (DECODER_ORDER+1)**2 stereo WAV files in ACN order.

    Returns:
        An array of shape (2*(DECODER_ORDER+1)**2, DECODER_TAPS), left ear's channels first.

    Raises:
        ConversionError: The file count does not match the order, a file is not stereo, or a
            filter is longer than DECODER_TAPS.
    """
    channels = (DECODER_ORDER + 1) ** 2
    names = sorted(n for n in os.listdir(path) if n.lower().endswith(".wav"))
    if len(names) != channels:
        raise ConversionError(f"{path} holds {len(names)} WAV files; order {DECODER_ORDER} "
                              f"needs {channels}, one stereo file per ACN channel")
    filters = np.zeros((2 * channels, DECODER_TAPS))
    for channel, name in enumerate(names):
        samples, _ = read_wav(os.path.join(path, name))
        if samples.shape[0] != 2:
            raise ConversionError(f"{name} has {samples.shape[0]} channels; each decoder file "
                                  "carries the left ear then the right")
        if samples.shape[1] > DECODER_TAPS:
            raise ConversionError(f"{name} is {samples.shape[1]} samples; the asset holds "
                                  f"{DECODER_TAPS} taps per filter")
        for ear in range(2):
            filters[ear * channels + channel, :samples.shape[1]] = samples[ear]
    return filters


def pack(hrtf: HrtfSet) -> bytes:
    """Serialise a set into the asset's byte layout.

    Args:
        hrtf: The set to write.

    Returns:
        The whole file, little-endian float32 throughout.
    """
    channels = (DECODER_ORDER + 1) ** 2
    if hrtf.decoder.shape != (2 * channels, DECODER_TAPS):
        raise ConversionError(f"decoder has shape {hrtf.decoder.shape}, expected "
                              f"{(2 * channels, DECODER_TAPS)}")
    flags = FLAG_SYMMETRIC_HEAD if hrtf.symmetric else 0
    out = bytearray(struct.pack("<8sIIIIII", MAGIC, FORMAT_VERSION, hrtf.sample_rate_hz,
                                len(hrtf.directions), TAP_COUNT, DECODER_ORDER, flags))
    for direction in hrtf.directions:
        out += struct.pack("<fff", direction.azimuth_rad, direction.elevation_rad,
                           direction.itd_frames)
        out += np.asarray(direction.left, dtype="<f4").tobytes()
        out += np.asarray(direction.right, dtype="<f4").tobytes()
    out += np.asarray(hrtf.decoder, dtype="<f4").tobytes()
    out += np.asarray(hrtf.diffuse, dtype="<f4").tobytes()
    return bytes(out)


def unpack(blob: bytes) -> HrtfSet:
    """Read a set back out of the asset's byte layout.

    Args:
        blob: The whole file.

    Returns:
        The set the bytes hold.

    Raises:
        ConversionError: The magic, the version or the length does not match.
    """
    if len(blob) < HEADER_BYTES:
        raise ConversionError("the file is shorter than its header")
    magic, version, rate, count, taps, order, flags = struct.unpack_from("<8sIIIIII", blob, 0)
    if magic != MAGIC:
        raise ConversionError(f"magic is {magic!r}, expected {MAGIC!r}")
    if version != FORMAT_VERSION:
        raise ConversionError(f"version is {version}, expected {FORMAT_VERSION}")
    channels = (order + 1) ** 2
    record = 12 + 8 * taps
    expected = HEADER_BYTES + count * record + 2 * channels * DECODER_TAPS * 4 + 2 * taps * 4
    if len(blob) != expected:
        raise ConversionError(f"the file is {len(blob)} bytes, expected {expected}")
    offset = HEADER_BYTES
    directions = []
    for _ in range(count):
        azimuth, elevation, itd = struct.unpack_from("<fff", blob, offset)
        pair = np.frombuffer(blob, dtype="<f4", count=2 * taps, offset=offset + 12)
        directions.append(Direction(azimuth, elevation, itd,
                                    np.array(pair[:taps], dtype=np.float64),
                                    np.array(pair[taps:], dtype=np.float64)))
        offset += record
    decoder = np.frombuffer(blob, dtype="<f4", count=2 * channels * DECODER_TAPS, offset=offset)
    offset += 2 * channels * DECODER_TAPS * 4
    diffuse = np.frombuffer(blob, dtype="<f4", count=2 * taps, offset=offset)
    return HrtfSet(sample_rate_hz=rate, symmetric=bool(flags & FLAG_SYMMETRIC_HEAD),
                   directions=directions,
                   decoder=np.array(decoder, dtype=np.float64).reshape(2 * channels, DECODER_TAPS),
                   diffuse=np.array(diffuse, dtype=np.float64).reshape(2, taps))


def round_trip_differences(written: HrtfSet, read_back: HrtfSet) -> List[str]:
    """Compare a set with the set read back out of its own bytes.

    Args:
        written: The set that was packed.
        read_back: The set unpack returned.

    Returns:
        One message per field that does not match to float32 precision; empty when every field
        agrees.
    """
    problems = []
    if written.sample_rate_hz != read_back.sample_rate_hz:
        problems.append(f"sample rate {written.sample_rate_hz} became {read_back.sample_rate_hz}")
    if written.symmetric != read_back.symmetric:
        problems.append(f"symmetry flag {written.symmetric} became {read_back.symmetric}")
    if len(written.directions) != len(read_back.directions):
        problems.append(f"{len(written.directions)} directions became "
                        f"{len(read_back.directions)}")
        return problems
    for index, (before, after) in enumerate(zip(written.directions, read_back.directions)):
        for name in ("azimuth_rad", "elevation_rad", "itd_frames"):
            expected = float(np.float32(getattr(before, name)))
            if expected != getattr(after, name):
                problems.append(f"direction {index} {name} {expected!r} became "
                                f"{getattr(after, name)!r}")
        for name in ("left", "right"):
            expected = np.asarray(getattr(before, name), dtype=np.float32)
            if not np.array_equal(expected, np.asarray(getattr(after, name), dtype=np.float32)):
                problems.append(f"direction {index} {name} does not round-trip")
    for name in ("decoder", "diffuse"):
        expected = np.asarray(getattr(written, name), dtype=np.float32)
        if not np.array_equal(expected, np.asarray(getattr(read_back, name), dtype=np.float32)):
            problems.append(f"{name} does not round-trip")
    return problems


LICENCE_HINTS = ("licence", "license", "notice", "copying", "readme", "terms", "copyright")
CITATION_MARKERS = ("SADIE", "Armstrong")


def documentation_files(root: str) -> List[str]:
    """Collect the text files in a download that could carry its licence or citation.

    Args:
        root: The downloaded database's directory, or the directory holding its SOFA file.

    Returns:
        Paths of every plain-text file whose name looks like documentation, deepest last.
    """
    found = []
    for directory, _, names in os.walk(root):
        for name in names:
            stem, extension = os.path.splitext(name.lower())
            if extension not in ("", ".txt", ".md", ".rst"):
                continue
            if any(hint in stem for hint in LICENCE_HINTS):
                found.append(os.path.join(directory, name))
    return sorted(found)


def extract_licence_and_citation(root: str) -> Tuple[str, str, str, str]:
    """Copy the licence statement and the citation request out of a download's own documents.

    Args:
        root: The downloaded database's directory.

    Returns:
        A tuple (licence_text, licence_path, citation_text, citation_path), each text copied
        verbatim from the file the path names.

    Raises:
        ConversionError: The download carries no licence statement or no citation request. The
            script writes no wording of its own in that case.
    """
    licence_text = licence_path = citation_text = citation_path = None
    for path in documentation_files(root):
        try:
            with open(path, "r", encoding="utf-8", errors="replace") as handle:
                text = handle.read()
        except OSError:
            continue
        if licence_text is None and ("Apache License" in text or "Copyright" in text):
            licence_text, licence_path = text.strip(), path
        if citation_text is None and all(marker in text for marker in CITATION_MARKERS):
            paragraphs = [p.strip() for p in re.split(r"\n\s*\n", text) if p.strip()]
            match = [p for p in paragraphs if "Armstrong" in p]
            if match:
                citation_text, citation_path = match[0], path
    if licence_text is None:
        raise ConversionError(f"no licence statement found under {root}; the README block is "
                              "not written, because its wording must be the database's own")
    if citation_text is None:
        raise ConversionError(f"no citation request found under {root}; the README block is "
                              "not written, because its wording must be the database's own")
    return licence_text, licence_path, citation_text, citation_path


def licence_block(root: str, output_path: str, blob: bytes, command: str, report: dict,
                  hrtf: HrtfSet) -> str:
    """Build the README's licence block out of the download's own text and the written file.

    Args:
        root: The downloaded database's directory, read for its licence and citation text.
        output_path: Path the converted file was written to.
        blob: The bytes that were written, hashed for the README.
        command: The command line that produced the file.
        report: The truncation, symmetry and band-error numbers the conversion measured.
        hrtf: The set that was written.

    Returns:
        The markdown block, between the two markers the README carries.
    """
    licence_text, licence_path, citation_text, citation_path = extract_licence_and_citation(root)
    digest = hashlib.sha256(blob).hexdigest()
    lines = [
        LICENCE_MARKER_BEGIN,
        "",
        f"- Source: `{os.path.abspath(root)}`",
        f"- Converted file: `{os.path.basename(output_path)}`, {len(blob)} bytes",
        f"- SHA-256: `{digest}`",
        f"- Converter: `tools/dsp/convert_hrtf.py` version {CONVERTER_VERSION}, "
        f"numpy {np.__version__}, scipy {scipy.__version__}",
        f"- Command: `{command}`",
        f"- Set: {len(hrtf.directions)} directions, {TAP_COUNT} taps, "
        f"{hrtf.sample_rate_hz} Hz, order {DECODER_ORDER} decoder, "
        f"symmetric head {str(hrtf.symmetric).lower()}",
        f"- Measured: truncation {report['truncation_db']:.1f} dB, "
        f"left-right mismatch {report['symmetry_db']:.1f} dB, "
        f"decoder third-octave error {report['band_error_db']:.2f} dB",
        "",
        f"Licence, copied verbatim from `{os.path.relpath(licence_path, root)}` in the download:",
        "",
        "```",
        licence_text,
        "```",
        "",
        f"Citation, copied verbatim from `{os.path.relpath(citation_path, root)}` in the "
        "download:",
        "",
        "```",
        citation_text,
        "```",
        "",
        LICENCE_MARKER_END,
    ]
    return "\n".join(lines) + "\n"


def write_licence_block(readme_path: str, block: str) -> None:
    """Replace the README's marked licence block with a freshly built one.

    Args:
        readme_path: The asset README to edit in place.
        block: The block licence_block returned, markers included.

    Raises:
        ConversionError: The README does not carry both markers.
    """
    with open(readme_path, "r", encoding="utf-8") as handle:
        text = handle.read()
    begin, end = text.find(LICENCE_MARKER_BEGIN), text.find(LICENCE_MARKER_END)
    if begin < 0 or end < 0:
        raise ConversionError(f"{readme_path} carries no licence block markers")
    replaced = text[:begin] + block + text[end + len(LICENCE_MARKER_END):].lstrip("\n")
    with open(readme_path, "w", encoding="utf-8", newline="\n") as handle:
        handle.write(replaced)


def run_self_test(output_path: str) -> int:
    """Build a four-direction fixture from the sphere model and prove the round trip.

    Args:
        output_path: Where the fixture is written.

    Returns:
        0 when every field round-trips and the harmonics are orthonormal, 1 otherwise.
    """
    orthonormality = check_harmonic_orthonormality(DECODER_ORDER)
    print(f"spherical harmonic orthonormality error: {orthonormality:.3e}")
    hrtf = build_spherical_head_set([(0.0, 0.0), (90.0, 0.0), (180.0, 0.0), (270.0, 0.0)], 48000)
    blob = pack(hrtf)
    directory = os.path.dirname(os.path.abspath(output_path))
    if directory:
        os.makedirs(directory, exist_ok=True)
    with open(output_path, "wb") as handle:
        handle.write(blob)
    read_back = unpack(open(output_path, "rb").read())
    problems = round_trip_differences(hrtf, read_back)
    channels = (DECODER_ORDER + 1) ** 2
    expected_bytes = (HEADER_BYTES + 4 * (12 + 8 * TAP_COUNT)
                      + 2 * channels * DECODER_TAPS * 4 + 2 * TAP_COUNT * 4)
    print(f"wrote {output_path}: {len(blob)} bytes (expected {expected_bytes})")
    print("azimuths read back: "
          + ", ".join(f"{d.azimuth_rad:.7f}" for d in read_back.directions))
    print("expected:           "
          + ", ".join(f"{float(np.float32(a)):.7f}"
                      for a in (0.0, np.pi / 2.0, np.pi, 3.0 * np.pi / 2.0)))
    print("ITD in samples:     "
          + ", ".join(f"{d.itd_frames:+.4f}" for d in read_back.directions))
    if len(blob) != expected_bytes:
        problems.append(f"file is {len(blob)} bytes, expected {expected_bytes}")
    if orthonormality > 1e-6:
        problems.append(f"spherical harmonics are not orthonormal: {orthonormality:.3e}")
    for direction, expected in zip(read_back.directions,
                                   (0.0, np.pi / 2.0, np.pi, 3.0 * np.pi / 2.0)):
        if direction.azimuth_rad != float(np.float32(expected)):
            problems.append(f"azimuth {direction.azimuth_rad!r} is not {expected!r}")
    for problem in problems:
        print(f"FAIL: {problem}", file=sys.stderr)
    print("self-test passed" if not problems else "self-test failed", file=sys.stderr)
    return 1 if problems else 0


def run_conversion(args: argparse.Namespace, command: str) -> int:
    """Convert a database, write the asset and refresh the README's licence block.

    Args:
        args: The parsed command line.
        command: The command line as typed, recorded in the README.

    Returns:
        0 on success.
    """
    # The licence text is read before anything is written, so a refusal leaves no asset behind.
    licence_root = None
    if args.readme:
        licence_root = args.licence_from or (args.input if os.path.isdir(args.input)
                                             else os.path.dirname(os.path.abspath(args.input)))
        extract_licence_and_citation(licence_root)
    hrtf, report = build_measured_set(args.input, args.decoder_from_file)
    blob = pack(hrtf)
    directory = os.path.dirname(os.path.abspath(args.output))
    if directory:
        os.makedirs(directory, exist_ok=True)
    with open(args.output, "wb") as handle:
        handle.write(blob)
    problems = round_trip_differences(hrtf, unpack(blob))
    for problem in problems:
        print(f"FAIL: {problem}", file=sys.stderr)
    print(f"wrote {args.output}: {len(blob)} bytes, {len(hrtf.directions)} directions, "
          f"{hrtf.sample_rate_hz} Hz")
    print(f"worst truncation error: {report['truncation_db']:.1f} dB "
          f"(limit {TRUNCATION_LIMIT_DB:.0f} dB)")
    print(f"left-right mismatch: {report['symmetry_db']:.1f} dB "
          f"(symmetric flag {'set' if hrtf.symmetric else 'clear'})")
    print(f"worst third-octave decoder error, {ERROR_BAND_LOW_HZ:.0f} Hz to "
          f"{ERROR_BAND_HIGH_HZ:.0f} Hz: {report['band_error_db']:.2f} dB")
    print(f"SHA-256: {hashlib.sha256(blob).hexdigest()}")
    if problems:
        return 1
    if licence_root is not None:
        write_licence_block(args.readme, licence_block(licence_root, args.output, blob, command,
                                                       report, hrtf))
        print(f"refreshed the licence block in {args.readme}")
    return 0


def main(argv: Sequence[str]) -> int:
    """Parse the command line and run either the self-test or a conversion.

    Args:
        argv: Arguments after the program name.

    Returns:
        The process exit code.
    """
    parser = argparse.ArgumentParser(
        description=__doc__.splitlines()[0],
        formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--self-test", metavar="PATH",
                        help="write a four-direction sphere-model fixture and verify the "
                             "round trip")
    parser.add_argument("--input", metavar="PATH",
                        help="the database: a .sofa file or a directory of WAV files")
    parser.add_argument("--output", metavar="PATH", help="the asset file to write")
    parser.add_argument("--readme", metavar="PATH",
                        help="the asset README whose licence block is refreshed")
    parser.add_argument("--licence-from", metavar="PATH",
                        help="the download's documentation root, when it is not the input's "
                             "own directory")
    parser.add_argument("--decoder-from-file", metavar="PATH",
                        help="use a published decoder set instead of designing one")
    args = parser.parse_args(argv)
    command = "python tools/dsp/convert_hrtf.py " + " ".join(argv)
    try:
        if args.self_test:
            return run_self_test(args.self_test)
        if not args.input or not args.output:
            parser.error("--input and --output are required without --self-test")
        return run_conversion(args, command)
    except ConversionError as error:
        print(f"error: {error}", file=sys.stderr)
        return 1


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
