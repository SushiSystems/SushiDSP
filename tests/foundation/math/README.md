# tests/foundation/math

## `test_biquad.cpp`

### `BiquadOnePoleLowPassMatchesTheAnalyticSingleRcResponse`

`// A one-pole loses 6 dB per octave, not 12.`

Golden values from the analytic single-RC magnitude 1/sqrt(1+(f/fc)^2), which the prewarped
bilinear form reproduces exactly at the corner and closely below fs/8. The point of the case is
the slope: a one-pole must lose 6 dB per octave, not the 12 dB a Butterworth biquad loses, or it
would darken a Miller-limited stage twice as fast as the physics does.

`// Bilinear warping pulls the response down above fs/8.`

Compared only up to fs/8; above that the bilinear map's frequency warping pulls the discrete
response below the analytic one by design (0.55 dB at 55 kHz here), which is the usual and
desirable direction for an anti-aliasing role.

### `BiquadFirstOrderShelfPlacesItsCornersAndAsymptotesExactly`

`// The goldens are the post-warp readings at 96 kHz.`

DD-3 pre-emphasis (DR1+DR2 plan N4): zero 594.07 Hz, pole 3386.28 Hz, LF gain 33/47. These are
the same corners modules/dd3::Dd3PreEmphasis configures, so the goldens below are shared with
Dd3PreEmphasisHitsItsThreeDerivedGains (test_dd3_analog_filters.cpp).

The probe sits at fs/16 and fs = 96 kHz, not 48 kHz: at 48 kHz, fs/4 = 12 kHz lands the sine's
own samples on the same four phase points every cycle (48000/12000 = 4 exactly), so the
peak-of-sine estimator below can never see the true peak, whatever the tolerance; and fs/16 = 3
kHz at 48 kHz sits below the pole, short of the HF asymptote it is meant to probe. 96 kHz clears
both problems, matching the sibling DD-3 test's fix (commit dc8422f).

```text
LF gain  = 0.7021277                     (-3.0745 dB)
HF gain  = 0.7021277 * (3386.28/594.07)  = 0.7021277 * 5.7001364 = 4.0022236
at 1 kHz = 0.7021277 * sqrt((1+(1000/594.07)^2)/(1+(1000/3386.28)^2))
         = 0.7021277 * 1.8778100 = 1.31855
```

The discretised HF asymptote is tan-warped and is NOT 4.0022236 at Nyquist; the goldens below
are the digital (post-warp) readings at 96 kHz, not the analogue ones above.

### `BiquadFirstOrderShelfPairCancelsExactly`

`// The shelf pair cancels to a null below -120 dB.`

N4: pre-emphasis (zero 594.07, pole 3386.28, gain 33/47) followed by de-emphasis (the same two
corners swapped, gain 47/33) is the identity, because the numerator polynomial of one is the
denominator polynomial of the other and the scalars multiply to 1. Assert a null below -120 dB
on a broadband signal, which no tolerance-based response check would catch.

### `BiquadFirstOrderHighPassIsMinusThreeDecibelsAtItsCorner`

`// A single RC reads -0.04 dB a decade above its corner.`

A single RC section: -3.01 dB at the corner and -0.04 dB a decade above it, which is what
separates it from the second-order Butterworth (-3.01 dB at fc, but -0.004 dB a decade up).

### `BiquadFirstOrderHighPassStaysCleanAtLowCornersAtHighRates`

`// The one-pole's real pole stays inside the unit circle.`

The corner range that makes set_high_pass diverge at the 384 kHz oversampled rate: there the
conjugate pole pair rounds onto the unit circle, while this one-pole's real pole does not. The
passband reading is checked against the analogue RC value, an independent reference.

### `BiquadSetCoefficientsLoadsThemVerbatim`

`// The impulse response is 1, 1/2, 1/4, 1/8.`

b = {1, 0, 0}, a = {-0.5, 0} is the one-pole y[n] = x[n] + 0.5 y[n-1], whose impulse response is
the geometric sequence 1, 1/2, 1/4, 1/8.

