## `test_biquad.cpp`

Every gain row drives the filter with a sine of a whole number of cycles, settles it for twice
ln(u)/ln(r) samples (r is the largest pole radius, found by mapping the analog poles through
z = (1 + t p)/(1 - t p)), and reads the amplitude by projecting the output onto sine and cosine.
The sampled peak is not used: it misses the crest by up to 0.9 %. The error bound of a reading is
4 u ||h||_1 (float rounding of the input and of the output, ||h||_1 the l1 norm of the impulse
response) plus the design budget below. The oracles are the analog prototypes of the RBJ designs
with the corner prewarped, which the bilinear map reproduces exactly, so the first-order shelf and
the one-pole low-pass carry no budget.

### `Small_BiquadMagnitude`

Rows: `LowPassPassesDcAtUnityGain` to `PeakingLeavesFarFrequenciesNearUnity` are the RBJ designs
against `1/(s^2 + s/Q + 1)` and its shelf and peaking forms; `OnePoleLowPassAt1kHz` and
`...AtTwiceItsCorner` against `1/(1 + jT/t_c)`; `FirstOrderShelfAt...` against
`g (1 + jT/t_z)/(1 + jT/t_p)` for the DD-3 pre-emphasis corners (zero 594.07 Hz, pole 3386.28 Hz,
low-frequency gain 33/47) at 96 kHz; `EqualCornerShelfAt...` where zero and pole cancel and leave
the flat gain 2; `FirstOrderHighPassIs...` against the analog RC `j(f/fc)/(1 + j(f/fc))`. The
impulse-invariant high-pass has a budget of omega_c^2 in relative gain, the order of the first
term of its map that the RC leaves out; by hand it reads 2e-5 at the corner and at a decade
(budget 1.7e-4 at fs = 48 kHz, fc = 100 Hz).

### `OnePoleLowPassLosesSixDecibelsPerOctaveFarAboveItsCorner`

A one-pole loses 6.02 dB per octave, not the 12 dB of a Butterworth biquad. The probes sit at 40
and 80 times the corner at 384 kHz; the bilinear warp lifts them by at most
20 log10(e) x^2/2 dB (tan x <= x(1 + x^2/2) for x < 0.5) and the 1 under the root by at most
10 log10(e)/omega^2 dB, and the test allows exactly that.

### `FirstOrderShelfPairCancelsToTheInputWithinRounding`

Pre-emphasis (zero 594.07, pole 3386.28, gain 33/47) followed by the swapped pair (gain 47/33)
is the identity, because the numerator of one is the denominator of the other and the gains
multiply to 1. The error bound is u (max|middle| ||h_de||_1 + max|input|): the float rounding of
the middle signal, filtered by the second stage, plus the float rounding of the output.

### `Small_BiquadLowCorner`

The corner range that makes `set_high_pass` diverge at the 384 kHz oversampled rate: there the
conjugate pole pair rounds onto the unit circle, while the one-pole's real pole R = exp(-2 pi fc/fs)
does not. `PassbandGainMatchesTheAnalogRc` reads the 1 kHz gain. `StepResponseDecaysAsTheRealPolePowerN`
uses y[n] = y[0] R^n after the first sample (b0 + b1 is exactly zero, so the recursion is a pure
multiplication by R); the relative bound is 2 u_f for the two float roundings plus 3 n u_d for the
state recursion and the library's one-ulp error in R.

### `BiquadSetCoefficientsLoadsThemVerbatim`

`// The impulse response is 1, 1/2, 1/4, 1/8.`

b = {1, 0, 0}, a = {-0.5, 0} is the one-pole y[n] = x[n] + 0.5 y[n-1], whose impulse response is
the geometric sequence 1, 1/2, 1/4, 1/8.

## `test_fft.cpp`

