# Copyright (c) 2026-present Mustafa Garip & Sushi Systems
# Licensed under the Apache License, Version 2.0. See LICENSE.
"""Design the windowed-sinc resampler prototype filter and emit its C++ headers.

Usage:  python tools/dsp/design_resampler.py

Writes include/SushiDSP/math/ResamplerDesign.hpp and src/math/ResamplerKernel.hpp,
both relative to the repository root.

Kaiser-windowed sinc, table-driven for polyphase interpolation, normalised so every
polyphase branch has unity DC gain rather than the peak; HALF_WIDTH is 34, not the plan's
32, per docs/superpowers/plans/2026-09-05-dr1-dr2-resampler-and-dd3.md Task 1.1 ruling.
"""
import sys
import numpy as np
import scipy
from scipy.signal import freqz

HALF_WIDTH = 34          # zero crossings each side (33 clears -100 dB with only 0.08 dB of
                         # margin at the true worst frequency; 34 gives ~0.45 dB of margin -
                         # see the DR1+DR2 plan, Task 1.1 ruling on the stopband conflict)
PHASES = 512             # table entries per zero crossing
CUTOFF = 0.45            # normalised to the lower of the two sample rates
PASSBAND_EDGE = 0.400
BETA = 10.0613           # 0.1102 * (100 - 8.7), for a 100 dB stopband
GRID = 1 << 19
BASE_SAMPLE_RATE_KHZ = 48.0  # convention used project-wide for the latency-floor figure


def phase_sums(kernel):
    """Compute every polyphase branch's DC sum over the right-wing kernel.

    Args:
        kernel: Right wing of the kernel, length HALF_WIDTH * PHASES + 1,
            sampled at t = 0 .. HALF_WIDTH.

    Returns:
        A length-PHASES array; entry p is phase p's DC gain, summed over both
        wings and all HALF_WIDTH zero crossings.
    """
    sums = np.zeros(PHASES)
    for phase in range(PHASES):
        total = 0.0
        for tap in range(HALF_WIDTH):
            right = tap * PHASES + phase
            left = (tap + 1) * PHASES - phase
            total += kernel[right]
            if left < len(kernel):
                total += kernel[left]
        sums[phase] = total
    return sums


def design():
    """Build the right-wing Kaiser-windowed sinc kernel plus its guard entry.

    Returns:
        A tuple (kernel, dc_sums) where kernel has HALF_WIDTH * PHASES + 2
        entries (peak, right wing, guard) and dc_sums holds the 512 measured
        per-phase DC gains after normalisation.
    """
    n = HALF_WIDTH * PHASES
    t = np.arange(n + 1) / PHASES
    kernel = 2.0 * CUTOFF * np.sinc(2.0 * CUTOFF * t)
    kernel *= np.kaiser(2 * n + 1, BETA)[n:]
    # The last table entry (t == HALF_WIDTH) sits at the Kaiser window's tapered edge and is
    # already negligible; it is the interpolator's outermost table point, so it is forced to
    # the exact zero the interface promises rather than left at a near-zero float.
    kernel[-1] = 0.0
    kernel /= phase_sums(kernel).mean()
    return np.concatenate([kernel, [0.0]]), phase_sums(kernel)


def measure_response(kernel):
    """Measure the interpolating filter's passband ripple and stopband level.

    Args:
        kernel: Right wing plus guard entry, as returned by design().

    Returns:
        A tuple (worst_passband_db, worst_stopband_db) measured on the full
        symmetric kernel evaluated at the oversampled (512x) rate.
    """
    full = np.concatenate([kernel[-2:0:-1], kernel[:-1]])
    w, response = freqz(full, worN=GRID, fs=PHASES)
    # freqz's raw DC gain is the tap count density (~PHASES); the interpolating filter's own
    # gain is that response divided by PHASES, matching the continuous-time integral it applies.
    magnitude_db = 20.0 * np.log10(np.maximum(np.abs(response) / PHASES, 1e-300))
    passband = magnitude_db[(w >= 0.0) & (w <= PASSBAND_EDGE)]
    stopband = magnitude_db[w >= 0.5]
    worst_passband = np.abs(passband).max()
    worst_stopband = stopband.max()
    return worst_passband, worst_stopband