## `test_newton_raphson_solver.cpp`

### `NewtonRaphsonSolverJointOverloadMatchesSplitOverloadBitForBit`

`// Both overloads must walk identical iterates.`

The circuit stages moved to the joint form to share one exp() between residual and derivative;
that must be a pure cost change, so both forms must walk identical iterates.

## `test_oversampler.cpp`

### `HalfbandDesignsHaveExactZeroTapsAndUnityDcGain`

`// The halfband zeros sit at even centre distances.`

tap_count = 4M-1 puts the centre at index 2M-1. The halfband zeros sit at even distances from
the centre; the surviving taps are at odd distances 1, 3, 5, ...

### `Oversampler8xImagesOfInBandTonesAreBelowMinus90dB`

`// A tone's images sit at k*f_base +- f.`

Spec D6a: image rejection >= 90 dB at 0.3\*f_base and beyond. A tone's images sit at k\*f_base
+- f; every line in the 8x spectrum other than the tone must be >= 90 dB down.

### `Oversampler8xAliasesOfOutOfBandTonesAreBelowMinus90dB`

`// Folds into (0.45..0.5)*f_base are skipped.`

Tones at the 8x rate from 0.55\*f_base to 3.95\*f_base in 0.05 steps, decimated to base rate:
whatever folds into [0, 0.45\*f_base] must be >= 90 dB below the tone. Folds that land in the
stage-1 transition band (0.45..0.5)\*f_base are the filter's don't-care region and are skipped.

`// The reference tone is bin-exact, as the aliases are.`

Reference level: the same-amplitude tone analysed with the same window at base rate. It has to
be bin-exact, as the folded aliases are; a tone off-bin loses up to 0.83 dB to Blackman-Harris
scalloping, which would flatter every alias by that much.

`// The sweep steps in integers; += 0.05 drifts.`

Integer-stepped: accumulating += 0.05 in double drifts, and near-integer fractions then fold to
\~1e-7 instead of \~0, where the sweep would assert nothing.

### `Oversampler4xRoundTripDroopAt20kIsWithinTenthOfADecibel`

`// 20 kHz is 0.4167*f_base, inside the 0.45 passband.`

20 kHz at 48 kHz is 0.4167\*f_base, inside the 0.45 passband; round trip through the two stages
shared with Oversampler8x's own stages 1 and 2.

### `Oversampler4xLatencyMatchesMeasuredImpulseDelay`

`// The round trip is 63.5 base-rate samples.`

The impulse peak is the round trip: 31.75 base-rate samples up and 31.75 back, 63.5 in all. The
27-tap stage is what makes it fractional - it runs at 4x, so its 13 taps of delay are 3.25
base-rate samples each way and 6.5 together. The 115-tap stage contributes 28.5 each way, a
whole 57. See the derivation in Oversampler4x::latency_samples. The reported floor is within one
sample of the peak.

### `ResetReturnsEveryStageToItsAsConstructedState`

`// A reset instance matches a never-used one bit for bit.`

An instance that has processed a burst and then been reset must produce bit-identical output to
a never-used instance, on all four classes. Oversampler4x is here because it is what every
shipping D6c node runs; TubeScreamer::reset() is only as good as this.

`// The 4x buffer is reused, so each block is copied.`

The 4x buffer is internally owned and reused, so each block is copied out before the next call
overwrites it. The two halfband blocks above need no such copy because process() writes to a
caller-owned buffer.

`// The 8x buffer is reused, so each block is copied.`

The 8x buffer is internally owned and reused, so each block is copied out before the next call
overwrites it.

## `test_resampler.cpp`

### `ResamplerHoldsADcLevelAtEveryRatio`

`// Every polyphase branch has unity DC gain.`

Every polyphase branch has unity DC gain and the decimating scale factor restores it, so a
constant comes back as the same constant whichever way the rate moves.

### `ResamplerRoundTripNullsAtTheDd3SlowClock`

`// The 7 kHz content sits inside the 8000 Hz passband.`

