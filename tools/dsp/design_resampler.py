"""Design the windowed-sinc resampler prototype filter and emit the C++ header.

Usage:  python tools/dsp/design_resampler.py > include/SushiDSP/math/ResamplerCoefficients.hpp

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


def emit(out):
    """Write the generated ResamplerCoefficients.hpp header to out.

    Args:
        out: A text-mode writable stream (stdout when run as a script).
    """
    kernel, dc_sums = design()
    worst_passband, worst_stopband = measure_response(kernel)
    interpolation_bound = measure_interpolation_bound(kernel)
    dc_spread = dc_sums.max() - dc_sums.min()
    latency_floor_ms = HALF_WIDTH / BASE_SAMPLE_RATE_KHZ

    print(f"passband ripple {worst_passband:.4f} dB over [0, {PASSBAND_EDGE}]", file=sys.stderr)
    print(f"stopband level {worst_stopband:.2f} dB over [0.5, {PHASES / 2}]", file=sys.stderr)
    print(f"polyphase DC sum spread {dc_spread:.2e} (mean 1.0 by construction)", file=sys.stderr)
    print(f"linear-interpolation bound {interpolation_bound:.4e} (limit 4.0e-6)", file=sys.stderr)
    print(f"latency floor {latency_floor_ms:.4f} ms/stage at {BASE_SAMPLE_RATE_KHZ} kHz "
          f"({HALF_WIDTH} zero crossings)", file=sys.stderr)
    if interpolation_bound >= 4.0e-6:
        raise SystemExit("interpolation bound exceeds N1's headroom")
    if worst_stopband > -100.0:
        raise SystemExit("HALF_WIDTH no longer clears the -100 dB stopband target")
    # The brief names no numeric passband-ripple target, so it is measured and printed
    # above but not gated.

    header = open("include/SushiDSP/core/PortLayout.hpp", encoding="utf-8").read().split("\n")
    licence = header[:22]
    name_line = "/* ResamplerCoefficients.hpp"
    licence[1] = name_line + " " * (76 - len(name_line) - 2) + "*/"
    out.write("\n".join(licence) + "\n\n")
    out.write("// GENERATED by tools/dsp/design_resampler.py - do not edit by hand.\n")
    out.write(f"// scipy {scipy.__version__}, numpy {np.__version__}, verification grid {GRID}.\n")
    out.write(f"// Measured: passband ripple {worst_passband:.4f} dB over [0, {PASSBAND_EDGE}], "
              f"stopband {worst_stopband:.2f} dB over [0.5, {PHASES / 2}],\n")
    out.write(f"// polyphase DC sum spread {dc_spread:.2e}, "
              f"linear-interpolation bound {interpolation_bound:.4e}.\n")
    out.write(f"// HALF_WIDTH={HALF_WIDTH} (grown from the planned 32 to clear -100 dB with\n")
    out.write(f"// margin, DR1+DR2 plan Task 1.1 ruling): latency floor "
              f"{latency_floor_ms:.4f} ms/stage at {BASE_SAMPLE_RATE_KHZ} kHz.\n\n")
    out.write("#pragma once\n\n#include <cstdint>\n\nnamespace SushiDSP\n{\n")
    out.write("    /// @brief Input samples stored on each side of the resampler prototype's peak\n")
    out.write("    /// (not the count of true zero crossings, which is lower since fc < 0.5).\n")
    out.write(f"    inline constexpr std::uint32_t kResamplerHalfWidthZeroCrossings = {HALF_WIDTH}u;\n\n")
    out.write("    /// @brief Table entries stored per zero crossing (the polyphase interpolation density).\n")
    out.write(f"    inline constexpr std::uint32_t kResamplerPhasesPerZeroCrossing = {PHASES}u;\n\n")
    out.write("    /// @brief Length of @ref kResamplerKernel: peak, right wing and one guard entry.\n")
    length = HALF_WIDTH * PHASES + 2
    out.write(f"    inline constexpr std::uint32_t kResamplerKernelLength = {length}u;\n\n")
    out.write("    /// @brief Cutoff of the prototype's ideal lowpass, in cycles per input sample.\n")
    out.write(f"    inline constexpr float kResamplerCutoff = {CUTOFF}f;\n\n")
    out.write("    /// @brief Edge of the band over which the passband ripple below is measured.\n")
    out.write(f"    inline constexpr float kResamplerPassbandEdge = {PASSBAND_EDGE}f;\n\n")
    out.write("    /// @brief Worst stopband level measured on the oversampled kernel, in dB.\n")
    out.write(f"    inline constexpr float kResamplerStopbandDecibels = {worst_stopband:.3f}f;\n\n")
    out.write("    /// @brief Right wing of the even Kaiser-windowed sinc kernel: kResamplerKernel[i]\n")
    out.write("    /// is h(i / kResamplerPhasesPerZeroCrossing) in input samples. Entry 0 is the peak,\n")
    out.write("    /// which is 2*kResamplerCutoff (the centre tap of a sub-Nyquist lowpass impulse\n")
    out.write("    /// response), not 1.0 - the kernel is normalised on the polyphase DC sums, which\n")
    out.write("    /// are already ~1 for any cutoff below 0.5 and barely move the peak. The last two\n")
    out.write("    /// entries are the exact-zero endpoint and guard.\n")
    out.write("    inline constexpr float kResamplerKernel[kResamplerKernelLength] =\n    {\n")
    for i in range(0, length, 4):
        row = ", ".join(f"{float(np.float32(v)):.9e}f" for v in kernel[i:i + 4])
        out.write(f"        {row},\n")
    out.write("    };\n")
    out.write("}\n")


if __name__ == "__main__":
    emit(sys.stdout)