def measure_interpolation_bound(kernel):
    """Measure the worst second-difference linear-interpolation error bound.

    Args:
        kernel: Right wing plus guard entry, as returned by design().

    Returns:
        max |h[i+1] - 2*h[i] + h[i-1]| / 8 over the interior of the table,
        excluding the guard entry so this covers the same index set as the
        committed test.
    """
    interior = kernel[:-1]
    second_difference = interior[2:] - 2.0 * interior[1:-1] + interior[:-2]
    return np.abs(second_difference).max() / 8.0


DESIGN_HEADER_PATH = "include/SushiDSP/math/ResamplerDesign.hpp"
KERNEL_HEADER_PATH = "src/math/ResamplerKernel.hpp"


def licence_block(file_name):
    """Build the repository licence block carrying one file's name.

    Args:
        file_name: Bare header file name to write into the block's name line.

    Returns:
        The licence block as a newline-terminated string.
    """
    header = open("include/SushiDSP/core/PortLayout.hpp", encoding="utf-8").read().split("\n")
    licence = header[:22]
    name_line = f"/* {file_name}"
    licence[1] = name_line + " " * (76 - len(name_line) - 2) + "*/"
    return "\n".join(licence) + "\n"


def banner_block(measurements):
    """Build the generated-by comment both headers carry.

    Args:
        measurements: Mapping as returned by measure_all().

    Returns:
        The comment block as a newline-terminated string.
    """
    return (
        "// GENERATED by tools/dsp/design_resampler.py - do not edit by hand.\n"
        f"// scipy {scipy.__version__}, numpy {np.__version__}, verification grid {GRID}.\n"
        f"// Measured: passband ripple {measurements['passband']:.4f} dB over "
        f"[0, {PASSBAND_EDGE}], stopband {measurements['stopband']:.2f} dB over "
        f"[0.5, {PHASES / 2}],\n"
        f"// polyphase DC sum spread {measurements['dc_spread']:.2e}, "
        f"linear-interpolation bound {measurements['interpolation_bound']:.4e}.\n"
        f"// HALF_WIDTH={HALF_WIDTH} (grown from the planned 32 to clear -100 dB with\n"
        f"// margin, DR1+DR2 plan Task 1.1 ruling): latency floor "
        f"{measurements['latency_ms']:.4f} ms/stage at {BASE_SAMPLE_RATE_KHZ} kHz.\n"
    )


def measure_all(kernel, dc_sums):
    """Measure the designed kernel and report the numbers both headers quote.

    Args:
        kernel: Right wing plus guard entry, as returned by design().
        dc_sums: Per-phase DC sums, as returned by design().

    Returns:
        A mapping with keys passband, stopband, dc_spread, interpolation_bound and latency_ms.

    Raises:
        SystemExit: If the design no longer clears its interpolation or stopband targets.
    """
    worst_passband, worst_stopband = measure_response(kernel)
    measurements = {
        "passband": worst_passband,
        "stopband": worst_stopband,
        "dc_spread": dc_sums.max() - dc_sums.min(),
        "interpolation_bound": measure_interpolation_bound(kernel),
        "latency_ms": HALF_WIDTH / BASE_SAMPLE_RATE_KHZ,
    }
    print(f"passband ripple {measurements['passband']:.4f} dB over [0, {PASSBAND_EDGE}]",
          file=sys.stderr)
    print(f"stopband level {measurements['stopband']:.2f} dB over [0.5, {PHASES / 2}]",
          file=sys.stderr)
    print(f"polyphase DC sum spread {measurements['dc_spread']:.2e} (mean 1.0 by construction)",
          file=sys.stderr)
    print(f"linear-interpolation bound {measurements['interpolation_bound']:.4e} (limit 4.0e-6)",
          file=sys.stderr)
    print(f"latency floor {measurements['latency_ms']:.4f} ms/stage at {BASE_SAMPLE_RATE_KHZ} kHz "
          f"({HALF_WIDTH} zero crossings)", file=sys.stderr)
    if measurements["interpolation_bound"] >= 4.0e-6:
        raise SystemExit("interpolation bound exceeds N1's headroom")
    if measurements["stopband"] > -100.0:
        raise SystemExit("HALF_WIDTH no longer clears the -100 dB stopband target")
    # The brief names no numeric passband-ripple target, so it is measured and printed
    # above but not gated.
    return measurements