48 kHz -> 20 kHz -> 48 kHz. The content stops at 7 kHz, which is the pedal's own delay-path
limit and is inside the prototype's passband (0.400 \* 20000 = 8000 Hz, N1), so the round trip
is limited by the kernel's own error: -100 dB stopband, -108 dB interpolation floor. -80 dB is
asserted with margin for the float buffers.

### `ResamplerRoundTripNullsAtTheDd3FastClockAndTheSpx90Ratio`

`// 12 kHz content survives at the SPX90 ratio.`

48 kHz -> 82.5 kHz -> 48 kHz, and 48 kHz -> 31.25 kHz -> 48 kHz (the SPX90's 1.536). At the
SPX90 ratio the passband reaches 0.400 \* 31250 = 12500 Hz, which is exactly the unit's
published 20 Hz - 12 kHz band, so 12 kHz content must survive.

### `ResamplerSizesTheHistoryForAnUpsampleOnlyInstance`

`// The history is sized on min(1, minimum_ratio).`

An interpolating ratio never widens the kernel past its own 34 input samples, so a minimum_ratio
above 1 must size the history on min(1, minimum_ratio). Dividing by the minimum_ratio alone
reserves 64 slots here against a live reach of 76 and the ring wraps on top of itself, which
shows up as a round trip that does not null.

### `ResamplerRejectsAToneAboveTheOutputNyquist`

`// 15 kHz into 20 kHz would fold to 5000 Hz.`

15 kHz into a 20 kHz output rate would fold to |15000 - 20000| = 5000 Hz. Normalised to the
lower rate, 15 kHz is 0.75 cycles/sample, deep in the prototype's stopband, so the image must
land below -90 dB relative to the input tone.

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
and linear interpolation between its entries adds an error near -108 dB, so the image must sit
95 dB under the tone.

### `ResamplerOutputCountFollowsTheRatioWithoutDrift`

`// The output count stays within two samples of the ratio.`

Over a million frames the accumulated output count must stay within a sample or two of
input_frames \* ratio; a phase accumulator that rebases badly drifts here and nowhere else.

### `ResamplerResumesAfterAnUnderSizedOutputBuffer`

`// Deferred outputs come out on the next call.`

A short output buffer defers the outputs it had no room for. While the ring still holds their
history the next call emits them, so the stream is the one an always-fed instance produces,
sample for sample. Every position here is an integer, so equality is exact.

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
output grid, and one overwritten tap breaks it.

### `ResamplerLatencyIsThirtyFourSamplesOfTheLowerRate`

`// The delay is 34 / min(source, target) seconds.`

DR1+DR2 plan N1, as the Task 1.1 ruling widened it: the delay is
K_RESAMPLER_HALF_WIDTH_ZERO_CROSSINGS = 34 divided by min(source, target) seconds at every ratio.

### `ResamplerRateChangeMidStreamIsContinuous`

`// At 20 kHz a 100 Hz sine steps at most 0.0314.`

The DD-3 usage: set_rates() every 8 frames while the target sweeps 82.5 kHz -> 20 kHz. A phase
reset or a bank rebuild shows up here as a step; a correct accumulator does not. Reference: a
100 Hz unit sine sampled at 20 kHz moves at most 2\*pi\*100/20000 = 0.0314 per sample, so 0.05
leaves room for the sweep's own rate change and none for a click.

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

## `test_resampler_design.cpp`

### `ResamplerDesignKernelLengthHoldsTheWingThePeakAndOneGuard`

`// 34 * 512 + 1 endpoint + 1 guard = 17410.`

34 zero crossings \* 512 entries + 1 endpoint + 1 guard = 17410. HALF_WIDTH is 34 rather than
32 so the stopband clears its -100 dB target with margin. The table's own properties, a unity
DC sum on every polyphase branch and a linear-interpolation error bound of 3.8127e-6, are
measured by `tools/dsp/design_resampler.py` when it writes the table, and reach the tests through
`Resampler` in `test_resampler.cpp`.

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