The bound of every transform check is the radix-2 result of Higham (Accuracy and Stability of
Numerical Algorithms, theorem 24.2): the 2-norm error is at most log2(N) eta / (1 - log2(N) eta)
times the 2-norm of the result, with eta = u + gamma4 (sqrt 2 + u) for twiddles rounded to float.
At N = 1 the bound is zero and the transform must be exact.

### `InverseReconstructsTheInput`

`// Each pass adds log2(N) eta of the norm; the scale adds u.`

The forward pass leaves log2(N) eta of ||y||. The inverse divides that error by sqrt(N) and the
input has sqrt(N) less norm than the spectrum, so it adds log2(N) eta of ||x|| again, and the
1/N scale adds u.

### `SingleBinCosineHasHalfNAtItsBinAndZeroElsewhere`

`// The cosine puts N/2 on its bin and on the mirrored one.`

A real cosine at bin k has N/2 at bins k and N - k and zero elsewhere. The float rounding of the
input adds sqrt(N) u ||x|| to the bound.

## `test_partitioned_convolution.cpp` and `test_streaming_convolution.cpp`

Both compare the float FFT convolution with the direct form in double precision. The bound is
`TestSupport::partitioned_block_error_bound` in `fft_error_bound.hpp`: per output block, the
2-norm error is at most (3 r + (P + 2) u) times P times the largest input-block 2-norm times the
largest impulse-response-slice l1 norm, where r is the relative bound of the 2N-point float FFT
(`fft_relative_bound`) and P the partition count. The three r are the two forward transforms and
the inverse; the (P + 2) u is the float sum of the P spectra and the overlap-add. The product error
is taken against the larger spectrum, whose sup norm is at most the slice's l1 norm. A sample's
error is at most its block's 2-norm error. The streaming test adds nothing to the bound: its
re-blocking only copies samples.

## `test_newton_raphson_solver.cpp`

### `JointOverloadMatchesSplitOverloadBitForBit`

`// Both overloads must walk identical iterates.`

The circuit stages moved to the joint form to share one exp() between residual and derivative;
that must be a pure cost change, so both forms must walk identical iterates.

## `test_oversampler.cpp`

The rejection and droop limits are the spec levels of D6a, named in the file
(`K_SPEC_REJECTION_DB` = -90 dB, `K_SPEC_DROOP_DB` = 0.1 dB), not tolerances of the arithmetic.
The halfband stages are checked against the direct form with a derived bound: the float
accumulator sums (taps - 1)/4 + 1 pair terms, each with the rounding of its pair sum and of its
product, and the double-precision reference is rounded to float once, so the error is at most
(pairs + 4) u Σ|h| max|x'|, with x' the zero-stuffed input. The zero taps and the unity DC gain of
the designs are held by `test_halfband_coefficients.cpp`.

### `ImagesOfAnInBandToneAreBelowTheSpecRejection`

`// A tone's images sit at k*f_base +- f.`

Spec D6a: image rejection >= 90 dB at 0.3\*f_base and beyond. A tone's images sit at k\*f_base
+- f; every line in the 8x spectrum other than the tone must be >= 90 dB down.

### `AliasesOfAnOutOfBandToneAreBelowTheSpecRejection`

`// Folds into (0.45..0.5)*f_base are not in the rows.`

Tones at the 8x rate from 0.55\*f_base to 3.95\*f_base in 0.05 steps, decimated to base rate:
whatever folds into [0, 0.45\*f_base] must be >= 90 dB below the tone. Folds that land in the
stage-1 transition band (0.45..0.5)\*f_base are the filter's don't-care region and have no row.
The reference level is the same-amplitude tone analysed with the same window at base rate; it is
bin-exact, as the folded aliases are, because a tone off-bin loses up to 0.83 dB to
Blackman-Harris scalloping and would flatter every alias by that much. The steps are integers:
accumulating 0.05 in double drifts, and near-integer fractions then fold to ~1e-7 instead of ~0.

