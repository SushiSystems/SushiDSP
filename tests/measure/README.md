# tests/measure

Header-only measurement helpers that the functional tests and the block tests share. Each one
drives an `AudioNode` or reads a signal and returns a figure in dB or percent. No product
includes these headers.

| File | What it holds |
| --- | --- |
| `render.hpp` | `render`, which streams a stimulus through a node block by block, and `sine_stimulus` |
| `window.hpp` | The four-term Blackman-Harris window |
| `spectrum.hpp` | The 65536-point magnitude spectrum and the bin helpers that read it |
| `thd.hpp` | Total harmonic distortion at a bin-exact fundamental |
| `alias_floor.hpp` | The worst non-harmonic line across 1 to 20 kHz tones |
| `frequency_response.hpp` | Magnitude response by exponential sine sweep |
| `null_test.hpp` | The residual of one signal against another after alignment |
| `spice_csv.hpp` | Loaders for the CSVs `tools/spice/export.py` writes |

## alias_floor.hpp

`alias_floor` renders one bin-exact tone per kilohertz from 1 kHz to 20 kHz. Each tone runs for
one second of settling plus one analysis frame. Lines within four bins of DC, of the
fundamental, or of any harmonic below Nyquist are excluded. A harmonic that would fall above
Nyquist has folded back and is counted as an alias, which is what the number is for.

The default stimulus is -12 dBFS. A node that clips inside its own chain at this level reports
that clipping's folded harmonics, not its conversion, so sweep such a node at the operating
point its data sheet or service note publishes.

## frequency_response.hpp

`frequency_response` sweeps from 5 Hz to half the sample rate over two seconds at -40 dBFS, then
holds one second of silence for the tail.

The sweep starts two octaves below the lowest frequency any caller reports. An exponential sine
sweep carries no energy below its own start frequency, and the abrupt onset at the start
frequency leaves the first few bins of the deconvolved response contaminated by that
truncation. Measured against an analytic second-order Butterworth high-pass at 192 kHz:

| Sweep start | Corner | Reading at 20 Hz | Analytic | Error |
| --- | --- | --- | --- | --- |
| 20 Hz | 87 Hz | -27.250 dB | -25.552 dB | -1.699 dB |
| 20 Hz | 20 Hz | -6.386 dB | -3.010 dB | -3.375 dB |
| 5 Hz | 87 Hz and 20 Hz | | | -0.068 dB |

With the 5 Hz start, the worst error anywhere from 20 Hz up is 0.17 dB.

The inverse filter is the time-reversed sweep with a 6 dB per octave amplitude envelope
(Farina). The envelope rises along the original sweep's time axis, so it compensates the
sweep's 1/f energy density: the low frequencies at the sweep's start are attenuated and the
high frequencies at its end are not. Weighting it the other way round attenuates the highs
twice over and deconvolves to a low-frequency blob where a delta should be.

The linear impulse response is the circular convolution of the output and the inverse filter.
Harmonic products land before the linear response and are excluded by keeping only the causal
second. The stimulus itself is deconvolved the same way and used as the 0 dB reference.

## null_test.hpp

`null_test` picks the lag in `[-max_lag, max_lag]` that maximises the magnitude of the
cross-correlation. A `b` that is inverted relative to `a`, as it is after an odd number of
inverting stages in the path under test, therefore still aligns on its true lag. Maximising the
signed correlation would align it on whichever lag made that correlation least negative. The
least-squares gain then comes out negative on its own.

The gain is the least-squares scalar over the overlapping region. The residual is
`20*log10(rms(a - g*b_shifted) / rms(a))`, measured over the same region. The samples the shift
leaves without a partner in `b` say nothing about how well the two signals match, and counting
them would floor the result at the ratio of the lag to the length.

The cost is O(length * max_lag), so pass a small `max_lag` when the delay is known to be small.

## spice_csv.hpp

`split_csv_line` strips carriage returns so that `std::stod` never sees one. They appear when a
file authored on Windows is read in text mode elsewhere.

`load_ac_csv` and `load_transient_csv` skip the single header line, blank lines and any line
with fewer than four fields. A missing or unreadable file is not an error. It yields an empty
result, which lets a test that needs the corpus skip cleanly instead of aborting.

`load_transient_csv` derives the sample rate as `1 / (t[1] - t[0])` and leaves it at 0 when
fewer than two rows are present or the first two time stamps coincide.