def emit_design(out, measurements):
    """Write the public design-constant header to out.

    Args:
        out: A text-mode writable stream.
        measurements: Mapping as returned by measure_all().
    """
    out.write(licence_block("ResamplerDesign.hpp") + "\n")
    out.write(banner_block(measurements) + "\n")
    out.write("#pragma once\n\n#include <cstdint>\n\nnamespace SushiDSP\n{\n")
    out.write("    /// @brief Input samples stored on each side of the resampler prototype's peak\n")
    out.write("    /// (not the count of true zero crossings, which is lower since fc < 0.5).\n")
    out.write(f"    inline constexpr std::uint32_t kResamplerHalfWidthZeroCrossings "
              f"= {HALF_WIDTH}u;\n\n")
    out.write("    /// @brief Table entries stored per zero crossing (the polyphase "
              "interpolation density).\n")
    out.write(f"    inline constexpr std::uint32_t kResamplerPhasesPerZeroCrossing = {PHASES}u;\n\n")
    out.write(f"    /// @brief Length of the kernel table in {KERNEL_HEADER_PATH}: peak, "
              f"right wing\n")
    out.write("    /// and one guard entry.\n")
    out.write(f"    inline constexpr std::uint32_t kResamplerKernelLength "
              f"= {HALF_WIDTH * PHASES + 2}u;\n\n")
    out.write("    /// @brief Cutoff of the prototype's ideal lowpass, in cycles per input sample.\n")
    out.write(f"    inline constexpr float kResamplerCutoff = {CUTOFF}f;\n\n")
    out.write("    /// @brief Edge of the band over which the passband ripple above is measured.\n")
    out.write(f"    inline constexpr float kResamplerPassbandEdge = {PASSBAND_EDGE}f;\n\n")
    out.write("    /// @brief Worst stopband level measured on the oversampled kernel, in dB.\n")
    out.write(f"    inline constexpr float kResamplerStopbandDecibels "
              f"= {measurements['stopband']:.3f}f;\n")
    out.write("}\n")


def emit_kernel(out, kernel, measurements):
    """Write the private kernel-table header to out.

    Args:
        out: A text-mode writable stream.
        kernel: Right wing plus guard entry, as returned by design().
        measurements: Mapping as returned by measure_all().
    """
    out.write(licence_block("ResamplerKernel.hpp") + "\n")
    out.write(banner_block(measurements) + "\n")
    out.write("#pragma once\n\n")
    out.write('#include "SushiDSP/math/ResamplerDesign.hpp"\n\nnamespace SushiDSP\n{\n')
    out.write("    /// @brief Right wing of the even Kaiser-windowed sinc kernel: kResamplerKernel[i]\n")
    out.write("    /// is h(i / kResamplerPhasesPerZeroCrossing) in input samples. Entry 0 is the peak,\n")
    out.write("    /// which is 2*kResamplerCutoff (the centre tap of a sub-Nyquist lowpass impulse\n")
    out.write("    /// response), not 1.0 - the kernel is normalised on the polyphase DC sums, which\n")
    out.write("    /// are already ~1 for any cutoff below 0.5 and barely move the peak. The last two\n")
    out.write("    /// entries are the exact-zero endpoint and guard.\n")
    out.write("    inline constexpr float kResamplerKernel[kResamplerKernelLength] =\n    {\n")
    length = HALF_WIDTH * PHASES + 2
    for i in range(0, length, 4):
        row = ", ".join(f"{float(np.float32(v)):.9e}f" for v in kernel[i:i + 4])
        out.write(f"        {row},\n")
    out.write("    };\n")
    out.write("}\n")


def main():
    """Design the prototype and rewrite both generated headers in place."""
    kernel, dc_sums = design()
    measurements = measure_all(kernel, dc_sums)
    with open(DESIGN_HEADER_PATH, "w", encoding="utf-8", newline="\n") as out:
        emit_design(out, measurements)
    with open(KERNEL_HEADER_PATH, "w", encoding="utf-8", newline="\n") as out:
        emit_kernel(out, kernel, measurements)


if __name__ == "__main__":
    main()