### `RoundTripDroopAt20kIsWithinTheSpecDroop`

`// 20 kHz is 0.4167*f_base, inside the 0.45 passband.`

20 kHz at 48 kHz is 0.4167\*f_base, inside the 0.45 passband. The tone is bin-exact over the
65536-sample window, so the power ratio of the settled half is free of leakage. The rows are
Oversampler4x (two stages) and Oversampler8x (six).

### `LatencyMatchesMeasuredImpulseDelay`

`// The reported floor is within one sample of the peak.`

The 4x impulse peak is the round trip: 31.75 base-rate samples up and 31.75 back, 63.5 in all. The
27-tap stage makes it fractional - it runs at 4x, so its 13 taps of delay are 3.25 base-rate
samples each way and 6.5 together. The 115-tap stage contributes 28.5 each way, a whole 57. See the
derivation in Oversampler4x::latency_samples. The reported floor is within one sample of the peak,
for both classes.

### `ReturnsTheStageToItsAsConstructedState`

`// A reset instance matches a never-used one bit for bit.`

An instance that has processed a burst and then been reset must produce bit-identical output to
a never-used instance, on the halfband upsampler and downsampler and on both oversamplers.
Oversampler4x is here because it is what every shipping D6c node runs; TubeScreamer::reset() is
only as good as this. The oversamplers' buffers are internal and reused, so the oversampled block
is copied out before it is decimated.

### `Oversampler8xPassesDcAtUnity`

The bound is the sum over the three designs of u (pairs + 3) 3 Σ|h| + 2 u Σ|h|: the accumulators
of the up and the down stage, whose terms add to at most 3 Σ|h|, and the table's float rounding in
each direction.

## `test_resampler.cpp`

### `ResamplerHoldsADcLevelAtEveryRatio`

`// Every polyphase branch has unity DC gain.`

Every polyphase branch has unity DC gain and the decimating scale factor restores it, so a
constant comes back as the same constant whichever way the rate moves.

### `ResamplerRoundTripNulls`

`// The round trip is limited by the kernel's own error.`

`// The 7 kHz content sits inside the 8000 Hz passband of the 20 kHz stage.`

`// 12 kHz content survives at the SPX90 ratio.`

One row per clock. Dd3SlowClock is 48 kHz -> 20 kHz -> 48 kHz. The content stops at 7 kHz, which
is the pedal's own delay-path limit and is inside the prototype's passband (0.400 \* 20000 =
8000 Hz, N1), so the round trip is limited by the kernel's own error: -100 dB stopband, -108 dB
interpolation floor, about -97 dB for the two stages in power. -80 dB is asserted with 17 dB of
margin for the float buffers. Dd3FastClock is 48 kHz -> 82.5 kHz -> 48 kHz. Spx90Ratio is
48 kHz -> 31.25 kHz -> 48 kHz (the SPX90's 1.536); its passband reaches 0.400 \* 31250 =
12500 Hz, which is exactly the unit's published 20 Hz - 12 kHz band, so 12 kHz content must
survive.

### `ResamplerSizesTheHistoryForAnUpsampleOnlyInstance`

`// The history is sized on min(1, minimum_ratio).`

An interpolating ratio never widens the kernel past its own 34 input samples, so a minimum_ratio
above 1 must size the history on min(1, minimum_ratio). Dividing by the minimum_ratio alone
reserves 64 slots here against a live reach of 76 and the ring wraps on top of itself, which
shows up as a round trip that does not null.

### `ResamplerRejectsAToneAboveTheOutputNyquist`

`// 15 kHz into 20 kHz would fold to 5000 Hz.`

15 kHz into a 20 kHz output rate would fold to |15000 - 20000| = 5000 Hz. Normalised to the
lower rate, 15 kHz is 0.75 cycles/sample, deep in the prototype's stopband (-100.477 dB), so the
image must land below -90 dB relative to the input tone, 10 dB above the stopband. The input is
one transform window plus the start-up long, since only the last window is analysed.

`// The 0 dB reference is the input at its own rate.`

0 dB reference: what the same tone would read if it had passed unattenuated. Use the input
itself, analysed at its own rate, so the two spectra share a window and a length.

### `ResamplerPassesAToneAtThePassbandEdgeAtItsLevel`

`// 0.399 of the source rate is inside the 0.4 passband edge.`

Upsampling 48 kHz to 96 kHz runs the kernel at its own time scale, so the response a tone sees
is the prototype's. 19.15 kHz is 0.399 cycles per input sample, just inside
K_RESAMPLER_PASSBAND_EDGE, and must keep its level to 0.1 dB against the input analysed at its
own rate. The frequency sits on a bin of both rates' transforms, so neither reading scallops.

### `ResamplerSuppressesTheImageOfAnUpsampledTone`

`// The image of f sits at 48000 - f, inside the stopband.`

Upsampling 48 kHz to 96 kHz leaves an image of every input tone mirrored about 24 kHz, which
only the kernel's stopband removes. 14.4 kHz images to 33.6 kHz, 0.7 cycles per input sample,
deep in the stopband. The kernel's stopband measures K_RESAMPLER_STOPBAND_DECIBELS = -100.5 dB
and linear interpolation between its entries adds an error near -108.4 dB (20 log10 of 3.8127e-6);
the two add in power to -99.8 dB, so the image must sit 95 dB under the tone, 4.8 dB above that.

### `ResamplerOutputCountFollowsTheRatioWithoutDrift`

`// The output count stays within the start-up deficit of the ratio.`

Over a million frames the accumulated output count must stay within 35 of input_frames \* ratio:
the start-up deficit is at most the latency in output frames (34) and the floor of the phase
adds one. A phase accumulator that rebases badly drifts here and nowhere else. The case is
`Medium_`: a million frames at the widest kernel are 70 million multiply-adds.

### `ResamplerResumesAfterAnUnderSizedOutputBuffer`

`// Deferred outputs come out on the next call.`

A short output buffer defers the outputs it had no room for. While the ring still holds their
history the next call emits them, so the stream is the one an always-fed instance produces,
sample for sample. Every position here is an integer, so equality is exact.

### `ResamplerNeverProducesMoreThanTheCapacityItWasGiven`

`// process() never writes more than it has room for.`

A buffer of 100 for 256 frames due, block after block: no call may report more outputs than the
capacity it was handed, and every call still produces some.

### `ResamplerBoundsThePhaseWhenTheOutputBufferStaysTooSmall`

`// process() drops the backlog instead of wrapping.`

Truncation that never lets up cannot be deferred forever: the ring holds one span of history,
and past that process() drops the backlog instead of reading slots it has overwritten. The proof
that it drops rather than wraps is the tail: once the buffer is big enough again, the output
lines up with the live input to better than -60 dB.

### `ResamplerReclampsThePhaseWhenARateChangeWidensTheKernel`

`// y[n-1] + y[n+1] = 2*cos(w)*y[n] for a sampled sinusoid.`

The one way a phase can start a call below the floor: starve it until it pins on the 1:1 floor,
then decimate 16:1. The half span grows from 34 to 544 input samples, which lifts the floor 510
samples above where the phase sits, and only a clamp on entry keeps the outermost taps out of
slots the ring has overwritten. The check is the identity every uniformly sampled sinusoid
obeys, y[n-1] + y[n+1] = 2\*cos(w)\*y[n]: it needs neither the phase nor the amplitude of the
output grid, and one overwritten tap breaks it. At 16:1 every output reads the table at the same
phase, so the output is a sinusoid up to float rounding and the bound is (2 + |2 cos w|) \* 3 u:
three float terms, each off by u in its own rounding and by u more in the input sine that a
kernel of absolute sum two passes through.

### `ResamplerLatencyIsThirtyFourSamplesOfTheLowerRate`

`// The delay is 34 samples of the lower rate, counted in source samples and rounded.`

DR1+DR2 plan N1, as the Task 1.1 ruling widened it: the delay is
K_RESAMPLER_HALF_WIDTH_ZERO_CROSSINGS = 34 divided by min(source, target) seconds at every ratio.
One row per pair of rates; the seconds are held to four rounded operations in double.

### `ResamplerKeepsAToneAtItsFrequency`

`// A bin-exact tone lands on its own bin.`

A 1 kHz tone chosen to sit on a bin of the 20 kHz transform comes out of the 48 kHz to 20 kHz
stage with its strongest bin on that bin exactly: the main lobe of a bin-exact tone peaks on its
own bin, and the resampler is linear.

### `ResamplerResetReturnsItToSilence`

`// Reset leaves no tail of the loud signal.`

The resampler is fed a loud constant, reset, then fed silence. Every output after the reset is
exactly zero, which a retained tail would break.

### `ResamplerRateChangeMidStreamIsContinuous`

`// At 20 kHz a 100 Hz sine steps at most 0.0314.`

The DD-3 usage: set_rates() every 8 frames while the target sweeps 82.5 kHz -> 20 kHz. A phase
reset or a bank rebuild shows up here as a step; a correct accumulator does not. Reference: a
100 Hz unit sine sampled at 20 kHz moves at most 2\*pi\*100/20000 = 0.0314 per sample. The bound
is that step grown by the 0.1 dB passband gain bound and two float roundings, 0.0318. The first
64 outputs, the kernel half span in output frames at the 82.5 kHz start, are not counted: the
zero history is not a sine's past.

### `ProcessPerformsNoHeapAllocation`

`// At the slowest clock interpolate() touches 164 taps.`

The DD-3's decimating leg at the VCO's slowest clock, which is where the Resampler is widest:
the prototype's half width of 34 (input samples; true crossings are fewer at fc = 0.45)
stretches to 34 / (20000 / 48000) = 81.6, so interpolate() runs two loops of 82 and touches 164
taps per output, and prepare() sizes the history ring for exactly that.

### `RateChangePerformsNoHeapAllocation`

`// One 256-frame block calls set_rates 32 times.`

The case plan Ruling 3 rests on. The DD-3 re-clocks every 8 frames, so one 256-frame block calls
set_rates 32 times; a ratio change that rebuilt a filter bank would put the node off the audio
thread entirely. The sweep runs the VCO's whole 20-82.5 kHz range, so the ratio reaches both
ends of the clamp prepare() was given.

## `compile_resampler_design.cpp`

The kernel is 34 zero crossings \* 512 entries + 1 endpoint + 1 guard = 17410 long. HALF_WIDTH is
34 rather than 32 so the stopband clears its -100 dB target with margin. The table's own
properties, a unity DC sum on every polyphase branch and a linear-interpolation error bound of
3.8127e-6, are measured by `tools/dsp/design_resampler.py` when it writes the table, and reach the
tests through `Resampler` in `test_resampler.cpp`. The header has no test of its own: its
constants are not behaviour, and the file only keeps it compiling.

## `benchmark_resampler.cpp`

### `measure_resampler_round_trip`

48 kHz down to 20 kHz and back is the resampler pair's worst case. The decimating stage's half
span is 34 / (20000 / 48000) = 81.6 input samples, so `interpolate()` runs two loops of 82 and
touches 164 taps per output. Both resamplers are prepared outside the timed body, since
`prepare()` is where every buffer is allocated.

The case prices the resampler pair in isolation at that clock. It does not decompose the DD-3's
own number. The node pushes 256 single frames per block where this case pushes one block of
256, and the node's clock follows D.TIME instead of sitting at 20 kHz, so the kernel span
differs as well. That is why the 0.10 ms the case prints against is advisory.
