# Unit tests

The reasoning behind the cases in this folder: why a tolerance has its value, where a golden
number comes from, what a case was written to catch. The source keeps one line per point and
cites this file.

There is one section per test file and one subsection per test case or helper. Each entry
opens with the comment line the source carries, so a search for that line lands on its
reasoning. Figures are quoted as they were measured, with the date where the source gave one.

## `_USE_MATH_DEFINES`

The files here and under `tests/functional/regression/` that use `M_PI` define `_USE_MATH_DEFINES` ahead
of their includes. MSVC's `<cmath>` declares `M_PI` only when the macro is already defined, and
`<cmath>` may be pulled in transitively by any earlier include, so the definition has to stay
the first thing in the translation unit. GCC and Clang, through libstdc++ and libc++, declare
`M_PI` unconditionally.

## Log tests

`test_log_formatter.cpp`, `test_log_compile_time_cut.cpp`, `test_logger.cpp`,
`test_log_line_queue.cpp` and `test_async_log_sink.cpp` cover the logger of
`modules/foundation/log/include/SushiDSP/foundation/log/`. The compile-time cut has a file of its own because it sets
`SD_LOG_MIN_LEVEL` before including `log.hpp`. `test_track_recorder_logging.cpp`,
`test_room_node_logging.cpp` and `test_directory_listing_logging.cpp` each provoke one
handled failure and read the single line it logs through a capturing sink.

## `test_ambisonic_encoder.cpp`

### `AmbisonicEncoderIsOrthonormalOverTheSphere`

`// The quadrature is exact here; the residual is rounding.`

Under SN3D the sphere mean of Y_i Y_j is 1 / (2n + 1) on the diagonal and zero off it; 42
Gauss-Legendre elevations times 61 azimuths, 2562 points, integrate every product of two order-3
harmonics exactly, so the residual is float rounding alone.

## `test_asio_sample_converter.cpp`

### `AsioSampleConverterFloatToSampleClampsOutOfRangeInput`

`// Out-of-range input clamps to full scale.`

A value outside [-1, 1] (e.g. from a hot upstream signal) must not overflow the integer
container - it clamps to the format's full-scale value instead.

## `test_audio_import_cache.cpp`

### `AudioImportCacheReusesTheWavItAlreadyHolds`

`// An untouched file proves that no decode ran.`

The source would fail to decode, so a returned path with its contents untouched is proof that no
decode was run.

## `test_band_response_fit.cpp`

### `BandResponseFitReachesADipBetweenFlatBands`

`// Each section has a peak, since two shelves cannot dip.`

Two shelves cannot dip, so the model gives each section a peak as well as a step. The first two
targets are one band cut with its neighbours flat; the third is the reference garage's direct
path through the 4x12, whose 2 kHz octave the array null pulls down. Measured: 0.20, 0.45 and
0.10 dB.

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

## `test_cabinet_directivity.cpp`

### `CabinetDirectivityBeamsAtTwoKilohertzAndNotAtOneTwentyFive`

`// The 2 kHz band's energy mean reads -15.29 dB.`

At the 2 kHz centre itself: a(f) = 0.125 / sqrt(1 + 0.64) = 0.097614 m, k = 36.6300 /m,
k\*a\*sin60 = 3.0958 so the piston is 0.195099 (-14.19 dB), and the array is |cos(36.6300 \*
0.30 \* 0.866025 / 2)| = 0.046834 (-26.59 dB), together -40.78 dB. The array factor's null sits
26 Hz above the centre, so the band's energy mean over 1414 to 2828 Hz reads -15.29 dB by the
1024-point midpoint rule of the same closed form.

`// The 125 Hz band's energy mean reads -0.49 dB.`

125 Hz: the piston is 0.992358 and the array 0.956086 at the centre, -0.46 dB; the energy mean
over the octave reads -0.49 dB.

### `CabinetDirectivityAveragesTheArrayNullOverTheOctave`

`// The row null at 2120 Hz averages to -11.48 dB.`

The reference garage's direct path leaves the baffle atan(0.7 / 2.5) = 15.64 degrees up. There
the two rows of drivers, 0.30 m apart, cancel at 343 / (2 \* 0.30 \* sin 15.64) = 2120 Hz: at
the 2 kHz centre the array factor alone is -21.02 dB and the closed form -22.05 dB, while the
energy mean over the octave reads -11.48 dB, since the null is narrow and the array factor
recovers to 0 dB at both edges of the band.

### `CabinetDirectivityMatchesTheOctaveMeanOfItsClosedForm`

`// Sixteen points per octave match 1024 within 0.05 dB.`

The model samples sixteen points per octave; the test integrates the same closed form with 1024,
and the two agree within 0.05 dB in every band at every direction tried, 60 degrees off axis,
15.64 degrees up, and 52.85 degrees up.

### `CabinetDirectivityIndexIsSmallWhereTheConeIsOmnidirectional`

`// The index stays under 1.5 dB at 125 Hz.`

125 Hz gives k\*a = 0.286, so the piston and array together cost under 0.2 dB over the sphere;
the closed box's rear shelf reads -1.55 dB over the back hemisphere, which is 10\*log10(2 / (1 +
0.70)) = 0.71 dB of index on its own. Together the index stays under 1.5 dB, and by 2 kHz the
beam is over 10 dB.

### `CabinetDirectivityFallsSixDecibelsBehindTheBaffleAboveTheStep`

`// Straight behind, only the rear shelf remains.`

Straight behind, the piston and the array are both 1 and only the rear shelf remains. The 4x12's
step is 115 / 0.75 = 153.3 Hz. At 4 kHz the shelf is sqrt((1 + (4000/306.7)^2) / (1 +
(4000/153.3)^2)) = sqrt(171.1 / 681.6) = -6.00 dB; at 125 Hz it is sqrt(1.1662 / 1.6647) = -1.55
dB.

## `test_cabinet_node.cpp`

### `CabinetNodeInstallsAndAppliesAnImpulseResponse`

`// An identity IR makes the cabinet a one-partition delay.`

An identity IR makes the cabinet a pure delay of one partition; every sample that has seen the
delayed input by now must reproduce it exactly.

### `CabinetNodeSurvivesVariableBlockSizes`

`// An identity IR must survive any host block size.`

StreamingConvolution re-blocks the stream internally, so the cabinet must survive a host that
hands the node a different frame count every callback - including counts that are not powers of
two and not multiples of the partition. An identity IR makes the whole cabinet a pure delay of
exactly one partition, so the processed stream must reproduce the raw input stream shifted by
latency_samples(), sample for sample.

## `test_channel_resampler.cpp`

### `ResampledFrameCountRoundsTheRatio`

`// 9093888 frames at 44.1 kHz are 9898109 at 48 kHz.`

1000 \* 48000 / 44100 = 1088.43..., and the 44.1 kHz song the cache was measured on is 9093888
frames long, which is 9898109 frames at 48 kHz.

### `ResampleChannelsDoublesTheRateAndKeepsTheWave`

`// The ends read silence on one side and stay within 0.035.`

Away from the ends the converted tone sits on the analytic sine to within 4e-6; the first and
last samples read a filter that has silence on one side of it, and stay within 0.035.

## `test_crossover_pair.cpp`

### `CrossoverPairSumsToUnityPowerEverywhere`

`// The pair's powers sum to one at every frequency.`

The halves feed two uncorrelated fields, so their powers add at the seam, and a fourth-order
Butterworth pair at one corner holds 1 / (1 + x^8) + x^8 / (1 + x^8) = 1 at every frequency; the
bilinear transform keeps the identity exact in the digital domain. What remains is float
rounding and the response's tail past 8192 samples, both under 0.001 dB, so the bound is 0.01
dB.

### `CrossoverPairAmplitudeSumPeaksThreeDecibelsAtTheCrossover`

`// The amplitude sum reads +3.01 dB at the corner.`

The price of power complementarity: the two halves are not phase-aligned, so one signal sent
through both and summed reads 2 / (0.7654 \* 1.8478) = sqrt 2 at the corner, +3.01 dB. No caller
sums a correlated signal through the pair; this records the cost.

### `CrossoverPairFallsTwentyFourDecibelsPerOctave`

`// The closed form two octaves up is -48.16 dB.`

Two octaves above the crossover a fourth-order Butterworth sits 48 dB under the passband; the
closed form 10 log10(1 / (1 + 4^8)) is -48.16 dB.

## `test_damped_newton_solver.cpp`

### `DenseLuPivotsWhenTheLeadingEntryIsZero`

`// Without partial pivoting the first step divides by zero.`

[[0, 1], [1, 0]] x = [3, 5] has the exact answer x = [5, 3]; without partial pivoting the first
elimination step divides by zero.

### `DenseLuSolvesTheLeadingBlockOfAnOversizedArray`

`// Only the leading 3x3 block may be touched.`

The same hand-checked 3x3 system, stored in the top-left corner of a 5x5 array whose remaining
rows are the zeros a circuit leaves behind when it uses fewer ports than the compile-time
maximum. Only the leading 3x3 block may be touched.

### `DampedNewtonWithHalvingDisabledFailsOnTheDivergentStart`

`// With no halving, every iteration takes the full step.`

The same start with the halving budget set to zero: every iteration is forced to take the full
Newton step, which is exactly the divergence damping exists to prevent. This pins
set_maximum_halvings() to observable behaviour, and it also pins the stall guard: out at |v| \~
1e168 the Jacobian 1/(1 + v^2) underflows, the perturbed solve returns a step that is negligible
next to |v|, and a step-size-only convergence test would report that runaway point as a solution
while the residual is still atan -> pi/2.

### `DampedNewtonWarmStartConvergesInOneIterationOnASmoothTrack`

`// A warm start needs at most a couple of iterations.`

Sweeping the root slowly and warm-starting from the previous answer is the audio-rate usage
pattern; it must not need more than a couple of iterations per step.

### `DampedNewtonReturnsTheBestIterateWhenItFailsToConverge`

`// A failed solve returns its lowest-residual iterate.`

One iteration from the divergent start with halving disabled: the only step available is the
full Newton step, which lands at |atan| larger than where it started. The step is still taken -
that is how the iteration escapes a flat region - but solve() returning false must hand back the
lowest-residual iterate it saw, as its @param v contract says.

### `DenseLuTwoByTwoSolvesTheSameSystemsAsTheGeneralTemplate`

`// [[4, 3], [6, 3]] x = [10, 12] has the answer x = [1, 2].`

[[4, 3], [6, 3]] x = [10, 12] has the exact answer x = [1, 2]:

```text
4*1 + 3*2 = 10,  6*1 + 3*2 = 12.   det = 4*3 - 3*6 = -6.
```

### `DampedNewtonLeavesItsLastEvaluationAtTheIterateItReturns`

`// The default budget of 8 iterations is too tight here.`

Each damped step moves the iterate by roughly -1 while it is far from the root (step = -r/r' =
-(1 - exp(0.25 - v))), so the true worst case is v = 12.0: about 14-15 iterations of that
near-unit step before the quadratic tail takes over. The v = -5.0 start first overshoots past
the root, since r' = exp(-5) is tiny there, but the line search clips that step back into the
unit-step regime, landing near v = 7 before it needs 10+ more of them. The default budget of 8
is too tight for either start.

## `test_dd3_acceptance.cpp`

### `File scope`

`// Every case renders seconds, hence SUSHIDSP_REGRESSION_TEST.`

Every case here is written with SUSHIDSP_REGRESSION_TEST although the file sits under unit/: each
one renders seconds of audio, and docs/CONTRIBUTING.md keeps `sd test -s unit` a fast edit loop.
tests/functional/unit/test_measure.cpp's alias-floor cases do the same.

### `Dd3AcceptanceClockCurveMatchesThePublishedRange`

`// Row 1 checks fs, which the service note publishes.`

Acceptance row 1: within 2 % of the published 20-82.5 kHz curve. The check is on fs, which is
what the service note publishes (a scope on the sample-and-hold); the derived delay times are
printed beside it, because N = 1024/4096/16384 while the published times were computed with N =
1000/4000/16000, which is dossier open question 4.

`// The only slack is the float that holds log(fs).`

prepare() snaps the R56/C52 pole, so the only slack here is the float the clock keeps log(fs)
in, which is 7e-7 of the answer.

### `Dd3AcceptanceMeasuredEchoLandsWhereTheClockCurveSaysItWill`

`// An impulse's echo cannot agree with a wrong fs.`

Row 1 again, as a measurement rather than as arithmetic: wet_delay_seconds() is derived from the
same clock the prediction uses, so on its own it could agree with a wrong fs. An impulse's echo
cannot. TIME 0.5 is the midpoint the endpoints do not cover, and mode L there is 403 ms of ring,
which is where a factor-of-four error in N would show first.

`// The floor is absolute: 48 samples below 4800.`

Two 7 kHz cascades spread the echo's peak and land it about 13 samples late, so the floor is
absolute. It is 48 rather than the 32 test_dd3_delay.cpp uses at TIME 0 because the peak is
broader here: TIME 0.5 clocks the ring at 40620 Hz, below the host rate, so each conversion's
kernel covers 34/40620 s = 40 host samples where at TIME 0 it covers 34/48000 s = 34. The centre
of that spread is already in the prediction; what widens is how far the argmax can sit from it.
Above 4800 samples the published curve's own 2 % is the looser bound, and 1 % is what stays
inside it.

### `Dd3AcceptancePitchSlursRatherThanJumping`

`// HOLD freezes the ring so only the clock moves the pitch.`

Acceptance row 4: "frequency trace under a time-knob sweep shows the R56/C52 glide, not a jump".
Fill the ring with a steady tone, freeze it with HOLD so the clock is the only thing left that
can move the pitch, then turn D.TIME across its whole range and trace the wet signal's upward
zero crossings.

The pole itself is pinned exactly in test_dd3_clock.cpp: tau = 0.594 ms, 24.4657 % of the
remaining distance per 8-frame sub-block. A single full-scale TIME step is not something a
zero-crossing trace can resolve, because the glide ends before the trace takes its first
reading: at 48 kHz it is over in about 3 ms, while the probe has already arrived at the
destination clock, where a 1 kHz tone reads out at 242.4 Hz and its period is 198 samples, or
4.1 ms. The transcription in the task report puts the first crossing at sample 148 and every
crossing after it on that settled 198-sample grid. What the trace can resolve, and what the row
is about, is the knob being turned: it must walk the whole 4.125:1 range without a step, where
an ungliding clock would move it 4.125x between one period and the next.

`// 397 cycles per ring pass join the loop without a step.`

397 cycles per ring pass, so the frozen loop joins its own tail without a phase step. The read
pointer does lap the ring during the case - the 20 ms settle spends 1650 ticks at 82500 Hz and
the sweep 15474 more, against a 16384-sample ring - and that continuity is what makes the lap
harmless rather than one anomalous period in the trace.

`// One crossing sample is 4 %, so the bound is 0.90.`

Monotone downward in pitch. The trace quantises the period to whole samples and it starts at 24,
so one crossing sample is 4 % and the bound cannot be tighter than that. It is 0.90 rather than
0.96 because 4 % is the quantum of a single reading and two adjacent readings can round opposite
ways: the transcription's worst dip is 23/24 = 0.9600, and the extra 6 % is what the resamplers'
own smoothing of the trace is allowed to cost.

### `Dd3AcceptanceProducesNoNonFiniteSampleUnderTheStimulusBattery`

`// HOLD runs twice: from a cold ring and entered halfway.`

The D6b battery at feedback 0.0, 0.5 and 1.0 - the last of which is past self-oscillation - in
every mode. HOLD gets two runs: one from a cold ring, and one entered halfway so that what it
freezes is real content rather than the zeros reset() left.

`// The bound is a tripwire for divergence.`

The converter's own rails bound the loop at about 1.15 full scale, so this is a tripwire for
divergence rather than a level the model approaches.

### `Dd3AcceptanceReportsZeroLatencyAndDelaysOnlyItsWetPath`

`// The dry path is sample-aligned; only the wet one delays.`

Ruling 5. The node reports no latency, and what backs the claim is that the dry path is
genuinely sample-aligned: at E.LEVEL 0 the node is bit-close to a wire, which
test_dd3_delay.cpp's Dd3DelayAtZeroLevelIsAWire pins. The wet delay is reported apart from it,
because it is the effect rather than something a host should compensate.

### `Dd3AcceptanceAliasFloorIsMeasuredNotAssumed`

`// The sweep runs at the service note's stimulus level.`

Ruling 7 deferred oversampling of the analog chain and promised a number instead of a paragraph.
alias_floor() sweeps 1-20 kHz tones and reports the worst non-harmonic line. The assertion is
deliberately loose: the DD-3 is a 12-bit converter behind a 7 kHz wall and is not supposed to be
clean. The printed figure is the point - if it comes back worse than -60 dB, Ruling 7 is
revisited with data.

The sweep runs at the service note's stimulus, not at alias_floor()'s -12 dBFS default. -12 dBFS
is 20 dB above the level this pedal is specified at, and the compressor puts 7.26 V peak on a
converter whose rails are +-2.4988 V: 78 % of the converter's samples clip and the sweep then
measures that clipping rather than the conversion, reading -20.4 dB where an 8 kHz tone's fifth
harmonic folds about the 40620 Hz clock onto 619 Hz.

Nothing clips at the note's level, and what was left there was the NE570's rectifier aliasing
rather than a fold of the test tone: |sin| carries a harmonic at every 2k\*f0 and 6\*8000.24 Hz
folded to 1.46 Hz, inside the rectifier's own 23.4 Hz pole. The Ruling 7 revisit took that
reading and bought Ne570Compander::K_RECTIFIER_OVERSAMPLING with it, which moved this row from
-35.86 dB to -55.32 dB. The number still misses the -60 dB the printf names, by 4.7 dB; 16
sub-steps would reach -65.13 dB, and the cost curve that chose 8 is in
docs/archive/superpowers/specs/2026-09-05-dd3-spx90-design.md, Model corrections.

## `test_dd3_analog_filters.cpp`

### `Dd3SectionConstantsMatchTheComponentValues`

`// f0 = 1/(2*pi*R*sqrt(C1*C2)) and Q = 0.5*sqrt(C1/C2).`

f0 = 1/(2\*pi\*R\*sqrt(C1\*C2)), Q = 0.5\*sqrt(C1/C2), C1 the cap bridging to the emitter and C2
the cap shunting the base to ground (dossier 1.3, 1.4).

```text
AA 1: C9 4700p / C8 2700p      AA 2: C15 6800p / C14 470p
AA 3: C17 0.018u / C16 220p    RC 1: C38 2700p / C37 1200p
RC 2: C35 6800p / C36 150p
```

### `Dd3EmitterFollowerDampingLowersEveryQ`

`// Q = 1/(1/q + 2*(1-K)*q) for a buffer of gain K.`

Q = 1/(1/q + 2\*(1-K)\*q) for the equal-R Sallen-Key with a buffer of gain K, which is what
takes the cascade from +6.45 dB at 7 kHz to inside the published window.

### `Dd3PrewarpIsExactAtThePassbandEdgeAndScalesEveryCornerAlike`

`// One prewarp reference serves the whole cascade.`

One prewarp reference for the whole cascade, so tan(pi\*fd/fs) stays proportional to the
analogue f0 and the sections keep their stagger at every rate.

### `Dd3AntiAliasCascadeMatchesItsDampedPrototype`

`// The prewarp holds the passband within 0.4 %.`

The discretisation is checked against the analogue prototype at both rates the node runs at and
at 192 kHz. The prewarp holds the passband to 0.4 % of the prototype everywhere, which is the
whole reason it is anchored at 7 kHz rather than per section.

### `Dd3ReconstructionCascadeMatchesItsDampedPrototype`

`// The 4th-order filter is still rising at the band edge.`

1.003462 at 1 kHz, 1.041637 at 5 kHz, 1.010033 at 7 kHz: the 4th-order filter alone is still
rising at the band edge, which is why the 6th-order one carries the roll-off.

### `Dd3DelayPathMeetsThePublishedResponseWindow`

`// The window is +1/-3 dB of 1 kHz from 40 Hz to 7 kHz.`

The acceptance: 40 Hz - 7 kHz within +1/-3 dB of 1 kHz, from the DD-3's published delay-path
figure (docs/archive/research/2026-08-26-boss-dd3-design-dossier.md section 3). Both the
anti-alias cascade alone and the two cascades in series are held to it, because on the board the
wet signal passes through both.

### `Dd3AntiAliasCascadeRejectsAboveTheBandEdge`

`// 20 kHz is further down at 48 kHz than at 96 kHz.`

What the filter is for. The RBJ low-pass carries a zero at Nyquist the analogue prototype does
not, so 20 kHz is further down at 48 kHz than at 96 kHz; both are asserted so a change of
discretisation is caught rather than absorbed.

`// The thresholds sit just above the measured readings.`

0.1325 and 4.7e-6 at 48 kHz; 0.2073 and 8.3e-4 at 96 kHz. The four thresholds are set just above
those, so they catch a change in the cascade rather than state a requirement.

### `Dd3EmphasisPairIsTheIdentity`

`// The pair is unity by construction: 33/47 * 47/33 = 1.`

The pre-emphasis zero is the de-emphasis pole and vice versa, and 33/47 \* 47/33 = 1, so the
cascade is unity by construction. Assert a null below -120 dB on broadband noise, which no
tolerance-based response check would catch.

### `Dd3PreEmphasisHitsItsThreeDerivedGains`

`// The probe stays at fs/16; fs/4 hides both errors.`

0.7025132 below the zero, 1.3184111 at 1 kHz, 3.5023974 well above the pole. All three are the
analogue shelf; the discretised one sits above it, because the bilinear tangent stretches the
shelf as the probe climbs (3.5234141 at 6 kHz), and the tolerances carry that gap. The probe
stays at fs/16: at fs/4 the peak-of-sine estimator reads 99.58 % of the digital response, which
happens to cancel the stretch and hide both errors.

### `Dd3DeEmphasisIsTheReciprocalOfThePreEmphasis`

`// The tolerance is the bilinear warp, not the estimator.`

The tolerance is the bilinear warp, not the estimator: at 5 kHz the discretised shelf sits
1.61e-3 below the analogue one and the peak-of-sine reading is within 4.6e-5 of the discretised
response. Dd3EmphasisPairIsTheIdentity is what pins the pair exactly.

## `test_dd3_clock.cpp`

### `Dd3ClockDelayTimesLandOnThePublishedRanges`

`// The delay-time errors are printed, not asserted.`

N5's mode table, and the reason it is not exact: N = 1024/4096/16384 against published times
computed with N = 1000/4000/16000. The delay-time errors are printed, not asserted; acceptance
row 1 is a check on fs, which the case above makes.

### `Dd3ClockSlewMatchesTheR56C52Pole`

`// After one tau the clock reads 33681.4 Hz.`

tau = 22000 \* 2.7e-8 = 5.94e-4 s = 0.594 ms = 28.512 samples at 48 kHz. After one tau the
control has covered 1 - e^-1 = 0.6321206 of the log distance, and log2(4.125) = 2.0443847
octaves, so the pitch has moved 1.2922934 octaves:

```text
fs = 82500 * 2^-1.2922934 = 82500 * 0.408259 = 33681.4 Hz
```

### `Dd3ClockAdvancesOneQuarterOfTheRemainingDistancePerSubBlock`

`// The pole closes 0.2446570 of the rest per sub-block.`

Ruling 11: 8 frames at 48 kHz is 8/28.512 = 0.28058305 time constants, so the pole closes 1 -
exp(-0.28058305) = 0.2446570 of what is left, every sub-block. That figure is what makes the
pitch trace a glide rather than a jump, so it is pinned here.

## `test_dd3_converter.cpp`

### `Dd3ConverterQuantisationNoiseMatchesTwelveBits`

`// An ideal 12-bit converter gives 74.008 dB of SNR.`

An ideal N-bit converter gives 6.0206\*N + 1.7609 dB of SNR for a full-scale sine: 6.0206\*12 +
1.7609 = 74.008 dB. Measure it directly as the ratio of signal power to error power, which needs
no window and no FFT.

### `Dd3ConverterHoldKeepsTheLoopAndInhibitsWrites`

`// Long and Hold share N, so the ring is not cleared.`

Dossier 1.8: HOLD is a mode bit into the LSI, sharing L's clock range with writes inhibited.
Ruling 14: Long and Hold share N, so the transition between them must not clear the ring -
holding silence is the one thing HOLD must not do.

`// The stored impulse returns each lap; writes are dropped.`

The stored impulse comes back on schedule and keeps coming back, lap after lap, while everything
pushed in is discarded.

## `test_dd3_delay.cpp`

### `Dd3DelayWetDelayMatchesThePlansArithmetic`

`// The two conversions cost 68 / min(fs, 48000), not 64.`

N2, with the resampler round trip read off the committed prototype rather than off the plan's
draft: Task 1.1 grew the half width from 32 to 34 zero crossings, so the two conversions cost 68
/ min(fs, 48000) between them, not 64.

```text
Mode L at TIME = 0: 16384/82500 + 68/48000 + 8/48000
  = 0.198593939 + 0.001416667 + 0.000166667 = 0.200177273 s = 9608.51 samples.
Mode S at TIME = 0: 1024/82500 + 68/48000 + 8/48000 = 0.013995455 s = 671.78 samples.
```

`// 32 samples catch a wrong N, fs or round trip.`

The echo has been through two 7 kHz cascades, so its peak spreads over a few samples and lands
about 13 late; 32 samples is tight enough to catch a wrong N, a wrong fs or a missed round trip.

### `Dd3DelayRepeatsDecayAndDarkenWithFeedback`

`// Each repeat loses level and treble together.`

Dossier 3, "Self-oscillation": every repeat is re-band-limited by both 7 kHz cascades and
re-quantised, so repeats lose level and lose treble together. The take-off sits at the wet leg's
output, so the emphasis pair cancels inside the loop and the tilt is the filters'.

### `Dd3DelaySelfOscillatesOnlyAtTheTopOfTheKnob`

`// The tail sustains only at the top of the knob.`

K_FEEDBACK_PATH_GAIN exists so that K_MAXIMUM_FEEDBACK is the loop gain it claims to be: the loop's
forward gain peaks at 6.3184 at 3278 Hz, and on the mix resistors alone the node would
regenerate from F.BACK 0.22 upward. Below the top of the knob the tail dies; at the top it
sustains.

### `Dd3DelayHitsCheckPointFive`

`// 0.025 full scale in gives 0.035 full scale of wet out.`

Service note ladder, 1 kHz at 100 mVp-p in -> [5] 140 mVp-p of delay signal (dossier 1.0). In
this tree's units: 0.025 full scale in, 0.035 full scale of wet out.

## `test_denormal_guard.cpp`

### `decay_toward_zero`

`// A volatile read stops the compiler folding the product.`

Reading through a volatile keeps the compiler from folding the product at compile time, where it
would be evaluated under IEEE rules regardless of the runtime FPU mode.

## `test_diezel_vh4.cpp`

### `unity_master_db`

`// unity_master_db is derived from the calibration constant.`

The master level whose linear multiplier is exactly 1.0, derived from the calibration constant
rather than written out, so a recalibration does not silently move what the tests below measure.

### `DiezelVh4DescriptorExposesTenParametersInContractOrder`

`// The index order is the contract of hosts and presets.`

The index order is the contract a host and a saved preset address these by, so it is pinned here
rather than merely checked for membership. The first five are the channel's own front panel in
panel order; Deep, Presence and Master are the master section.

### `DiezelVh4IsDcStableAcrossManyBlocksAtHighGain`

`// Each knob visits both extremes in turn: ten settings.`

Every one of the five normalized knobs is taken to both of its extremes in turn, which is ten
settings rather than the 32 a full cross product would be: the failure this guards against - a
stage's warm start integrating a DC term until the output walks off - is a property of one
extreme at a time, and 32 x 200 blocks of an 8x oversampled six-tube chain buys nothing for the
runtime it costs.

### `DiezelVh4ThdDoesNotFallAsGainRises`

`// The THD reading must not fall as the gain knob rises.`

The gain pot is this channel's only drive control, so it has to sweep the cascade from nearly
clean into saturation. Measured with measure::thd() at a bin-exact 220 Hz on the settled tail of
a 131072-sample render.

The floor never reaches zero, and that is the circuit rather than the model: V1a sits ahead of
the pot and its grid conducts at 0.203 V, so a 0.2 input clips it wherever the knob is. What the
knob has to do is put V2a, V3a and V4a past their own knees on top of that.

2026-09-07, with the level staging replaced by the plate-swing ceiling and the phase inverter's
tail degeneration and power-grid scale in place: 37.867 %, 39.963 % and 42.822 %. The floor is
an order of magnitude higher than the 4.43 % this test read on 2026-09-06 because the dividers
now run at face value in volts, which leaves every stage past V1a saturated at any knob position
- the knob moves the reading by six points rather than by twenty.

What is asserted is therefore that the reading does not fall as the knob rises. Spec section 4
allows 1.0 percentage points of fall; this test holds the tighter 0.5 the readings support. Once
every stage clips, THD saturates as a metric, and a strictly rising spectrum is not a property
the circuit has to have. DiezelVh4HasACleanEndAtMinimumGain is what proves the knob's other end
still works.

### `DiezelVh4NeverExceedsFullScaleWithoutALimiter`

`// The output stays in [-1, 1] and reaches the rail.`

A plain clamp is what bounds the output. Driving gain, master and output to their ceilings must
still leave every sample inside [-1, 1], and must actually reach the rail - otherwise the test
proves only that the amp has headroom.

### `DiezelVh4DefaultSettingsPeakNearMinusSixDbfsForAMinusTwelveDbfsInput`

`// A -12 dBFS 82 Hz sine comes out at about -6 dBFS.`

The shipped calibration point, and the test that pins DiezelVh4::K_MASTER_UNITY_GAIN: a -12 dBFS 82
Hz sine into an untouched amp comes back out at about -6 dBFS, the frequency all four amplifiers
pin at (DA5 spec section 6).

`// The first blocks hold the startup transient.`

The first blocks are skipped so the measurement is of the settled amp rather than of the
filters' and the sag stage's startup transient.

### `DiezelVh4HasACleanEndAtMinimumGain`

`// The knob is read at 0.01 because 0.0 is a short.`

The acceptance the spec settled on after the level staging came out. Every stage past V1a
saturates at the default setting - dossier section 6 says the circuit is that hot, and the grid
probes in modules/nodes/diezel_vh4/README.md record where the grids sit - so what has to be
proved is the other end of the knob: turned down, into a small signal, the channel is still an
amplifier rather than a fuzz box.

The knob is read at 0.01 rather than 0.0 because the GAIN pot is a plain divider to ground and
0.0 is a short: the node is silent there and THD has no denominator. 0.01 is the first position
that passes signal. It read 0.0940 % on 2026-09-07 and 1.843 % on the volt-true power stage the
same day, the pair's class-AB handover now reaching the output.

### `DiezelVh4GridProbesReportEveryStageInSignalOrder`

`// The probes cover all five stages in signal order.`

The probes are what modules/nodes/diezel_vh4/README.md's grid table is measured with, so they
have to cover the chain rather than a prefix of it: five stages, every one of them moving after
a render, and nothing but zero outside the range.

### `DiezelVh4DryLatencyIsOneOversampledRegion`

`// The latency is one Oversampler8x round trip.`

V1a through the sag stage is a single 8x region, so the reported latency is one Oversampler8x
round trip. A host compensates by this number, so it is pinned against the oversampler itself
rather than against a literal.

### `DiezelVh4GainKnobMovesTheInputHighPassCorner`

`// The knob's travel is worth 3.0 dB at 300 Hz.`

C13's 1 nF into R14 and the gain pot is what voices channel 3: the knob moves the high-pass
corner ahead of the first distorting stage from 618 Hz at the bottom of its travel to 314 Hz at
the top (dossier section 6). What that buys at 300 Hz relative to 1 kHz is bounded arithmetic,
so the gate is stated rather than guessed: a first-order high-pass at 618 Hz reads 300 Hz at
-5.8 dB relative to 1 kHz, and at 314 Hz it reads -2.8 dB, so the whole travel is worth 3.0 dB
and the assertion below sits just inside it.

The knob is measured at 0.02 rather than at 0, because the pot is a divider and a wiper at the
bottom grounds the channel: 618 Hz is the corner's limit as the knob approaches zero, not a
setting that passes signal.

### `DiezelVh4DeepAtMaximumIsATenDecibelShelfAtEightyHertz`

`// A shelf carries half its decibels at its corner.`

Deep is a 10 dB low shelf at 80 Hz (dossier section 5: 80 Hz from the owner's manual, the depth
a modelling choice). A shelf carries half its decibels at its own corner, so 5 dB at 80 Hz is
what pins both the corner and the depth.

The master sits 20 dB under unity. frequency_response() sweeps at -40 dBFS, and since the
volt-true power stage this channel turns that sweep into a full-scale output: at unity the flat
response at 80 Hz reads +39.8 dB over the sweep, the output sits on the +/-1 rail, and a shelf
that lifts a clamped signal reads 2.18 dB where it lifts 5 (2026-09-07). The shelf is linear and
sits after the amp, so once nothing clamps the reading stops depending on the level: 4.813 dB at
80 Hz with the master 10, 20 and 40 dB under unity alike. Before the volt-true stage the -40
dBFS sweep never reached the rail and unity read 4.622 dB.

Only 80 Hz is asserted. The swept-sine estimate stops isolating the shelf at 20 Hz: C13's input
high-pass - 314 Hz with the gain knob at its top, 618 Hz at its bottom, and above 314 at the
default position this test uses - has thrown the fundamental away before the first distorting
stage, so what reaches Deep down there is mostly the saturated chain's own product rather than a
20 Hz tone, and the lift reads 6.840 dB where the shelf is 10 dB.

### `DiezelVh4MixBusAttenuationIsApplied`

`// The chain is linear end to end at -120 dBFS.`

The mix bus costs this channel 23.6 dB before V4a ever sees it: R43 680 kOhm into the 48.1 kOhm
the three grounded legs make, x0.0660 (dossier section 6), and a single-channel module has to
hard-code that shunt or the mixer triode is driven 24 dB too hard (section 8, question 13).

The level is -120 dBFS because that is where this chain is still linear end to end: the same
measurement reads 70.853 dB at -80, 72.965 at -100 and 73.114 at -120. At -80 dBFS the EL34
grids already carry 4.9 V, past the 4.1 V at which the cut-off tube's 35 V plate headroom
saturates, so the pair is compressing there by 2.3 dB. Dossier section 6 warns against testing a
cascade gain figure at all, because at 108.7 dB "nothing survives that linearly" - which is true
of the amp as it is played, and is why the level here is six orders below it.

Every factor at 1 kHz with the knobs at noon, each one a constant diezel_vh4.cpp names. A
TriodeStage's grid sits on Rg/(Rs + Rg) of its input, so each stage's own divider is on its row
with its gain:

```text
V1a, grid divider 1M/(1M+10k), gain 56.8                x56.2376  -> 0.0056238 V
C13 0.923278, the gain pot at 0.5 giving 0.326772       x0.301701 -> 0.0016967 V
V2a's cathode shelf 0.879497, its Miller pole 0.999304  x0.878885 -> 0.0014912 V
V2a, grid divider 3M3/(3M3+47k), gain 67.2              x66.2564  -> 0.0988016 V
C18 0.997970, K_V2A_TO_V3A_DIVIDER 0.4996003                x0.498586 -> 0.0492611 V
V3a's cathode shelf 0.969856, its Miller pole 0.929217  x0.901207 -> 0.0443944 V
V3a, grid divider 500k/(500k+470k), gain 71.4           x36.8041  -> 1.6339 V
the V3b follower 0.986, C20 1.000054                    x0.986053 -> 1.61111 V
the FMV deck at noon, R40 folded into it                x0.388541 -> 0.625983 V
volume 0.5 x 0.5, K_MIX_BUS_ATTENUATION 0.0660             x0.0165   -> 0.0103287 V
V4a, grid divider 48.1k/(48.1k+44.923k), gain 28.7      x14.8401  -> 0.153279 V
the V4b follower 0.986, / K_PHASE_INVERTER_DEGENERATION    x0.522032 -> 0.0800166 V
the inverter, grid divider 1M/(1M+10k), gain 61.927402  x61.3143  -> 4.90620 V
                                               product to the EL34 grids  x49062
```

The EL34 pair differenced, K_POWER_TUBE_TO_TRANSFORMER_ATTENUATION, the transformer and the sag stage
are the one block left unwritten, and they are worth x0.09225 here: 49062 x 0.09225 is the 4526
the assertion pins, 73.114 dB on 2026-09-07, with the input at 1e-6 rather than the 1e-4 the
rows above are written for. The grid probes corroborate the table at that level - it predicts
0.000229 V on V3a's grid against a measured 0.000244, and 0.000792 V on the inverter's against
0.000853.

K_MASTER_UNITY_GAIN is absent on purpose: the knob is set to unity_master_db, which is its
reciprocal, so the two cancel and the master contributes x1.

## `test_dk_circuit.cpp`

### `magnitude_at`

`/// @brief Returns the magnitude of the DTFT of a real impulse response at one frequency.`

Magnitude of the DTFT of a real impulse response at one frequency.

|H(e^{jw})| = |sum_n h[n] e^{-jwn}|. Evaluated directly rather than by FFT because the resonance
test needs magnitudes on an arbitrarily fine frequency grid, at full double precision, and the
impulse response is short.

### `DkCircuitInstantiatesEveryShapeD6cAndD6dNeed`

`/// @brief Tests that every DkCircuit shape D6c and D6d need compiles, prepares and steps.`

Compile-and-smoke gate for the three DkCircuit shapes D6c and D6d instantiate. Tasks 6 and 7 own
the numerical validation; this case only proves every shape compiles, prepares and steps.

`// This is the widest shape D6d instantiates.`

The widest D6d-style shape: four states, a two-branch nonlinear port, two sources (signal plus
supply rail), one probe, and a potentiometer to drive re-derivation.

### `DkCircuitRcLowpassMatchesTheBilinearTransformExactly`

`// Trapezoidal DK reproduces the bilinear transform.`

Node 1 = source, node 2 = output. R from 1 to 2, C from 2 to ground, probe node 2. Trapezoidal
DK must reproduce the bilinear transform of 1/(1 + sRC) exactly:

```text
Gc = 2C/T,  H(z) = (1 + z^-1) / ((1 + R*Gc) + (1 - R*Gc) z^-1)
```

With R = 1 kOhm, C = 100 nF, fs = 48000: Gc = 2\*1e-7\*48000 = 9.6e-3 S and R\*Gc = 9.6, so

```text
pole      = -a1/a0 = (R*Gc - 1)/(R*Gc + 1) = 8.6/10.6 = +0.811320754717
DC gain   = (1 + 1)/((1 + 9.6) + (1 - 9.6)) = 2/2 = 1 exactly
Nyquist   = (1 - 1)/(...) = 0 exactly
```

Note the sign: a lowpass pole must approach +1 as fs -> infinity, so the pole is (R\*Gc -
1)/(R\*Gc + 1) and NOT (1 - R\*Gc)/(1 + R\*Gc); the latter is a Nyquist pole. The -3 dB point of
the analogue prototype is 1/(2\*pi\*R\*C) = 1591.549431 Hz.

### `DkCircuitSeriesRlcResonatesWhereTheAnalogueCircuitDoes`

`// The magnitude peak sits at w0*sqrt(1 - 2*zeta^2).`

Source -> R -> L -> C -> ground, probing across C: H(s) = 1 / (1 + sRC + s^2 LC). With R = 100
Ohm, L = 10 mH, C = 100 nF:

```text
w0    = 1/sqrt(LC)            = sqrt(1e9)                 = 31622.776602 rad/s
                                                          -> f0   = 5032.921210 Hz
zeta  = (R/2)*sqrt(C/L)       = 50*sqrt(1e-5)             = 0.158113883, zeta^2 = 0.025
w_d   = w0*sqrt(1 - zeta^2)   = sqrt(1/(LC) - R^2/(4L^2)) = 31224.989990 rad/s
                                                          -> f_d  = 4969.611 Hz
w_pk  = w0*sqrt(1 - 2*zeta^2) = sqrt(1/(LC) - R^2/(2L^2)) = 30822.070014 rad/s
                                                          -> f_pk = 4905.485 Hz
```

The MAGNITUDE peak of a second-order lowpass sits at w0\*sqrt(1 - 2\*zeta^2), not at the damped
natural frequency w0\*sqrt(1 - zeta^2); the two differ by 1.3 % here, well outside the 0.5 %
tolerance, so the damped resonance is the wrong golden for a measured peak. Peak magnitude is
1/(2\*zeta\*sqrt(1 - zeta^2)) = 3.202563.

Run rate 384 kHz so trapezoidal frequency warping is negligible: the digital peak sits at
(2/T)\*atan(w_pk\*T/2) = 30805.6 rad/s, 0.053 % below the analogue peak - an order of magnitude
inside the tolerance, so the assertion is about the circuit, not the warping.

The response is measured as the DTFT of the impulse response rather than with
measure::frequency_response: the resonance is broad (Q = 3.16), so a 0.5 % frequency error costs
only 0.004 dB of magnitude. Locating the peak by argmax therefore needs a magnitude accuracy far
below the \~0.05 dB a swept-sine measurement delivers, and a sweep-based assertion here would be
measuring the sweep rather than the circuit.

`// 4096 samples are 53 time constants of the envelope.`

The envelope decays as exp(-zeta\*w0\*t), one time constant every 76.8 samples at 384 kHz, so
4096 samples is 53 time constants: the truncated tail is below 1e-23 of the peak.

### `DkCircuitDiodeClipperReachesTheTrueRootOfTheDiodeEquation`

`// The oracle is the bisected root of the diode equation.`

The circuit solves v = Vs - Rs\*Is\*(exp(v/Vt) - exp(-v/Vt)). Node 1 = source, node 2 = the
diode node, Rs from 1 to 2, and TWO antiparallel SiliconDiode branches from 2 to ground - a
pair, not a single symmetric element, so D6c can make it asymmetric later. Using SiliconDiode
with n = 1 and the same Vt reproduces that law exactly except for its -1 offsets, which cancel
between the two antiparallel branches.

D6b's DiodeClipperCircuit used to sit here too, as a second, independently written
implementation to agree with. D6c's plan (docs/archive/superpowers/plans/
2026-09-05-d6c-tubescreamer-on-dk.md, Task 10) retired it: it ran four fixed float Newton
iterations with no convergence test and drifted up to 2.8 mV from the root at high slew, so
agreeing with it was a weaker claim than agreeing with the equation. Bisecting the scalar
equation itself, in double precision, is an independently written \*root\* rather than an
independently written approximation, and it is the oracle this case now compares against
exclusively.

`// |residual| / slope bounds the distance to the root.`

The scalar equation, evaluated in double precision, and its slope. |residual| over the slope is
a first-order bound on the distance from a candidate voltage to the equation's true root, which
is the oracle: it compares the DK solution against the equation itself rather than against
another approximation of it.

### `DkCircuitPotentiometerChangesTheResponseAndCostsNothingWhenUnchanged`

`// The DC gain from source to wiper is the position.`

A 10 kOhm pot as a voltage divider into a 100 nF cap. The track runs from ground (the pot's a
terminal) to the source (its b terminal), so the a-to-wiper section is position\*10k and the DC
gain from the source to the wiper is exactly the position: a DC input of 1 V settles to p at the
output. Positions 0.25, 0.5, 0.9 give 0.25, 0.5, 0.9.

`// tau is at most 250 us, so 40000 samples settle it.`

The worst-case source impedance seen by the cap is the parallel combination of the two sections,
at most 2.5 kOhm, so tau \<= 250 us. 40000 samples is 0.83 s, over 3000 tau.

`// K_MINIMUM_POT_OHMS floors a shorted section at 1 Ohm.`

Both end stops must still derive and produce finite output. K_MINIMUM_POT_OHMS floors the shorted
section at 1 Ohm rather than 0, so the divider reads 1/10001 and 10000/10001 instead of exactly
0 and 1; that 0.01 % is the floor, and it is what keeps the augmented nodal matrix non-singular
at the stops.

### `DkCircuitNeverEmitsANonFiniteSampleAndCountsItsFallbacks`

`// One inf sample must not poison the capacitor's state.`

An RC lowpass with a linear nonlinear branch across the cap, so there is a state a poisoned
sample could corrupt. A nonlinearity that deliberately returns inf on one sample must not poison
that state: the trajectory has to return to that of an identical circuit that was never
poisoned.

`// The fallback costs one sample's worth of state update.`

The fallback holds the previous state rather than advancing it, so the poisoned sample costs
exactly one sample's worth of state update - a bounded step off the reference trajectory, not a
poisoned state. The bound below is the size of one sample's output change on this stimulus
(\~0.04 V at 300 Hz through a 100 us time constant); measured 0.022. The two then reconverge at
the circuit's own pole, 0.811 per sample here, so 190 samples later the deviation is 0.811^190 =
6e-18 of that step, i.e. gone. A state that had actually been poisoned would have stayed
non-finite, or diverged, instead.

### `DkCircuitClampsItsStateUnderAnAbsurdInputAndStaysFinite`

`// The output stays finite and the state stops at a limit.`

1e30 V into the RC lowpass. The output is allowed to be enormous - it is a linear circuit driven
by an enormous source - but it must stay finite, and the state must stop at the configured limit
rather than running away.

## `test_dk_op_amp.cpp`

### `DkCircuitIdealOpAmpInvertingAmplifierHasGainMinusRfOverRin`

`// V(3) = -(Rf/Rin) V(1) = -10 V(1) at every frequency.`

Node 1 = source, 2 = the summing junction, 3 = the output. The nullator senses ground against
node 2, so V(2) = 0; the norator drives node 3. KCL at node 2 is then -V(1)/Rin - V(3)/Rf = 0,
so V(3) = -(Rf/Rin) V(1) = -10 V(1) at every frequency.

### `DkCircuitIdealOpAmpNonInvertingAmplifierHasGainOnePlusRfOverRg`

`// 1 + 51000/4700 = 11.851063829787234 exactly.`

The Tube Screamer clip stage with its capacitors removed and the drive pot at zero: 1 +
51000/4700 = 11.851063829787234 exactly.

### `DkCircuitIdealOpAmpLowPassMatchesTheBilinearTransformExactly`

`// The pole is 95/97 and the DC gain is -10 exactly.`

Inverting stage with Rf in parallel with Cf: H(s) = -(Rf/Rin) / (1 + s Rf Cf). Trapezoidal DK
must reproduce its bilinear transform exactly. With Rin = 10 kOhm, Rf = 100 kOhm, Cf = 10 nF, fs
= 48000: a = 2 fs Rf Cf = 2\*48000\*1e-3 = 96, so

```text
H(z) = -(Rf/Rin) (1 + z^-1) / ((1 + a) + (1 - a) z^-1)
```

with pole (a - 1)/(a + 1) = 95/97 = 0.979381443298969 and DC gain -10 exactly. The analogue
corner is 1/(2 pi Rf Cf) = 159.154943 Hz.

### `DkCircuitRejectsAnOpAmpWithNoFeedbackPath`

`// Without a feedback path the system is rank deficient.`

An op-amp whose output does not reach its own input leaves the augmented system rank deficient.
prepare() must say so rather than derive nonsense.

## `test_dk_triode_stage.cpp`

### `File scope`

`// The netlist lives in tests/circuits/jcm_v1a_netlist.hpp.`

The netlist, its component constants and the triode glue live in
tests/circuits/jcm_v1a_netlist.hpp: the RT-safety traps and the benchmark build the same block,
and one description of it is the only way the three stay the same circuit. The same namespace
carries tests/circuits/spice_block_compare.hpp's spice_path, band_limit and interpolate_magnitude,
which the Tube Screamer block tests use unchanged.

### `DkTriodeStageSettlesAtTheHandComputedBiasPoint`

`/// @brief Tests that the jcm_v1a stage settles at the hand-computed bias point.`

Spec D6b.4 acceptance: the operating point of the jcm_v1a stage.

Hand solution (independent of DkCircuit, two-variable reduction plus bisection on Ip): Kirchhoff
gives Vgk = -Ip\*Rk and Vpk = 360 - Ip\*(Rp + Rk) with Rk = 2.7k, Rp = 100k, so the whole bias
reduces to the scalar fixed point Ip = koren(-Ip\*Rk, 360 - Ip\*102.7k). koren(...) - Ip is
strictly decreasing over 0 to 3.4 mA (it falls from 11.3 mA at Ip = 0 to zero before 1.7 mA), so
the root is unique; bisection to machine precision gives

```text
Ip = 8.555837e-4 A,  Vk = 2.3101 V,  Vpk = 272.132 V,  Vp = 274.442 V.
```

The brief's advisory band (0.8 V \< Vk \< 2.2 V) was derived for a 1.5k cathode resistor; with
the block's actual 2.7k and a 360 V supply the correct cathode voltage is 2.31 V, and the hand
solve above says so without reference to any DkCircuit output. The assertions below therefore
pin the hand-computed fixed point directly, which is strictly stronger than the advisory band:
any stamp sign error moves the operating point far further than these tolerances allow.

### `DkTriodeStageNewtonConvergesOnNinetyNinePointNineNinePercentOfSamples`

`// The stimulus set is a sweep and three bursts at 384 kHz.`

Spec D6b.4: \<= 0.01 % non-converged, zero NaN, over the full stimulus set. The set is a
2-second sweep 20 Hz -> 20 kHz at -12 dBFS scaled to volts by K_FULL_SCALE_VOLTS, plus a 1 kHz
burst at 0.05 V, 0.5 V and 5 V (well past grid conduction), all at 384 kHz.

### `DkTriodeStageNullsAgainstTheLtspiceTransientReference`

`// Only the last of K_PRE_ROLL_PASSES passes is compared.`

The exporter saves one 20 ms steady-state block per setting. The output high-pass formed by Cc
(0.022 uF) into the \~1 MOhm pot has tau \~ 22 ms, so driving that block once from the quiescent
bias leaves a turn-on DC transient around -43 dB that the plan's 5 ms discard nowhere near
removes. The block is 20 whole cycles of the 1 kHz stimulus, so it repeats seamlessly: it is run
K_PRE_ROLL_PASSES times to give over 160 ms (more than seven time constants, decaying the transient
past -100 dB) and only the final pass is compared, which is why no leading samples are discarded
from it.

### `DkTriodeStageMatchesTheLtspiceAcSweepWithinHalfADecibel`

`// -40 dBFS keeps the stage small-signal.`

frequency_response drives the node at -40 dBFS, which keeps the stage small-signal - the regime
an LTspice .ac analysis linearises around.

`// measure::render calls node.prepare() itself.`

measure::render calls node.prepare() itself, so the derivation and the bias settle happen there;
prepared() is checked afterwards rather than preparing twice.

## `test_early_reflection_renderer.cpp`

### `EarlyReflectionRendererCarriesOneTapsStateBetweenBanks`

`// Only tap 0's state is carried into bank 1.`

Two copies of a resonator ring in bank 0 after an impulse. Tap 0's state carried into bank 1
makes bank 1 ring exactly as bank 0's tap 0 alone does; tap 1, not carried, stays silent in bank
1 because the ring has gone quiet.

### `EarlyReflectionRendererCarriesATapOntoItselfWithoutAChange`

`// A carry onto the same bank is a copy onto itself.`

The same bank as both ends of a carry is a copy onto itself: a copy of the renderer that never
made the call renders the same block.

## `test_eyring_decay.cpp`

### `EyringDecayReproducesTheGaragesSixBandDecay`

`// V = 60 and S = 98 give these six decay times.`

V = 60, S = 98; alpha = (92 concrete + 6 doorway) / 98 per band, m from ISO 9613-1: 1.3415,
1.3326, 1.1608, 1.1460, 1.1081, 0.8931 s.

### `EyringDecayReproducesTheBedroomsSixBandDecay`

`// V = 36.4 and S = 67 give these six decay times.`

V = 36.4, S = 67; alpha = (14 carpet + 51.5 gypsum + 1.5 glass) / 67 per band: 0.3264, 0.8705,
1.1542, 0.7307, 0.4281, 0.3565 s.

### `EyringDecayReproducesTheBlockRoomsSixBandDecay`

`// V = 54 and S = 88.6 give these six decay times.`

V = 54, S = 88.6; alpha is the painted block row itself: 0.9292, 1.8860, 1.5476, 1.3051, 0.9826,
0.9809 s.

### `EyringDecayComputesTheSchroederFrequency`

`// Schroeder frequency = 2000 * sqrt(RT60(500 Hz) / V).`

2000 \* sqrt(RT60(500 Hz) / V): garage 2000 \* sqrt(1.1608 / 60) = 278.18 Hz, bedroom 2000 \*
sqrt(1.1542 / 36.4) = 356.14 Hz, block room 2000 \* sqrt(1.5476 / 54) = 338.58 Hz.

### `EyringDecayDivergesFromSabineOnlyInAnAbsorbentRoom`

`// Without air, Sabine / Eyring is -ln(1 - a) / a.`

Without the air term the ratio Sabine / Eyring is exactly -ln(1 - a) / a: 1.010 at a = 0.02,
1.042 at a = 0.0796 and 1.386 at a = 0.5. Both forms below keep the air term, so the 125 Hz band
is used: there 4 m V is 0.024 m2 against a surface term of at least 1.98 m2, and the three
ratios still land within 0.002.

`// The garage's mean absorption never exceeds 0.09.`

The garage's mean absorption never exceeds 0.09, so its Sabine and Eyring decays agree within 5
% in every band, which is why a concrete garage comes out the same either way.

### `EyringDecayHandlesAFullyAbsorbentRoom`

`// A mean alpha of 1 gives 0 s, not a NaN.`

Every surface OpenDoorway: mean alpha is 1, -ln(0) is infinite, and the decay is 0 s in every
band rather than a NaN, so the Schroeder frequency is 0 Hz as well.

## `test_fmv_tone_stack.cpp`

### `closed_form_gain_db`

`/// @brief Returns the gain of the hand-derived closed form of the tone stack.`

The hand-derived closed form FmvToneStack carried until 2026-09-20.

Kept here as the reference the netlist must reproduce: it was nulled against the jcm_fmv SPICE
sweep, so it is the one independent statement of the deck's unloaded response the repository
holds. Its coefficients are the Yeh/TSC solution of the rheostat-wired stack with an ideal
source and an open output.

### `FmvToneStackNetlistReproducesTheClosedFormItReplaced`

`// 192 kHz keeps bilinear warping inside the tolerance.`

192 kHz keeps the bilinear warping of the closed form and of the DK discretisation well inside
the tolerance at 6 kHz, where a 48 kHz run would separate them by more than the measurement's
own error.

### `FmvToneStackLoadingCostIsTheDecksOwnAndNotOneFigure`

`// The same termination costs each deck a different tilt.`

The same termination, V1A's 44 k plate impedance ahead of the deck and a 1 M volume pot across
it, costs the two decks different things, because the slope resistor sets how much of a
perturbation a 44 k source is: 33 k on the JCM against 100 k on the IIC+. So a model cannot
carry one loading figure for every amplifier, and terminating one deck says nothing about
another. An independent nodal solve of the same network predicts 3.89 dB of tilt for the JCM's
values and 0.87 dB for the IIC+'s.

## `test_hrtf_set.cpp`

### `HrtfSetInterpolationBetweenThreePointsIsABarycentricBlend`

`// By symmetry each of the three points carries one third.`

The centroid of +x, +y and +z is (1, 1, 1) / sqrt(3): azimuth pi / 4, elevation atan(1 /
sqrt(2)); by symmetry each of the three carries one third.

### `HrtfSetInterpolationWeightsSumToOne`

`// Each weight is a coordinate's magnitude over the sum.`

On the octahedron the triangle around a direction holds the axis of each coordinate's sign, and
the barycentric weights are the coordinates' magnitudes over their sum.

## `test_image_source_solver.cpp`

### `first_order_metres`

`// These are the seven first-order path lengths.`

dx = 2.5 on every path but the two x images; dz = 0.7 on every path but the floor (1.7) and the
ceiling (3.3); dy = 4.0 on the two y images; dx = 5.5 and 6.5 on the two x images.

### `ImageSourceSolverPlacesTheFirstOrderArrivalsExactly`

`// The two y images share one arrival.`

direct sqrt(6.74) = 2.596151 m, floor sqrt(9.14) = 3.023243 m, ceiling sqrt(17.14) = 4.140048 m,
-y and +y sqrt(22.74) = 4.768648 m, -x sqrt(30.74) = 5.544367 m, +x sqrt(42.74) = 6.537584 m;
times 48000 / 343 that is 363.3098, 423.0778, 579.3654, 667.3326, 667.3326, 775.8880 and
914.8805 frames. At order 4 t_c is 1749.27 frames and all seven survive; the two y images share
one arrival, so the lookup asks for a pair there and a single tap elsewhere.

### `ImageSourceSolverCutsTheFirstOrderSetAtItsOwnHandover`

`// At order 1 t_c is 699.708 frames, so five taps remain.`

At order 1 t_c is 2 \* 2.5 / 343 = 0.0145773 s, 699.708 frames, so the -x and +x images at
775.888 and 914.881 frames are past it and five taps remain.

### `ImageSourceSolverDropsEveryTapPastTheHandoverTime`

`// Order 4 enumerates 129 images; some arrive after t_c.`

Order 4 enumerates 129 images and the far corners of the lattice arrive after t_c, so the set is
smaller than the lattice.

### `ImageSourceSolverHandsTheScatteredEnergyToTheTail`

`// Cluttered keeps sqrt(0.60 / 0.90) = 0.816497 of Flat.`

One bounce keeps sqrt(1 - s) of the pressure: sqrt(0.90) / r for Flat and sqrt(0.60) / r for
Cluttered, so Cluttered is sqrt(0.60 / 0.90) = 0.816497 of Flat. The two y images share an
arrival, so the floor, ceiling, -x and +x taps are the ones looked up.

### `ImageSourceSolverSolvesTheGarageInsideThePublishBudget`

`// A Release solve measures 1 ms; the margin is deliberate.`

A solve is the band fits plus the lattice, and it is the largest part of the render set whose
own test bounds the whole build at the 33 ms a 30 Hz republish allows. The bound here is that
budget less the tail design and the modal solve, which measure well under a millisecond each. A
Release solve measures 1 ms; the margin is deliberate, because a tighter one measures how loaded
the machine is rather than how the solver scales.

## `test_ir_loader.cpp`

### `band_mean_decibels`

`/// @brief Returns the mean magnitude in dB over a band by a direct DFT of the samples.`

Mean magnitude in dB over [low_hz, high_hz], measured by a direct DFT of the samples zero-padded
to fft_size - independent of IrLoader's own FFT and bin arithmetic.

### `IrLoaderNormalisesShapedBandAverageToZeroDb`

`// A two-tap comb makes the band selection matter.`

A two-tap comb: |1 + 0.9 e^-jwK| ripples between 0.1 and 1.9 across the 100 Hz - 5 kHz band, so
unlike a single-tap IR the band selection and the averaging rule both matter.

## `test_jcm800.cpp`

### `unity_master_db`

`// unity_master_db is derived from K_MASTER_UNITY_GAIN.`

The master level whose linear multiplier is exactly 1.0. Master is a dB control whose 0 dB
reference is K_MASTER_UNITY_GAIN, so the tests that used to ask for "master fully up at 1.0 linear"
ask for this instead and keep measuring the same signal - including across the D5b recalibration
of that reference, which is why it is derived from the constant rather than written out as a
number.

### `Jcm800PreampIncreasesOutputEnergy`

`// The preamp knob never costs level; 1 % is sag drift.`

The claim used to be 6 dB between preamp 0.1 and 1.0. DA4 removed the interstage staging scalars
and gave the chain the losses the decks document, and the cascade saturates below preamp 0.1, so
the knob moves output level by 5.3 % rather than 6 dB: 0.201164 RMS at 0.1 against 0.211904 at
1.0, measured 2026-09-07. What survives is that the knob never costs level, so that is what is
pinned, with 1 % of slack for the sag stage's drift over a 30-block run.

### `Jcm800OutputTracksInputLevelRatherThanPinningToTheSolverClamp`

`// The knob is read at 0.02; at zero there is no output.`

A quiet note and a loud note must come out at different levels; the failure this guards against
is the amp degenerating into a fixed-amplitude square-wave generator. Measured 2026-09-07 at
preamp 0.02, near the bottom of the knob where the cascade is not yet fully saturated: a 0.01
input peaks at 0.007612 and a 0.251 input at 0.123296, a factor of 16.2 (5.95 before the cold
clipper's gain and the inverter's bias were corrected in DA5 part 2). The knob is read at 0.02
rather than at the minimum, where preamp_attenuation(0) is exactly zero and there is no output
to compare.

### `Jcm800DescriptorExposesEightParametersInContractOrder`

`// The index order is the contract of hosts and presets.`

The index order is the contract a host and a saved preset address these by, so it is pinned here
rather than merely checked for membership.

### `Jcm800ToneChangeDoesNotRebuildFiltersOffTheAudioThread`

`// The control thread stores a target and nothing else.`

The control-thread call stores a target and nothing else: the coefficients the audio thread last
built are still the old ones, which is exactly what "dirty" reports.

`// The 20 ms smoothing outlasts one block.`

One block is not enough: the knob is smoothed over 20 ms, so the audio thread is still walking
the coefficients toward the target. That it converges at all is the audio thread's doing - the
control thread never touched a filter.

### `Jcm800InputTrimAndOutputScaleTheSignalInDecibels`

`// The trim sits ahead of the nonlinear chain.`

The input trim sits ahead of the whole nonlinear chain, so it drives it harder rather than
scaling it: at this level the amp is still close to linear, so more in means more out.

### `Jcm800ThdDoesNotFallAsPreampRises`

`// THD must not fall as the preamp knob rises.`

Spec section 4's second acceptance criterion after the 2026-09-07 ruling: THD must not fall as
the gain knob rises, within 1.0 percentage point across its travel. The old claim was a strictly
monotonic rise from a clean bottom, which DA4 ended: with the interstage staging scalars gone,
V2a and the phase inverter clip at every setting and the metric saturates. Measured 2026-09-07
at a 0.2 input, master at 0 dB: 15.998 % at preamp 0.02, 42.812 % at 0.1, then 44.586, 44.803,
44.862, 44.893, 44.920, 44.942, 44.959, 44.974 and 44.986 % at 1.0. The sweep never falls; it
flattens above 0.2 because every stage is already clipping.

The sweep starts at 0.02 rather than 0.0 because preamp_attenuation(0) is exactly zero: the pot
is a short to ground and there is no signal to measure.

### `Jcm800PowerTubeBiasMismatchShapesTheEvenHarmonicContent`

`// The sign of the difference is pinned at 1e-5.`

Guards the power-tube bias mismatch (jcm800.cpp's K_POWER_TUBE_GRID_BIAS_MISMATCH_VOLTS) against being
dropped or neutralized. It is asserted at the amp's output rather than at the pair because the
pair itself is not reachable from here; what an input-referred offset does to the pair's
composite is pinned in test_pentode_stage.cpp.

The claim is self-checking: the same stimulus is rendered twice, once as shipped and once with
set_power_tube_grid_bias_mismatch_volts(0), so the gate is the comparison between the two rather
than a hand-copied number a later change could silently invalidate.

The direction has reversed twice. The corrected long-tailed-pair factor of 2026-09-07 made the
mismatch raise the second harmonic by 1.78e-5 (0.007257050 against 0.007239286); DA5 part 2,
with the offset in real volts on a volt-true pair and the inverter at its 2203 bias, has it
lowering the second harmonic again: 0.007392797 as shipped against 0.007536265 zeroed, a
difference of -1.435e-4 at preamp 0.5, 0.2 input, as the pre-DA4 chain did (2.17 % against 2.93
%). Which way the handover asymmetry adds to the preamp's asymmetry depends on the phase the
chain delivers, so the sign is a property of the shipped chain and is pinned as such.

The gate is signed and set at 1e-5, an order of magnitude under the reading.

`// Only the settled last quarter is measured.`

Only the settled last quarter is measured, for the same reason Jcm800ThdDoesNotFallAsPreampRises
measures only that: the slew, the transformer highpass and the sag recovery are all still moving
well past block 15.

### `Jcm800NeverExceedsFullScaleWithoutALimiter`

`// The output stays in [-1, 1] and reaches the rail.`

The soft-knee limiter is gone; a plain clamp is what bounds the output. Driving preamp, master
and output to their ceilings must therefore still leave every sample inside [-1, 1], and must
actually reach the rail - if nothing ever hits 1.0 the test would be proving only that the amp
has headroom, not that the clamp works.

### `Jcm800DefaultSettingsPeakNearMinusSixDbfsForAMinusTwelveDbfsInput`

`// A -12 dBFS low-E sine comes out at about -6 dBFS.`

The shipped calibration point: a -12 dBFS low-E sine into an untouched amp comes back out at
about -6 dBFS, leaving headroom for a hotter pickup without the post-cabinet clamp ever engaging
on a normal signal.

`// The first blocks hold the startup transient.`

The first blocks are skipped so the measurement is of the settled amp, not of the filters' and
the sag stage's startup transient.

### `Jcm800DryLatencyIsOneOversampledRegion`

`// The latency is one Oversampler8x round trip.`

D6a.3 collapses V1a..sag into a single 8x region, so the reported latency is one Oversampler8x
round trip - not two. A host compensates by this number, so it is pinned against the oversampler
itself rather than against a literal.

### `Jcm800HasACleanEndAtMinimumGain`

`// The knob is read at 0.01 because 0.0 passes no signal.`

Spec section 4's first acceptance criterion: with the gain knob at minimum and a -40 dBFS input,
THD must be below 10 %.

The knob is read at 0.01 rather than 0.0, because preamp_attenuation(0) is exactly zero: the pot
is a short to ground, nothing reaches V1b, and the amp has no output to measure. 0.01 is the
first position that passes signal.

### `Jcm800GridProbesReportEveryStageInSignalOrder`

`// The probe hooks are pinned, not their figures.`

The probe accessors themselves, which no other case exercises: spec section 4 asks for the
readings to be recorded rather than asserted, so this pins the hooks and not the figures.

## `test_jcm_tone_stack.cpp`

### `JcmToneStackAtEightTimesRateMatchesBaseRateWithinFifthOfADecibel`

`// Float coefficient quantisation stays within 0.2 dB.`

The stack moves inside the 8x region in D6a; at fs/f0 \~ 3840 the float biquads' coefficient
quantisation must not shift the response by more than 0.2 dB.

## `test_late_reverb.cpp`

### `LateReverbHadamardPreservesEnergy`

`// The normalised Hadamard matrix is orthogonal.`

The transform alone: 1000 applications to a fixed vector keep its sum of squares within 1e-5 of
the start, because the normalised Hadamard matrix is orthogonal.

`// The left ear reads 0 and 4 in turn without decaying.`

The network with 16 lines of length 1, unit filters and no absorption: an impulse puts 1 in
every line; H maps the all-ones state to 4 e0 and back, so the left ear, whose vector A sums to
zero and has A[0] = 1, reads 0 and 4 in turn without decaying over 1000 frames.

### `LateReverbRunsBothSectionsOfEveryOutputFilter`

`// Two sections of gain 0.5 scale the output by 0.25.`

Two sections of gain 0.5 in the tone correction scale both ears by 0.25 against unity; the same
pair in coherence_b scales the right ear alone, since the left ear is A alone.

### `LateReverbSurvivesALineLengthChangeWithoutAClick`

`// A 50 Hz sine steps at most 0.0065 of peak per sample.`

A 50 Hz sine at 48 kHz steps by at most 2 pi 50 / 48000 = 0.0065 of its peak per sample; a read
head that jumped between lines hundreds of frames apart would step by up to twice the peak, and
the 512-frame crossfade spreads that over 512 samples.

## `test_late_reverb_design.cpp`

### `LateReverbDesignMeetsTheModalOverlapRule`

`// The lines total at least 1.4 * RT60(500 Hz).`

Garage: Eyring gives RT60(500 Hz) = 1.160756 s, so the sixteen lines must total at least 1.4 \*
1.160756 = 1.625058 s, which is 78002.8 frames at 48 kHz. The plan's 78019 rounds the decay to
1.161 s first; both bounds hold.

`// The shortest line is the handover time.`

The rule is met by spreading the lines, not by holding them all back: the shortest line is the
handover time, so the tail's first output lands where the taps stop.

### `LateReverbDesignUsesSixteenDistinctPrimes`

`// The garage's primes run from 1753 to 10211.`

The garage's shortest line is its handover time, 1749 frames, and the spread that carries the
sixteen to 1.4 \* 1.160756 s = 78002.8 frames is 5.836; the targets 1749 \* 5.836^(k / 15),
rounded and stepped up to the next prime, run from 1753 to 10211.

`// The targets 4800 and 19200 step up to 4801 and 19207.`

line_frames alone: a 100 ms shortest line spread by four asks for 4800 and 19200 frames, and the
primes at or above them are 4801 and 19207.

`// A zero shortest line yields the first sixteen primes.`

A zero shortest line still yields sixteen distinct primes, the first sixteen, so the network
never sees a zero or a repeated length.

### `LateReverbDesignFitsTheDecayEachBandAsks`

`// The cascade lands within 1 dB of each wanted gain.`

Line 0 is 3449 frames, tau_0 = 0.0718542 s. Against the garage's six decays 1.341523, 1.332626,
1.160756, 1.146019, 1.108081 and 0.893129 s the wanted gains 10^(-3 tau_0 / RT60) are -3.214,
-3.235, -3.714, -3.762, -3.891 and -4.827 dB; the cascade lands within 1 dB.

### `LateReverbDesignDelaysTheTailToTheHandoverTime`

`// The delay is the handover time less the shortest line.`

t_c = 5 \* 2.5 / 343 = 0.0364431 s, 1749.27 frames. The network emits nothing until its shortest
line has run, so the delay ahead of it is the handover time less that line: the garage's
shortest line is 1753 frames, already past t_c, and the delay is zero.

`// Every reference room hands over within a frame of t_c.`

Every reference room hands over within a frame of t_c, the tail's first output landing at the
delay plus the shortest line.

### `LateReverbDesignPutsEachBandsRemainderIntoTheTail`

`// Each band's share is floored at -30 dB of the largest.`

Per band the tail carries what the diffuse field holds beyond the taps: the input gain squared
is the mean over bands of remainder over network energy, each band's share floored at -30 dB of
the largest, and the output cascade lands within the fit's error on sqrt of each band's share of
that mean. The bedroom's 4 kHz reflections alone exceed its field there, so that band's share
sits on the floor.

`// The tail's gain grows as the taps shrink.`

An empty set leaves the whole field to the tail, so its gain exceeds the full set's; halving
every tap raises the tail, since the remainder grows as the taps shrink.

### `LateReverbDesignRendersEachBandsRemainder`

`// Each band renders within 1 dB, or 2 dB at 125 Hz.`

An impulse through LateReverb at each room's design, 131072 frames, read as the power spectrum's
mean over each octave: within 1 dB of the band's remainder from 250 Hz up, where the fits are
anchored on both sides, and within 2 dB at 125 Hz, whose octave runs down to 88 Hz where the
line filters and the output cascade hold whatever their low shelves settle to. A band whose
remainder is zero renders at least 10 dB under the reflections it would sit beneath.

### `LateReverbDesignMakesTheTailCoherentLowAndIncoherentHigh`

`// Coherence is above 0.85 at 125 Hz, below 0.20 at 4 kHz.`

sin(k d) / (k d) at d = 0.17 m is 0.975 at 125 Hz, 0.902 at 250 Hz and 0.0088 in magnitude at 4
kHz; the plan's 0.900 is the 250 Hz value and its 0.0257 matches no band. The coherence filter's
magnitude is above 0.85 at 125 Hz and below 0.20 at 4 kHz.

### `LateReverbDesignKeepsTheCoherencePairPowerComplementary`

`// |A|^2 + |B|^2 = 1 within 1e-9.`

|A|^2 + |B|^2 = 1 at every band centre and at the corner between them within 1e-9, so the right
ear carries the left ear's energy whatever the split between the two sums.

### `LateReverbDesignShapesTheOutputWithBothSections`

`// The second section carries part of the fall to 4 kHz.`

The garage's output cascade falls from +4.7 dB at 125 Hz to -12.1 dB at 4 kHz, the room's longer
bass decay and the 4x12's beaming both handed to the tail; the first section alone does not
reach the 4 kHz figure, so the second carries part of the fall.

### `LateReverbDesignGivesTheMicrophoneOneOutput`

`// A microphone's coherence filters are pass-throughs.`

ListenerKind::Microphone leaves both coherence filters as pass-throughs: b0 = 1 in each section,
the rest 0. The lines are the room's, the same for both listeners.

### `LateReverbDesignHandsAMicrophoneItsShareOfTheField`

`// A microphone picks up a^2 + (1 - a)^2 / 3 of the field.`

A microphone picks up a^2 + (1 - a)^2 / 3 of a diffuse field's power for a pattern a + (1 - a)
cos(theta): all of it as an omni, a third as a cardioid, so at one spot the cardioid's tail is
designed against a third of the field beyond its own taps, and the ears' against the whole
field. The taps hold each capsule's pattern already.

`// The design's gain follows that arithmetic within 1e-6.`

The cardioid's remainder at 500 Hz is a third of the field less its own taps, which the pattern
has already reduced; the design's gain follows that arithmetic within 1e-6.

## `test_long_tailed_pair.cpp`

### `LongTailedPairDrivesItsTwoPlatesInOppositeDirections`

`// Matched amplitudes are not asserted.`

Whether the two amplitudes come out matched is not asserted: each grid sits behind its own
source and leak, and dossier question 6 leaves the pair's DC point open.

## `test_measure.cpp`

### `MeasureAliasFloorOfUnoversampledTanhIsPoor`

`// Folded harmonics count as aliases.`

A 20 kHz tone through tanh at 48 kHz folds its harmonics straight back into band; the folded
third harmonic (60 kHz -> 12 kHz) is tens of dB up. This pins that the sweep counts folded
harmonics as aliases rather than masking them.

### `MeasureFrequencyResponseTracksAOnePoleLowpass`

`// The expectations are the difference equation's own |H|.`

Expectations are the difference equation's own |H|, not the analogue RC it is derived from: with
a = exp(-2\*pi\*1000/48000), |H(w)| = (1 - a) / |1 - a\*exp(-jw)| is -3.004 dB at 1 kHz and
-19.415 dB at 10 kHz. The analogue RC's -20.04 dB at 10 kHz is 0.63 dB away, which is the
impulse-invariant transform's warping, not measurement error.

### `MeasureNullTestHandlesPolarityInversion`

`// The lag search matches on correlation magnitude.`

b is inverted as well as delayed and scaled, as it would be after an odd number of inverting
stages. The lag search has to match on correlation magnitude to find 37 here; matching on the
signed correlation picks whatever lag is least negative and the residual never nulls.

### `K_D5B_JCM800_ALIAS_FLOOR_DB`

`// K_D5B_JCM800_ALIAS_FLOOR_DB is the D5b baseline of -3.8 dB.`

D5b baseline, measured by Task 2 of the D6a plan at commit eec6499 with the 15-tap Hamming
halfbands: TubeScreamer drive=1 -8.3 dB, Jcm800 preamp=10 -3.8 dB. Spec D6a requires the pre-DK
chain to improve by >= 30 dB once the oversampler is replaced.

Measured after D6a Task 4 replaced the halfbands with the >= 90 dB Parks-McClellan designs:
TubeScreamer -9.6 dB (1.3 dB better), Jcm800 -32.0 dB (28.2 dB better). Both assertions
therefore still fail, and deliberately so - the bar is the spec's and is not to be lowered here.
What the redesign fixed is resampling aliasing; what remains is the nonlinearities' own
aliasing, which no anti-imaging filter can touch:

- Jcm800 came within 1.8 dB of the target, so its floor was filter-dominated. The rest is
  expected to fall to the post-cab clamp (deferred to D6d) and the two-region topology with the
  tone stack between them (Task 5).
- TubeScreamer barely moved, so its floor was never filter-dominated. A per-tone sweep at 48
  kHz puts the worst alias at 6.0 kHz for an 18 kHz tone: that is the 21st harmonic of 18 kHz
  (378 kHz) folding about the 8x Nyquist of 192 kHz, inside the oversampled domain where the
  halfbands cannot see it. A hard diode clipper's harmonics decay as 1/n, so the floor sits near
  -20 dB whatever the filters do, and it barely tracks the drive knob (-18.1 dB at drive 0, -6.1
  dB at drive 0.5, -9.6 dB at drive 1). Closing this needs an antialiased nonlinearity (ADAA) or
  a higher oversampling factor, neither in Task 4.

Measured again after D6a Task 5 collapsed the JCM800 into a single 8x region. Collapsing it
alone made the floor worse, not better - -20.4 dB - because region A's decimator had silently
been band-limiting V1b's output before the later clipping stages saw it. Restoring the
interstage bandwidth the model was actually missing (the 12AX7's Miller input capacitance, in
jcm800.cpp) took it to -43.4 dB, 39.6 dB better than D5b and clear of the spec bar. DA4 then
removed the JCM800's level staging and the floor went to -6.7 dB, so that case is skipped as a
known issue and its own comment carries the reason. The TubeScreamer's floor is aliasing
generated inside the oversampled domain, and D6c's rebuild moved it to -18.6 dB without reaching
the bar, so it stays a regression floor. The case's own comment carries that number.

### `TubeScreamerAliasFloorDoesNotRegress`

`// The floor is the measured -18.6 dB with 1 dB of slack.`

A regression floor, not the spec's 30 dB improvement target. -18.6 dB on the D6c DK chain, 9.0
dB better than D6a's -9.6 dB and 20 dB short of the -38.3 dB C1 asked for. C1 says to report the
number and loosen nothing, so this stays a floor at the measured value with 1 dB of slack.

D6a Task 4's diagnosis still holds and the rebuild did not change it, only its arithmetic. The
worst line is an odd harmonic of the test tone folding about the oversampled domain's own
sampling frequency, which lands it below 24 kHz there, so it is in band before any decimator
sees it. At D6a's 8x the fold point was 384 kHz and it took the 21st harmonic of 18 kHz; at C1's
4x the fold point is 192 kHz and the 13th harmonic of a 15 kHz tone reaches it (13 x 15000 =
195000 -> 195000 - 192000 = 3000 Hz), which is the -18.6 dB worst case. Halving the oversampling
factor halved the harmonic order the fold needs, and a diode clipper's harmonics decay as 1/n,
so the same mechanism reads about 6 dB louder.

The D6c plan expected the clip block's feedback pole (Rf against Cf + 2 Cd = 59 pF, so 1/(2 pi
551k 59p) = 4.89 kHz at Drive 1 and 1/(2 pi 51k 59p) = 52.9 kHz at Drive 0) to attenuate the
fold source by 23-37 dB. It does not, and the drive sweep is decisive: the pole moves 10.8x
across the knob's travel, nearly 21 dB of first-order rolloff at any fixed frequency above both
ends, while the floor moves 2.3 dB - -20.9 dB at Drive 0, -18.7 at 0.5, -18.6 at 1. Those are
the small-signal poles. While a diode conducts, the feedback impedance is its dynamic resistance
rather than Rf, some hundreds of ohms, and the pole moves three decades up, which is why moving
Rf barely registers: the clipped corners stay sharp and the high-order harmonics survive.
Closing the last 20 dB needs an antialiased nonlinearity (ADAA) across the diode pair, not a
filter.

The stimulus is alias_floor()'s -12 dBFS default, which is 0.5 V peak at the core's 2.0 V
calibration - a hot humbucker, and a level the TS808 is meant to clip at. Unlike the DD-3 case
there is no mismeasured rail clipping to back off from: 0.2512 full scale reads -18.6 dB, 0.05
reads -21.6 and 0.025 reads -25.3, so 6.7 dB covers the whole plausible range from a hard pick
to a weak one and none of it changes the mechanism.

### `Jcm800AliasFloorImprovedAtLeast30dBOverD5b`

`// The case is skipped as a known issue but still prints.`

Skipped as a known issue; the measurement below still runs and still prints its figure.

-43.4 dB before DA4. Two D6a changes had got it there, pulling in opposite directions: Task 4's
>= 90 dB halfbands took the resampling aliasing out (-3.8 -> -32.0 dB), Task 5's single 8x
region cost 11.6 dB of that back because region A's decimator had been acting as an unplanned
interstage band-limit, and the Miller grid poles took that bandwidth away again for a physical
reason rather than a filtering one.

### `Peavey5150AliasFloorAtItsDefaultIsUnderMinusThirtyDb`

`// The case is skipped as a known issue but still prints.`

Skipped as a known issue; the measurement below still runs and still prints its figure.

DA5 spec section 11's second acceptance for the 5150: pre gain 0.5 at -12 dBFS under -30 dB. It
read +26.0 dB before DA5 part 1, -17.2 dB after it, -11.4 dB after part 2 and -26.8 dB with
every stage antialiased (part 4); the worst line is the 20 kHz tone's 19th harmonic folding
about 384 kHz to 3995 Hz, made by stages fed an already-clipped waveform.

### `TriodeStageAliasFloorAtThreeAndTwelveTimesItsWindow`

`// V5A alone folds under -60 dB at 6 V and -40 dB at 24 V.`

DA5 spec section 11's first acceptance: the 5150's V5A stage alone in an 8x region, its 2.0 V
window driven 3x and 12x past, folds under -60 dB and -40 dB. On the Norman grid law it measured
-81.2 dB at 6 V and -53.7 dB at 24 V on 2026-09-07, and -102.4 and -79.8 dB once the stage's
composite transfer was antialiased (DA5 part 4); the room-temperature Shockley knee read -37.3
dB and -13.7 dB at the same drives.

### `PentodeStageAliasFloorInsideAndAtItsWindow`

`// The EL34 folds under -80 dB at 8 V and -50 dB at 38 V.`

DA5 spec section 11's third acceptance, in the real volts the stage now takes: the 2203's EL34
alone in an 8x region, driven to the 8 V that one model volt of the old stage stood for and to
its whole 38 V window, folds under -80 dB and -50 dB. Measured 2026-09-07: -88.1 dB at 8 V and
-55.1 dB at 38 V, and -101.2 and -84.4 dB once antialiased (DA5 part 4), where the
bias-referenced stage read -49.7 dB at one model volt.

### `MeasureFrequencyResponseIsAccurateAtTheBottomOfTheAudioBand`

`// The sweep starts at 5 Hz, two octaves below the band.`

The ESS carries no energy below its own start frequency, and its abrupt onset there leaves the
first bins of the deconvolved response contaminated by that truncation. When the sweep started
at 20 Hz this measurement read a 87 Hz first-order highpass at 20 Hz as -27.250 dB against an
exact -25.552 dB, an error of -1.699 dB, which is more than three times the half-decibel band
the LTspice AC comparisons are held to. Starting the sweep at 5 Hz instead - two octaves below
the lowest frequency any caller asks for - brings that same point to within 0.068 dB. This pins
the fix: it fails if the sweep's low edge is ever raised back into the reported band.

## `test_mesa_mark2c.cpp`

### `MesaMark2cDescriptorExposesEighteenParametersInContractOrder`

`// The index order is the contract of hosts and presets.`

The index order is the contract a host and a saved preset address these by, so it is pinned here
rather than merely checked for membership.

`// The three switches follow the fifteen knobs.`

The three panel switches were appended on 2026-09-21, after the fifteen knobs, so that a preset
saved before them still addresses the same parameter at every index.

### `MesaMark2cIndexedParameterSetMatchesTheDescriptorOrder`

`// A switch reads back the position it selects.`

The switches read back the position they select, not the value that selected it, so each is
given a value that is already one of its own positions.

### `MesaMark2cIsDcStableAcrossManyBlocksAtHighGain`

`// The settled DC stays under -40 dBFS and stops moving.`

Every knob is driven to both of its stops and held there for 200 blocks. The output must stay
finite, its settled DC term must stay under -40 dBFS, and that term must stop moving.

The bound was 0.25 while OutputTransformerStage ran its tanh after its own rolloff and the
rectification the tanh introduced at the extreme setting was never blocked: the high stop read
-0.0506. Since DA5 part 3 the stage's secondary carries no mean, as a real secondary winding
does, and the high stop reads -0.00009 (-80.7 dBFS, 2026-09-07). See
modules/nodes/mesa_mark2c/README.md.

At the low stop Master 1 sits at -60 dB, so a bound on the DC alone would pass on attenuation.
The drift check is what makes that extreme mean something: the DC of the third quarter and of
the last must agree, which a slowly diverging state would not.

### `MesaMark2cGridProbesReportEveryStageInSignalOrder`

`// Seven stages report; an out-of-range index answers 0.`

The probe pair is what the README's grid table and the plate-swing work are read through, so it
needs its own cover: seven stages, every one of them reporting a grid that moved, and an
out-of-range index answering 0 rather than reading off the end of the array.

### `MesaMark2cThdDoesNotFallAsLeadDriveRises`

`// The sweep starts at 0.05, where the knob passes signal.`

Spec section 4: the metric saturates once every stage clips, so a rising spectrum is not
required of Lead Drive; what is required is that turning it up never makes the amp cleaner,
within the 1.0 percentage point that section allows for the wave squaring off. Measured with the
shared THD analyser at a bin-exact 1 kHz and a 0.2 input.

The sweep starts at 0.05, the first position that passes signal: audio_taper(0) is exactly zero,
so the knob's floor is a hard mute and the analyser would read noise against noise.

### `MesaMark2cNeverExceedsFullScaleWithoutALimiter`

`// The output stays in [-1, 1] and reaches the rail.`

A plain clamp is what bounds the output, so driving Lead Drive, Master 1 and Output to their
ceilings must still leave every sample inside [-1, 1] and must actually reach the rail;
otherwise the test proves headroom rather than the clamp.

### `MesaMark2cHasACleanEndAtMinimumGain`

`// Lead Drive 0.02 at -40 dBFS reads 0.20 % THD.`

The clean end the effects loop network and Master 1 at node F give the amp: at Lead Drive 0.02
and -40 dBFS the chain reads 0.20 % THD (2026-09-08), where it read 40.7 % with V2A's plate
meeting V2B's grid directly and Master 1 scaling the output. Lead Drive 0.02 is the lowest
position this test uses, not a threshold: the control passes signal below it too.

### `MesaMark2cDefaultSettingsPeakNearMinusSixDbfsForAMinusTwelveDbfsInput`

`// A -12 dBFS 82 Hz sine comes out at about -6 dBFS.`

The shipped calibration point, and what pins MesaMark2c::K_MASTER_UNITY_GAIN: a -12 dBFS 82 Hz sine
into an untouched amp comes back out at about -6 dBFS, the frequency all four amplifiers pin at
(DA5 spec section 6).

### `MesaMark2cDryLatencyIsOneOversampledRegion`

`// The latency is one Oversampler8x round trip.`

V1A through the sag stage is a single 8x region, so the reported dry latency is one
Oversampler8x round trip; it is pinned against the oversampler rather than a literal.

### `MesaMark2cToneStackSitsBeforeTheLeadStages`

`// The stack is ahead of the distortion: treble moves THD.`

A brighter stack drives the lead stages harder, which shows as THD only while the chain is on
its knee: at Lead Drive 0.05 and a 0.01 input (-40 dBFS) treble 0 against 1 read 0.41 against
8.91 %, +8.50 pp (2026-09-08). At Lead Drive 1.0 and 0.2 every stage from V3B on is a square
with or without treble and the same difference is 0.03 pp; that is the amplifier, 110 dB of gain
fed a hot line level, so the operating point is the knee, not the ceiling.

Since 2026-09-20 the deck is terminated by V1A's 44 k plate impedance and Volume 1's 1 M, which
costs 4.1 dB ahead of the four lead stages, so the same measurement sits lower on the knee and
the difference is +3.32 pp. The threshold moved with it. The property the test exists for is
unchanged: the stack is ahead of the distortion, so treble moves THD.

### `MesaMark2cEqBandBoostsItsCentre`

`// At Master 1 -20 dB the power section is linear.`

The 750 Hz slider's full lift reaches the output only where the power section is linear, so
Master 1 sits 20 dB down: the lift read 16.93 dB there against 16.09 dB at 0 dB, where the
inverter's compression takes the rest (2026-09-08). The same repair the VH4's Deep test needed.
The centre moved from 979.5 Hz on 2026-09-20 with the band's inductor
(docs/agent/2026_09_20_V0_1_0_RELEASE/REPORT.md).

### `MesaMark2cFallsSilentAtEveryPresencePositionWhenTheDriveStops`

`// The ratio sat at 0.92 while the loop oscillated.`

The power amplifier's feedback loop oscillated at 11.8 kHz on 2026-09-21 when its depth was set
from the presence sweep alone, and the ratio below sat at 0.92 instead of falling.
docs/agent/2026_09_20_V0_1_0_RELEASE/REPORT.md carries the measurement.

### `MesaMark2cSweepSoundsTheSameWhateverTheHostBlockSize`

`// Coefficients are rebuilt once per control period.`

Coefficients are rebuilt once per control period, not once per block, so a knob under a sweep
reaches the audio at the same moments however the host cuts the work up. Rebuilding per block
made the 256-frame render lag the 32-frame one by up to a whole block.

### `MesaMark2cPullDeepStopsV2bRemovingBass`

`// V2B's cathode shelf is 0.6972 deep, -3.13 dB.`

V2B's cathode shelf is 0.6972 deep, -3.13 dB. Out, its zero sits at 339 Hz and 80 Hz is below
it; in, the zero drops to 10.3 Hz and 80 Hz climbs back to the shelf's top.

### `MesaMark2cPullBrightLiftsTrebleAtBothOfItsCapacitors`

`// The switch tilts the amplifier; it does not raise it.`

One switch, two capacitors: a second 250 pF beside the tone stack's treble cap and 180 pF across
Volume 1's top section. Both pass treble past their own corner, so what the switch does is tilt
the amplifier, not raise it.

## `test_microphone_model.cpp`

### `MicrophoneModelCardioidAtFiveCentimetresGainsFifteenPointFiveDecibels`

`// At 100 Hz the two distances differ by 15.47 dB.`

Spec section 8.1 states its 15.5 dB at 100 Hz, which is not a band centre. At 100 Hz the boost
is 10.9637 at 5 cm and 1.0164 at 3 m: 0.5 + 0.5 \* 10.9637 = 5.9819 (15.54 dB) against 1.0082
(0.07 dB), a difference of 15.47 dB.

`// At 125 Hz the two distances differ by 13.75 dB.`

Band 0 is 125 Hz: the boost is 8.7915 at 5 cm and 1.01054 at 3 m, so the model reads 0.5 + 0.5
\* 8.7915 = 4.8957 (13.80 dB) against 1.00527 (0.046 dB), 13.75 dB apart.

### `MicrophoneModelProximityBoostIsTheSphericalWaveTerm`

`// sqrt(1 + 10.9180^2) = 10.9637 at 100 Hz and 5 cm.`

f_p = c / (2 pi r) is 1091.8 Hz at 5 cm and 182.0 Hz at 30 cm. 343 / (2 pi \* 100 \* 0.05) =
10.9180, sqrt(1 + 119.204) = 10.9637.

### `MicrophoneModelCardioidAtThirtyCentimetresGainsThreePointSevenDecibels`

`// At 100 Hz and 30 cm the capsule gains 3.74 dB.`

At 100 Hz, 0.5 + 0.5 \* 2.07634 = 1.53817, which is 3.74 dB over the far-field value of 1;
against the same capsule at 3 m (0.07 dB) the difference is 3.67 dB.

`// At 125 Hz and 30 cm the capsule gains 2.82 dB.`

Band 0, 125 Hz: the boost is 1.76612 at 30 cm, so 0.5 + 0.5 \* 1.76612 = 1.38306 (2.82 dB), 2.77
dB over the capsule at 3 m.

### `MicrophoneModelFigureEightRejectsNinetyDegreesByFortyDecibels`

`// The pattern term floors at K_DECIBEL_FLOOR, never a NaN.`

cos(pi/2) is 6.1e-17 in double, so the pattern term is of that order and the decibel conversion
floors it at K_DECIBEL_FLOOR; no band may be a NaN and every band is at least 40 dB below the same
capsule on axis.

## `test_modal_bank.cpp`

### `ModalBankRingsAtEachModesFrequency`

`// A parabola through three bins places the peak.`

The parabola through the three bins around the maximum places the peak between bins, which are
0.73 Hz apart against a 0.34 Hz tolerance.

### `ModalBankDecaysAtTheDampingItWasGiven`

`// The slope between two windows is the 60 dB time.`

The RMS of equal windows falls linearly in decibels along r^n; the slope between two windows
inside the ring is the 60 dB time.

## `test_modal_solver.cpp`

### `DiffuseFieldIsEyringOverTheCabinetsDirectivity`

`// The field is Eyring over the 4x12's directivity.`

The garage: Eyring's 1.342, 1.333, 1.161, 1.146, 1.108 and 0.893 s over 0.161 \* 60 m3 times 16
pi, and the 4x12's directivity index of 1.12, 2.99, 7.59, 12.01, 15.74 and 18.82 dB taken back
out, give 5.39, 3.48, 1.05, 0.376, 0.154 and 0.0610 per hertz.

`// The field is the cabinet's, not the room's alone.`

A cabinet with one driver and a narrower baffle radiates its bass more evenly, so the same room
holds more of it; the field is the cabinet's, not the room's alone.

### `ModalSolverFindsTheGaragesEigenfrequencies`

`// f = (c/2) sqrt((nx/6)^2 + (ny/4)^2 + (nz/2.5)^2).`

(c / 2) sqrt((nx / 6)^2 + (ny / 4)^2 + (nz / 2.5)^2) at c = 343 for the first eight: 28.5833,
42.8750, 51.5293, 57.1667, 68.6000, 71.4583, 74.3167 and 80.8964 Hz.

### `ModalSolverCountsOneHundredAndEightySevenModesBelowTheGaragesSchroederFrequency`

`// Weyl's count is 134.11 + 50.63 + 5.07 = 189.80.`

(4 pi / 3) 60 (278.2 / 343)^3 + (pi / 4) 98 (278.2 / 343)^2 + (50 / 8) (278.2 / 343) = 134.11 +
50.63 + 5.07 = 189.80.

### `ModalSolverDampsAnAxialModeByTheTwoWallsItBouncesBetween`

`// The decay ratio of the two modes is 1.846.`

Both modes sit under 125 Hz, so gypsum holds 0.29, the doorway face 0.716 and concrete 0.01. The
soft pair counts twice for (1, 0, 0) and once for (0, 0, 1): delta_x = (343 / 960) (2 (0.29 +
0.716) 10 + (0.02) 15 + (0.02) 24) = 7.467 nepers/s, delta_z = (343 / 960) ((0.29 + 0.716) 10 +
(0.02) 15 + 2 (0.02) 24) = 4.044 nepers/s, a decay ratio of 1.846.

### `ModalSolverMatchesTheOctaveBelowTheCrossoverToTheDiffuseField`

`// The octave below f_s holds the diffuse field's power.`

Averaged over the cabinet and the listener, one mode's power for unit white input is
level_scale^2 times the section's 2 / (1 - r^2); the octave [f_s / 2, f_s) sum must equal the
power the diffuse field of that room and cabinet holds in the octave, in all three reference
rooms, so the modal layer and the tail read one reference.

`// The garage's octave holds 2.334e-2 of the variance.`

The garage's octave [139.09, 278.18) holds 2.334e-2 of a unit white input's variance. Eyring
alone, 16 pi times 1.33 to 1.34 s over 0.161 \* 60 m3 spread as 2 / fs over the octave, would
give 4.02e-2; the ratio sits between the 4x12's mean radiated power at the two bands the octave
spans, 10^(-0.299) at 250 Hz and 10^(-0.112) at 125 Hz.

### `ModalSolverReadsTheMicrophoneAtOnePoint`

`// The ears sit 8.5 cm to either side of the listener.`

The microphone listener puts psi at microphone_position in both ear gains; the ears listener
puts psi at the two ears 8.5 cm to either side of listener_position, so a listener at (5.4, 3.4,
2.0) facing +x reads psi at y = 3.485 and y = 3.315. The resonators, the count, the crossover
and the level are the room's, the same for both.

## `test_musical_clock.cpp`

### `MusicalClockPublishesTheStepTheTempoAndMeterName`

`// An eighth-note beat is exactly half the quarter.`

An eighth-note beat is half the quarter, exactly, because the halving is a division by two of a
value the double holds without loss.

### `MusicalClockPlacesAnAwkwardMeterAtAHandComputedFrame`

`// The frame sits 4999.484 into a 10480.349 frame beat.`

The eighth truly begins at frame 2955458.516, so the frame sits 4999.484 into a 10480.349 frame
beat.

### `MusicalClockKeepsBarStartsWithinOneSampleFarDownTheTimeline`

`// Every start derives from the absolute beat index.`

The step is rounded to the nearest 2^-32 of a frame and every start is derived from the absolute
beat index, so the gap against the ideal start never grows with the bar number.

## `test_ne570_compander.cpp`

### `Ne570CompressorFollowsTheTwoToOneLaw`

`// V_out = sqrt(V_ref * V_in) with V_ref = 6.6125 V.`

N6: V_out = sqrt(V_ref \* V_in) with V_ref = 6.6125 V.

```text
V_in = 0.066125 (0.01 V_ref) -> sqrt(6.6125*0.066125) = 0.66125     (exactly 0.1 V_ref)
V_in = 0.66125  (0.1 V_ref)  -> sqrt(6.6125*0.66125)  = 2.0910639   (0.3162278 V_ref)
V_in = 6.6125   (1.0 V_ref)  -> 6.6125                              (exactly V_ref)
V_in = 26.45    (4.0 V_ref)  -> sqrt(6.6125*26.45)    = 13.225      (exactly 2 V_ref)
```

The 2 % tolerance is the rectifier's own ripple at 1 kHz: the ripple sits at 2 kHz and the
23.4051 Hz pole leaves 23.4051/2000 = 1.17 % of it on the control voltage.

### `Ne570ExpanderFollowsTheOneToTwoLaw`

`// V_out = V_in^2 / V_ref.`

N6: V_out = V_in^2 / V_ref.

```text
0.66125    -> 0.66125^2/6.6125    = 0.066125
2.0910639  -> 2.0910639^2/6.6125  = 0.66125
6.6125     -> 6.6125
13.225     -> 13.225^2/6.6125     = 26.45
```

### `Ne570EnvelopeReachesSixtyThreePercentAtSixPointEightMilliseconds`

`// tau = 10k * 0.68u = 6.8 ms, 326.4 samples at 48 kHz.`

G(t) = (G_i - G_f)\*e^(-t/tau) + G_f with tau = 10k \* 0.68u = 6.8 ms exactly. 1 - e^-1 =
0.6321206 of a step, at 0.0068 \* 48000 = 326.4 samples.

`// The 63.21 % point is 0.6321206 * pi/2 = 0.9929407.`

The rectifier reads |x| \* pi/2, so its final value is pi/2; the 63.21 % point is 0.6321206 \*
pi/2 = 0.9929407.

### `Ne570CompressorGainSaturatesAtTheEnvelopeFloor`

`// Below 6.6125e-4 V the gain stops at 1e4.`

Below V_ref \* K_ENVELOPE_FLOOR = 6.6125e-4 V the gain stops at 1/K_ENVELOPE_FLOOR = 1e4 and the law
goes linear, which is the modelled end of the gain cell's range (N6).

### `Ne570RectifierRippleReachesTheSignalAtLowFrequencies`

`// The third harmonic has a floor, not a ceiling.`

Dossier 3, "Compander behaviour": the 23.4051 Hz rectifier corner is deliberately fast, and the
datasheet's stated penalty - ripple modulating the gain cell into third-harmonic distortion on
low-frequency material - is a feature to reproduce, not an artefact to filter out. At 50 Hz the
ripple sits at 100 Hz and the pole leaves 23.4051/100 = 23.4 % of it, so the third harmonic must
be clearly present. This asserts a floor, not a ceiling: the test fails if a future
"improvement" smooths the envelope.

### `Ne570RectifierDoesNotFoldItsOwnHarmonicsOntoTheCarrier`

`// The -70 dB bound covers the comb's tail from bin 4.`

|sin| carries a harmonic at every 2k\*f0, so a rectifier sampled at the host rate folds them,
and 48000/6 is the tone that folds one worst: the sixth harmonic lands on 48001.5 Hz and aliases
to 1.46 Hz, two analysis bins off the carrier and far inside the 23.4051 Hz pole, so it
modulates the gain cell at full weight. The board's rectifier is continuous time and does none
of this, which is why K_RECTIFIER_OVERSAMPLING exists.

The fold itself sits at +-2 bins, inside the Blackman-Harris main lobe (+-3 bins) and so
unmeasurable here; the search below starts at bin 4 and therefore bounds the comb's tail, not
its peak. The worst line outside the main lobe reads -76.05 dB with the sub-stepping and -57.67
dB without it, so the -70 dB bound sits 6 dB above the measurement (headroom against run-to-run
noise) and 12.3 dB below what a host-rate rectifier produces (still catching the regression).
The tail bound and Ne570RectifierRippleReachesTheSignalAtLowFrequencies's floor are the
invariant pair: the genuine 100 Hz ripple a 50 Hz tone makes must survive, and it does, unmoved
at -23.75 dB. Driven at 2.0 V, the project-wide full-scale peak (SushiDSP::K_FULL_SCALE_VOLTS,
modules/foundation/core/include/SushiDSP/foundation/core/calibration.hpp), to catch the fold at the loudest stimulus the converter
passes; the ripple test above drives K_REFERENCE_VOLTS because it is exercising the 2:1 law's own
pivot point instead.

### `Ne570HalvesDoNotShareState`

`// The expander is open-loop; instances share no state.`

The expander is open-loop: it tracks the reconstructed signal, never what the compressor did
(dossier 1.9 item 2). Two instances driven differently must hold different envelopes.

## `test_newton_raphson_solver.cpp`

### `NewtonRaphsonSolverJointOverloadMatchesSplitOverloadBitForBit`

`// Both overloads must walk identical iterates.`

The circuit stages moved to the joint form to share one exp() between residual and derivative;
that must be a pure cost change, so both forms must walk identical iterates.

## `test_output_transformer_stage.cpp`

### `OutputTransformerStageSecondaryCarriesNoMean`

`// A transformer's secondary passes no mean.`

A push-pull pair driven through asymmetric clipping delivers a square whose two halves are not
equally long, and a saturating core compresses the taller half more than the shorter, so the
saturated waveform carries a mean even after the rolloff has removed the square's own. A
transformer's secondary sees d(flux)/dt and passes no mean at all (DA5 spec section 14).

Stimulus: a 220 Hz square of amplitude 2.2 at 40 % duty, two seconds at 48 kHz, the mean taken
over the second second. With the saturation as the last element the stage read -0.063500
(2026-09-07); with the secondary's mean removed it reads +0.000005.

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

## `test_panel_library.cpp`

### `PanelLibraryReturnsAHandAuthoredPanelForEveryAmplifier`

`// The registry says which products are amplifiers.`

The registry is what says which products are amplifiers, so adding one does not mean remembering
to name it here.

### `PanelLibraryGivesEveryRegisteredNodeAPanelWithAControlPerParameter`

`// Every parameter of every node has a control.`

A node with no hand-authored entry still has to reach every one of its parameters, or the
product ships a knob nobody can turn. Nothing here names a node type, so adding one is a module
and a registry line rather than a test to remember.

## `test_peavey5150.cpp`

### `K_LINEAR_PATH_PRE_GAIN`

`// The shelves are read with a small-signal path ahead.`

measure::frequency_response() sweeps at -40 dBFS, and at the default setting that squares every
stage from V2A on, so a shelf measured there reads the amplifier's compression rather than the
shelf: 6.74 dB for presence and 5.75 dB for resonance on 2026-09-07's chain. Both shelves are
linear sections after the decimator, so the honest place to read them is with the path ahead of
them small-signal. Turning PRE GAIN and the input trim down is not enough on its own - the trim
floors at -24 dB and the power section is still saturating - so POST GAIN comes down too: it
sits ahead of the driver, the phase inverter and the output tubes, which is where the remaining
compression is. At this setting the two shelves read 9.90 dB and 9.90 dB against their 10 dB
target.

### `K_SPECTRUM_BLOCKS`

`// measure::thd() needs at least 65536 samples.`

measure::thd() windows a 65536-point FFT at the end of the signal, so anything handed to it must
be at least that long or its offset underflows: 300 blocks of 256 frames is 76 800 samples.

### `Peavey5150DescriptorExposesNineParametersInContractOrder`

`// The index order is the contract of hosts and presets.`

The index order is the contract a host and a saved preset address these by, so it is pinned here
rather than merely checked for membership.

### `Peavey5150ToneChangeDoesNotRebuildFiltersOffTheAudioThread`

`// The control thread stores a target and nothing else.`

The control-thread call stores a target and nothing else: the coefficients the audio thread last
built are still the old ones, which is exactly what "dirty" reports.

### `Peavey5150IsDcStableAcrossManyBlocksAtHighGain`

`// A stuck operating point shows as a drifting offset.`

Every knob at both extremes, 200 blocks of a DC-offset drive each: the output must stay finite
throughout and the settled DC term must stay below -40 dBFS. A cascade of eight grid-conduction
solvers around a feedback-derived resonance shelf is where a stuck operating point would show
up, and it would show up as a drifting offset rather than as a NaN.

### `Peavey5150ThdDoesNotFallAsPreGainRises`

`// The sweep starts at 0.02, where the knob passes signal.`

Spec section 4: once every stage clips, THD saturates, so the metric is not asked to rise - it
is asked not to fall. The tolerance is 1.0 percentage point across the knob's travel, because
the metric eases once the phase inverter clips and the stages ahead of it square the wave into
faster-falling harmonics.

The sweep starts at 0.02, the first position that passes signal: VR2 at the bottom mutes the
amplifier and the reading there is the FFT's floor on denormals, not distortion.

Measured 2026-09-07 on the volt-true power stage: 24.20, 25.97, 26.26, 26.51, 26.37, 26.28,
26.32, 27.09, 27.25 percent; the largest fall is 0.22 pp from 0.30 to 0.40.

### `Peavey5150EvenHarmonicsRiseFromLowToFullPreGain`

`// The two points are drive levels, not knob positions.`

What this proves is its name and no more: the amplifier's even-harmonic content is higher at
full pre gain than at a low one. It was written to assert V2A's asymmetry - the 39 kOhm
unbypassed cathode starves that stage to 0.102 mA and puts its plate within 15 V of the rail
(dossier section 3), so it clips one half-cycle far harder than the other - but an output-side
measure cannot attribute even harmonics to one stage, and the sweep below is not monotonic, so
the test is named for what it measures.

The two points are physical drive levels, not knob positions. The comparison was calibrated on
the n\*sqrt(n) taper at pre gain 0.2, which passed 0.089443 of the pot; the exponential audio
taper of DA5 passes that same fraction at (ln(1 + 80 \* 0.089443) / ln 81) = 0.4776, so the
quiet point moved with the taper and the drive it stands for did not.

`// The gate is a factor of 2 against a measured 3.18.`

Measured 2026-09-07 on the volt-true power stage: 0.04113 at pre gain 0.4776 against 0.13064 at
1.0, a factor of 3.18. The gate is a factor of 2. On the Shockley-knee chain the same two drive
levels read 0.00865 against 0.09843, a factor of 11.4, and the gate was 4; the Norman grid law
and the real-volt power tubes leave far less of the output square, so the two points sit closer
together.

The measure is not monotonic across the knob - 0.0421, 0.0460, 0.0724, 0.0535, 0.0385, 0.0411,
0.0454, 0.1203, 0.1306 at pre gain 0.05, 0.10, 0.20, 0.30, 0.40, 0.4776, 0.50, 0.70 and 1.00 -
because at this stimulus every stage from V5A on is past its window, so the amplifier's even
harmonics are the push-pull pair's as much as V2A's. V2A's asymmetry is real; what an
output-side measure cannot do is attribute it, which is why this test is named for the two
points it compares. modules/nodes/peavey5150/README.md, "The cold clipper's asymmetry", carries
the sweep.

### `Peavey5150GridProbesReportEveryStageInSignalOrder`

`// The probe hooks are pinned, not their figures.`

The two test hooks the plate-swing spec added, shaped the way
Jcm800GridProbesReportEveryStageInSignalOrder is: spec section 4 asks for the readings to be
recorded rather than asserted, so this pins the hooks and not the figures. Eight stages in
signal order - V1A, V1B, V2A, V2B, V5B, V5A, V3A and the phase inverter.

### `Peavey5150PresenceAtMaximumLiftsTwoKilohertzByTenDecibels`

`// Presence at maximum lifts 2 kHz by 9.90 dB.`

Dossier section 6, quoting Peavey's own specification table: "Active Presence: +10 dB 2 kHz".
The shelf's corner sits at 600 Hz so its plateau has arrived by 2 kHz; the comment beside
K_PRESENCE_HZ in peavey5150.cpp carries why. Read on the small-signal path for the reason above
small_signal_node(); measured 2026-09-07: 3.32 dB flat, 13.22 dB full, a 9.90 dB lift.

### `Peavey5150ResonanceAtMaximumLiftsTheLowEndByTenDecibels`

`// Resonance at maximum lifts 100 Hz by 9.90 dB.`

Dossier section 6: "Active Resonance: +10 dB at the cabinet resonant frequency". The cabinet
resonance is a modelling choice at 100 Hz, the dossier derives none, and the shelf's own corner
sits at 330 Hz so its plateau has arrived by then. The knob is flat at 0 and full at 1, so those
two are the pair compared. Read on the small-signal path for the reason above
small_signal_node(); measured 2026-09-07: -7.83 dB flat, 2.07 dB full, a 9.90 dB lift.

### `Peavey5150NeverExceedsFullScaleWithoutALimiter`

`// The output stays in [-1, 1] and reaches the rail.`

A plain clamp is what bounds the output, so driving pre gain, post gain and output to their
ceilings must still leave every sample inside [-1, 1] - and must actually reach the rail, or the
test would be proving only that the amp has headroom.

### `Peavey5150DefaultSettingsPeakNearMinusSixDbfsForAMinusTwelveDbfsInput`

`// A -12 dBFS 82 Hz sine comes out at about -6 dBFS.`

The shipped calibration point, and the measurement that pins K_POST_GAIN_UNITY_GAIN: a -12 dBFS 82
Hz sine into an untouched amp comes back out at about -6 dBFS, the frequency all four amplifiers
pin at (DA5 spec section 6). At 1 kHz, where this constant was pinned before, the resonance
shelf's lift below 330 Hz put the low E on the output clamp.

`// The first blocks hold the startup transient.`

The first blocks are skipped so the measurement is of the settled amp, not of the filters' and
the sag stage's startup transient.

### `Peavey5150HasACleanEndAtMinimumGain`

`// The nominal lead input is -83.01 dBFS.`

Spec section 4's acceptance for the plate-swing phase: a player who turns PRE GAIN down must
reach a setting the amplifier does not square. The 5150 asserts that at Peavey's own nominal
lead input and no louder: -80 dBV is 1e-4 V RMS, so 1.4142e-4 V peak, which against
K_FULL_SCALE_VOLTS 2.0 is an amplitude of 7.071e-5 full scale, -83.01 dBFS. A 20 mV guitar sits 46
dB above the nominal the manufacturer prints for this channel, so the -40 dBFS figure is
measured and printed but not asserted.

The knob is read at 0.02, the lowest position this test uses. pre_gain_attenuation() is an
exponential audio taper, exactly zero only at the bottom of VR2's travel, so 0.0 mutes the
amplifier and the THD there is the FFT's own floor on denormals rather than distortion.
Positions between pass signal - 0.02 gives 0.00115 - so 0.02 is a choice, not a threshold.

Measured 2026-09-07 on the Norman grid law, the solved inverter bias and the audio taper: 0.499
% at the nominal input, and 21.73 % at -40 dBFS. At -40 dBFS the amplifier squares from V5A on.
modules/nodes/peavey5150/README.md, "The clean end", carries the table.

### `Peavey5150DryLatencyIsOneOversampledRegion`

`// The latency is one Oversampler8x round trip.`

The whole amplifier is one 8x region, so the reported dry latency is one Oversampler8x round
trip. A host compensates by this number, so it is pinned against the oversampler itself.

## `test_pentode_el34.cpp`

### `PentodeEL34MatchesKorenAtThreeOperatingPoints`

`// The three points are hand-computed from Koren's law.`

mu = 11, kp = 60, kvb = 24, kg1 = 650, kg2 = 4500, kx = 1.35.

(1) Vpk = 400, Vg2k = 400, Vg1k = -37  (a JCM800 idle point at -37 V fixed bias)

```text
u_arg = 60*(1/11 - 37/400) = -0.0954545454545452
s = ln(1+exp(u_arg)) = 0.646558421976595,  E1 = 400*s/60 = 4.31038947984397
knee = atan(400/24) = 1.51086817167369
Ip  = 2 * E1^1.35 / 650 * knee = 3.34149261300818e-2 A   (33.4 mA idle)
dIp/dVg1k = 4.98316907313410e-3 S,  dIp/dVg2k = 5.73718514953930e-4 S,
dIp/dVpk  = 3.30555615548759e-6 S   (a ~300 kOhm plate resistance: pentode, as expected)
z = 400/11 - 37 = -0.636363636363633 < 0, so Ig2 = 0 and its whole row is 0 - the screen
draws nothing at idle, which is what the max(0, .) clamp is for.
```

(2) Vpk = 250, Vg2k = 300, Vg1k = -20

```text
u_arg = 60*(1/11 - 20/300) = 1.45454545454545,  s = 1.66440636412608
E1 = 300*s/60 = 8.32203182063039,  knee = atan(250/24) = 1.47508961870809
Ip = 7.92947981392609e-2 A,  dIp/dVg1k = 1.04281593600714e-2 S,
dIp/dVg2k = 1.05203721563143e-3 S, dIp/dVpk = 2.04537709631731e-5 S
z = 300/11 - 20 = 7.27272727272727,  Ig2 = z^1.35 / 4500 = 3.23652408855726e-3 A
dIg2/dVg1k = 1.35 * z^0.35 / 4500 = 6.00779783938442e-4 S
dIg2/dVg2k = 6.00779783938442e-4 / 11 = 5.46163439944038e-5 S
```

(3) Vpk = 100, Vg2k = 400, Vg1k = -5   (hard into conduction, plate pulled down)

```text
u_arg = 60*(1/11 - 5/400) = 4.70454545454546,  s = 4.71355874131221
E1 = 400*s/60 = 31.4237249420814,  knee = atan(100/24) = 1.33525134607403
Ip = 4.31496731894557e-1 A,  dIp/dVg1k = 1.83712705886168e-2 S,
dIp/dVg2k = 1.68594235250184e-3 S,  dIp/dVpk = 7.33338162266083e-4 S
z = 400/11 - 5 = 31.3636363636364,  Ig2 = 2.32789367120296e-2 A,
dIg2/dVg1k = 1.00200640630040e-3 S,  dIg2/dVg2k = 9.10914914818548e-5 S
```

### `PentodeEL34GridCurrentFollowsNormansSoftLaw`

`// Ig1 = 1e-6 * ln(1 + exp((Vg1k - 0.5)/0.15)).`

Ig1 = 1e-6 \* ln(1 + exp((Vg1k - 0.5)/0.15)), dIg1/dVg1k = (1e-6/0.15) \* logistic(same).

```text
Vg1k = -1.5 : argument = -13.3333333333333, ln(1+exp) = 1.61959548076714e-6
              Ig1 = 1.61959548076714e-12 A,  dIg1 = 1.07972944614873e-11 S
Vg1k =  0.5 : argument = 0, ln(2) = 0.693147180559945
              Ig1 = 6.93147180559945e-7 A,  dIg1 = (1e-6/0.15)*0.5 = 3.33333333333333e-6 S
Vg1k =  1.0 : argument = 3.33333333333333, ln(1+exp) = 3.36838574941280
              Ig1 = 3.36838574941280e-6 A,  dIg1 = 6.43703202889193e-6 S
```

### `PentodeEL34JacobianMatchesCentredDifferencesEverywhere`

`// The screen law kinks at z = 0; skip it.`

The screen law has a max(0, z) kink at z = Vg2k/mu + Vg1k = 0. A centred difference that
straddles the kink averages two different one-sided slopes and is not the derivative there, so
skip a step-sized neighbourhood of it.

### `PentodeEL34ScreenCurrentIsCOneAcrossItsKink`

`// kx > 1, so the screen current is C1 at z = 0.`

z = Vg2k/mu + Vg1k crosses zero at Vg1k = -Vg2k/mu. kx = 1.35 > 1, so z^kx and kx\*z^(kx-1) both
vanish continuously there: no slope step for the Newton solver to trip on.

### `PentodeEL34PlateCurrentKneesOverWithPlateVoltage`

`// Ip factors into a grid term times atan(Vpk/kvb).`

The pentode's whole character is atan(Vpk/kvb): Ip factors into a grid-only term times that
knee, so quadrupling the plate voltage raises Ip only by atan(400/24)/atan(100/24) =
1.51086817167369 / 1.33525134607403 = 1.13152342150114. A triode, whose plate current tracks Vpk
directly, would have risen far more.

### `PentodeEL34JointFormAgreesWithTheSeparateEntryPoints`

`// current() and jacobian() forward to the joint form.`

Same shape as the triode case. current() and jacobian() forward to the joint form, so these
agree exactly by construction today, and pinning that keeps the three entry points from drifting
apart later. The first screen voltage sits below K_SCREEN_FLOOR_VOLTS, so the clamped Vg2k column
is exercised too.

## `test_pentode_stage.cpp`

### `PentodeStageSaturatesInsideItsPlateSwing`

`// The plate is bounded by the tube's swing on each side.`

The plate is bounded by the tube's own swing on each side: toward the supply by the idle
current's drop across the reflected load, toward the bottom by the rest of the datasheet's
full-signal swing. A grid driven far past cutoff or far into conduction reaches the tanh ceiling
and nothing else.

### `PentodeStageIsNearlyLinearForSmallInput`

`// The grid sits at 0.867 of the input inside the window.`

Inside the window the grid draws no current, the 220 kOhm leak against the 33.8 kOhm source sets
the grid at 0.867 of the input, and the plate reads that times the gain.

### `PentodeStageGridConductsAtTheCathodeNotAtTheBiasPoint`

`// The grid law is referenced to the cathode.`

The grid law is referenced to the cathode: the whole |Vk| window is clean, and the grid only
draws current once the input has carried it past 0 V. A +30 V swing on a -38 V grid comes back
as the linear plate answer through the tanh, to the float; the same swing on a -20 V grid has
crossed its window, the 150 kOhm conducting slope loads the 33.8 kOhm source, and the plate is
held 4.28 V short of that answer (measured 2026-09-07).

### `PentodePairIsOddSymmetricInsideTheWindowAndABiasDifferenceMovesTheHandover`

`// The composite is odd while both grids stay in window.`

The push-pull composite el34_a(u) - el34_b(-u) is odd in u while both grids stay inside their
windows: the two tubes are the same function of their input. Since the grid law is referenced to
the cathode, two halves with different bias voltages have different windows and the composite
acquires an even part once the drive reaches the nearer window; a cathode-bias difference is a
real handover asymmetry, where the bias-referenced stage of DA4 cancelled it exactly. Measured
2026-09-07: an even part of exactly 0 at equal bias, and 3.411 V for -38 V against -25 V at 60 V
of drive, on an odd reference of 183.5 V.

### `PentodeStageAntialiaserAntiderivativeDifferentiatesBackToTheTransfer`

`// dG/du = bound(-gain u) dg/du on the EL34's table.`

The EL34's knee sits 38.5 V above rest and its plate reaches 280 V toward the bottom, so the
identity the triode's antialiaser keeps, dG/du = bound(-gain u) dg/du, is checked on the power
tube's own table.

## `test_power_supply_sag_stage.cpp`

### `PowerSupplySagStageRecoversTowardUnityAfterSilence`

`// The sag envelope decays over silence.`

With zero input the sag envelope should decay, so a subsequent unity-input sample should be
closer to unity than the deeply-sagged sample was.

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

## `test_resampler_coefficients.cpp`

### `ResamplerKernelHasTheCommittedShape`

`// 34 * 512 + 1 endpoint + 1 guard = 17410.`

34 zero crossings \* 512 entries + 1 endpoint + 1 guard = 17410  (DR1+DR2 plan, Task 1.1
controller ruling: HALF_WIDTH grew from the planned 32 to clear the -100 dB stopband target with
margin).

`// The centre tap of the kernel is 2*fc, not 1.`

The centre tap of a sub-Nyquist lowpass impulse response is 2\*fc, not 1: at t = 0 the ideal
kernel is 2\*cutoff\*sinc(0) = 2\*cutoff, and the polyphase-DC-sum normalisation (which targets
the branch sums, already \~1 by Poisson summation) barely moves it.

### `ResamplerKernelSumsToOnePerPhase`

`// Branch p is h(p/512 + k) for integer k.`

Every polyphase branch must have unity DC gain, or a rate conversion would change the level of a
constant. Branch p is h(p/512 + k) for integer k, over both wings.

### `ResamplerKernelMeetsItsPassbandAndStopbandTargets`

`// The passband is |f| <= 0.400, the stopband >= 0.500.`

The full symmetric kernel, evaluated as a filter running at 512x the input rate. Its passband is
|f| \<= 0.400 input-rate cycles/sample and its stopband |f| >= 0.500. DFT directly at the
frequencies of interest - the full kernel is too many taps for a naive FFT here, and only a
handful of frequencies matter.

`// The kernel is even and real, so is its transform.`

The kernel is even and real, so the transform is real; the 1/512 factor turns the sum into the
continuous-time integral the interpolating filter actually applies.

### `ResamplerKernelIsSmoothEnoughForLinearInterpolation`

`// The predicted interpolation error bound is 3.8127e-6.`

N1: the linear-interpolation error is bounded by |h''| \* step^2 / 8, and the committed table's
second difference already carries the step^2 factor. The predicted bound is 3.8127e-6 (-108.4
dB); assert it against 4.0e-6 so a table designed at half the density fails here rather than
quietly setting the noise floor.

## `test_rig_controller.cpp`

The file holds five groups of cases under one test binary. Separator lines used to mark them;
the groups are listed here and the file is reported as a candidate for splitting, not split:

- Per-slot bypass, from `RigControllerBypassBridgesTheSlotsNeighbours`.
- The impulse-response folder, from `RigControllerRemembersTheIrFolder`. The folder is
  preset-level and has no audio behind it.
- Rack-list selection tracking (`applications/host_gui/slot_selection.hpp`), from
  `SlotSelectionStillValidReturnsTrueWhenSlotIsInTheList`.
- The room: its scene in the preset, its publish pacing and its HRTF (DE6).
- Snapshot and edge accessors (`applications/host_gui`, Phase 2 chain-type rewiring), from
  `RigControllerSnapshotRestoreKeepsParametersAndCabinetPath`.

### `RigControllerRemoveDropsFromChainAndReconnectsSurvivors`

`// The chain re-links around a removed middle slot.`

b's removal must not leave a and c disconnected, the chain must re-link around the gap,
matching today's behavior where removing a middle slot still produces one working chain.

### `RigControllerBuildsSplitterTwoBranchMixerShapeAndProcessesCorrectly`

`// The split/mix shape needs publish batching to build.`

Every node exists and the graph compiled/published without throwing, the split/mix shape this
test builds was, before the publish-batching parameter, impossible to construct through the
public API at all (the second add_node() would have republished a two-node, zero-edge graph,
which RigGraph::compile() rejects for having no sink). Confirm the rig actually processes a
block without crashing or throwing.

### `RigControllerAddBranchBuildsTheParallelShapeTheGuiOffers`

`// add_branch() turns one drop into the four-edge shape.`

The whole point of the feature, in the order a user performs it: drag a source, a splitter and
one effect into a straight line, then drop a second effect on the splitter's open row.
add_branch() is what turns that one drop into the four-edge shape RigGraph::compile() accepts,
mixer and all.

### `RigControllerSaveToJsonUsesOwnStoredValueNotLiveNodeState`

`// No process() runs here; the live node still reads 0.5.`

Deliberately no process() call and no further structural edit here, the live node still reads
the pre-edit default (0.5), but save_to_json() must report the controller's own authoritative
0.9f, matching what get_parameter() and the GUI slider both already show.

### `RigControllerBypassBridgesTheSlotsNeighbours`

`// A bypassed slot keeps its place, parameters and edges.`

Per-slot bypass.

A bypassed slot keeps its place in slot_order_, its parameters and its edges; only the published
graph leaves it out, wiring its upstream neighbours straight to its downstream ones.

### `SlotSelectionStillValidReturnsTrueWhenSlotIsInTheList`

`// A SlotId stays valid across insert, move and remove.`

Rack-list selection tracking (applications/host_gui/slot_selection.hpp).

A SlotId stays valid across insert/move/remove, unlike the old int-index scheme, so the only
remaining job is dropping a selection that no longer names a live slot.

### `RigControllerReAmpsARecordedTakeFromTheBarItWasRecordedAt`

`// The re-amp leg is silent before the take's bar.`

The re-amp leg is silent before the bar the take was recorded at, which is the whole point of
the stamp: docs/archive/superpowers/specs/2026-09-08-arrangement-view-design.md §5.

### `RigControllerLoadsAPresetWithNoRoomAsTheStarterScene`

`// A fresh node carries the starter scene.`

A fresh node carries the starter scene and no drag has landed, so the slot took the node's scene
rather than publishing one.

### `RigControllerCoalescesADragToThirtyPublishesPerSecond`

`// At most 1 + floor(30 T) publishes land in T seconds.`

The first edit publishes at once and every later one only after 1 / 30 s has passed since the
previous publish, so the count is bounded by the loop's own wall time: at most 1 + floor(30 T),
which is 3 for a loop inside 100 ms.

## `test_rig_graph.cpp`

### `RigGraphProcessZeroesOutputWhenGraphIsEmpty`

`// The sentinel proves process() writes zero itself.`

Pre-fill with a sentinel to prove process() actively writes zero rather than merely leaving
whatever the caller's buffer already held (e.g. SDL's raw stream buffer replaying stale audio
from a previous block).

### `RigGraphRootNodeStillReceivesNullptrWhenGraphHasNoInput`

`// The graph passes nullptr through to a root node.`

Unchanged behavior: a root node must not crash or misbehave when the graph itself has no live
input (e.g. the existing SDL2 output-only path), GainNode would dereference a null
context.inputs, so this uses ConstantSourceNode (0 input ports) to prove the \*graph\* passes
nullptr through, without the test node needing to read it.

### `RigGraphDisconnectRemovesOnlyThatEdge`

`// A root node is fed context.inputs directly.`

gain_b now has no upstream edge; RigGraph feeds root nodes context.inputs directly, so it must
NOT receive source's output. Use a real live-input buffer here (rather than nullptr) since
GainNode unconditionally reads context.inputs[0] and a null root input would crash the fixture,
not RigGraph.

`// 8.0 comes out only if exactly one edge was removed.`

gain_b lost its edge from source, so it becomes a root fed by device_input (1.0) directly; with
the source->gain_a edge intact, gain_a still becomes last in topological order (unblocked only
once source is processed) and is what gets copied to the graph's output. Its value, 8.0
(source's 4.0 \* gain_a's gain of 2.0), only comes out right if BOTH held: disconnect() actually
removed source->gain_b (else gain_a would still be sandwiched between source and gain_b in a
different topological order, and the last node copied to output would be gain_b at
4.0\*3.0=12.0), and the untouched source->gain_a edge survived (else gain_a would be a root fed
by device_input directly, giving 1.0\*2.0=2.0 instead of 8.0).

### `RigGraphCompileThrowsWhenNoSinkNodeExists`

`// Two unconnected nodes form a graph with no sink.`

Two live nodes, no edges between them at all: neither node has an outgoing edge, but neither has
an incoming edge either (and live_node_count != 1), so neither qualifies as a sink candidate.
This is a genuine zero-sink, non-empty, acyclic graph shape, and compile() must reject it rather
than leaving sink_handle_ pointing at a stale/default handle for process() to misuse.

### `RigGraphFeedsRootNodeExternalInputThroughAFullWidthGatherArray`

`// Every node receives a K_MAX_FAN_IN_OUT-sized array.`

Regression: a multi-input node (MixerNode reads all K_MAX_FAN_IN_OUT slots) placed as a ROOT node , 
no incoming edges, reachable via a hand-authored or rig_preset-loaded JSON whose mixer has an
empty "inputs", used to be handed the host's raw context.inputs pointer. Hosts pass a 1-element
array, so the mixer read 7 pointers past the end of it and dereferenced whatever garbage
followed. Every node must now receive a K_MAX_FAN_IN_OUT-sized array whose unused slots are nullptr.

`// Unused slots are nullptr, never garbage.`

gain_0 defaults to 1.0 and every other slot must be silent (nullptr), never garbage, so the
output is exactly the external input.

### `RigGraphSumsFanInPerChannel`

`// A surplus edge is summed into the slot per channel.`

Two producers reach a consumer that declares one input slot, so the surplus edge is summed into
that slot channel for channel rather than dropped.

## `test_rig_preset.cpp`

### `DeserializeRigFromJsonRebuildsBranchingTopologyAndParameters`

`// The mixer's inputs are handles 2 and 3, in that order.`

Topology: the mixer (preset id 4, handle 4 since deserialize preserves JSON-array add order)
must have exactly the two upstream nodes the JSON's "inputs": [2, 3] declares, the amp (handle
2) and the dry tubescreamer (handle 3), in that order.

### `SerializeThenDeserializeRoundTripsABranchingGraph`

`// Rebuilt handles line up 1:1 with the original's.`

Topology: deserialize_rig_from_json adds nodes into an empty graph in the same JSON-array order
that serialize_rig_to_json wrote them (which itself follows original.nodes()'s add_node()
order), so rebuilt's handles line up 1:1 with original's. Compare upstream_of() as ordered
vectors for every handle to catch a dropped connect() or a reversed fan-in that a "does some
node have the right value" check would miss.

### `DeserializeRigFromJsonRejectsMalformedEntriesWithoutTouchingTheGraph`

`// A rejected preset leaves the graph unmodified.`

Each of these is rejected in the first validation pass, so the caller's graph must come back
completely unmodified, never half-populated with orphan nodes that would then make the caller's
next compile() throw "no sink node found".

`// Id 7 is declared by no entry.`

Dangling "inputs" reference: id 7 is declared by no entry. Previously this escaped as
std::out_of_range from std::map::at(), which does not derive from std::runtime_error.

### `SerializeAndDeserializeStillWorkWithNoCallback`

`// Call sites without a callback behave unchanged.`

Regression guard: every existing call site (no callback argument) must keep compiling and
behaving exactly as before this task.

## `test_rig_topology.cpp`

### `RigTopologyOpeningAPathReportsItForTheCallerToFill`

`// The path the model opens starts empty.`

The model holds identities, so the path it opens starts empty: filling it from the main path is
the caller's work.

## `test_room_motus.cpp`

### `MotusEmptyRoomDecayIsNotShorterThanMeasuredAndAtMostTwiceAsLong`

`// Two times is a ceiling on the error, not a target.`

The module's documented state: a first hand estimate put the empty room 40 to 70 percent too
long in the mid bands, so two times is a ceiling on that error, not a target.

## `test_room_node.cpp`

### `find_peaks`

`// Prominence is measured to the deeper valley.`

The prominence is measured to the deeper of the two valleys before the spectrum climbs back
above the peak on either side.

### `lagrange_impulse`

`// Weight k lands an impulse at whole + 2 - k.`

Weight k reads the sample k - 2 frames past the integer delay, so an impulse at 0 lands at whole
+ 2 - k with that weight.

### `RoomNodePlacesTheFirstOrderPeaksWhereTheSolverSaid`

`// Nothing but taps reaches the first 1749 frames.`

The seven first-order paths of the example scene, in metres: direct sqrt(2.5^2 + 0.7^2) =
2.596151, floor sqrt(2.5^2 + 1.7^2) = 3.023243, ceiling sqrt(2.5^2 + 3.3^2) = 4.140048, the two
y walls sqrt(2.5^2 + 4^2 + 0.7^2) = 4.768648, -x sqrt(5.5^2 + 0.7^2) = 5.544367 and +x
sqrt(6.5^2 + 0.7^2) = 6.537584; times 48000 / 343 that is 363.310, 423.078, 579.365, 667.333
(twice), 775.888 and 914.881 frames. The tail's input waits for t_c = 5 \* 2.5 / 343 s = 1749
frames, so nothing but taps reaches the first 1749 frames of the response. The cabinet is a 1 cm
piston: the 4x12's directivity fit puts the ceiling image's peak, 53 degrees above the baffle's
axis, one frame late at 580, which is the filter's group delay and not the tap's placement.

### `RoomNodeCrossfadesAPublishWithoutAStep`

`// The arriving bank starts from zero after a 0.36 m move.`

The microphone moves 0.36 m, which reorders the taps and refits every filter, so the arriving
bank starts from zero. Measured through the seam's high half: a step ratio of 1.0033 and a
residual of 0.0051, -45.8 dBFS, where the taps alone measured 0.0020, -53.9 dBFS and the
two-shelf fit 1.0051 and 0.0233, -32.7 dBFS. It read -52.1 dBFS while the tail sat 12 dB under
its diffuse-field level and its output cascade was the room's alone; the cascade now follows
what the taps leave of the field, so a move steps its coefficients on the network's running
state, 3 % in the second section here. Spec 5.3 and the README's crossfade section carry the
figures.

### `RoomNodeCrossfadesASmallMoveFromTheCarriedState`

`// 78 of the 96 taps carry their state over one centimetre.`

One centimetre moves every delay by under 1.4 frames; 78 of the 96 taps refit within the match
bound and carry their state, the other 18 start from zero. Measured through the seam's high
half: a step ratio of 1.0000 and a residual of 0.0010, -59.6 dBFS, where the taps alone measured
-60.2 dBFS and the two-shelf fit carried 52 and measured 0.0160, -35.9 dBFS.

### `RoomNodeRepublishesTheSameSceneWithoutAResidual`

`// The same scene refits to the same taps: zero residual.`

The same scene refits to the same taps, every tap carries its state, and the arriving bank
renders the retiring bank's block sample for sample. Measured: a residual of exactly zero.

### `RoomNodeReclaimsARetiredSetOnlyAfterTheAudioThreadMovedOn`

`// The node never reads a slot pointer past its block.`

The node copies a set's taps into its own bank the block it first sees the set, and never reads
a slot pointer past that block, so a reclaim can free a retired set at any time without a read
from freed memory. test_rt_swap_slot.cpp proves the slot's own deferral; this case walks the
node through the same sequence and checks that the block after each reclaim still carries the
room.

`// The direct tap arrives 363 frames in.`

The direct tap arrives 363 frames in, so the room is silent for the first block and the energy
check starts at the second.

### `RoomNodeTakesTheDirectImageOutOfTheEarsBankAndLeavesTheMicrophones`

`// Tap 0 of a solved set is the direct image.`

Tap 0 of a solved set is the (0, 0, 0) image, the shortest path. For the ears the HRIR pair
renders that path, so the bank the node renders from carries reflections only and the direct tap
goes to the direct path; the microphone's direct sound is a tap and stays.

### `RoomNodeCompensatesAnEarsSetByItsRoundedPropagationDelay`

`// The ears' render moves earlier by round(d / c * fs).`

The ears' whole render moves earlier by round(d / c \* fs), 363 frames for the example scene's
363.31: tap 0 lands 0.31 frames past zero, every reflection keeps its spacing to it unless that
would put it under a frame, and the tail's input delay moves with them, floored at zero, since
the network's shortest line is latency the design cannot give back. A microphone set is left as
it was.

### `RoomNodeRingsAtTheGaragesEigenfrequencies`

`// The first six modes lie between 28.583 and 71.458 Hz.`

A unit impulse through the garage, 262144 frames at 48 kHz and a 262144-point transform, 0.183
Hz per bin. The first six modes under f_s = 278.18 Hz are (c / 2) sqrt((nx / 6)^2 + (ny / 4)^2 +
(nz / 2.5)^2) at c = 343: 28.583, 42.875, 51.529, 57.167, 68.600 and 71.458 Hz. The (0, 1, 1)
mode at 80.896 Hz bounds the search from above.

### `RoomNodeModeWidthsMatchTheirDamping`

`// A resonator's -3 dB width is 2.199 / T60 Hz.`

A resonator whose pole radius is r has a -3 dB width of (1 - r) fs / pi; with r = 10^(-3 / (T60
fs)) that is 3 ln(10) / (pi T60) = 2.199 / T60 Hz. The solver's decays give the widths the
spectrum must show within 20 %: 1.485, 0.821, 1.519, 1.485 and 0.842 Hz for the first five
modes. The sixth, (2, 1, 0) at 71.458 Hz, sits 2.86 Hz above (0, 0, 1) with widths of 1.52 and
0.84 Hz, so their -3 dB contours run into each other and the spectrum reads 3.65 Hz for the
pair; it is left out here.

### `RoomNodePressesTheBassIntoTheCorner`

`// Every mode with an odd index has a node at the centre.`

The listener stands in the corner at (0.15, 0.15, 0.15) and then at the centre (3.0, 2.0, 1.25),
where every mode with an odd index is at a node. The ears carry the taps and the tail with them,
and what those pass under 100 Hz through the LR4 pair, two octaves under f_s, moves the figure
by hundredths of a decibel: 14.06 dB with the geometric receiver held still, 14.02 dB with it in
the corner too.

### `RoomNodeIsContinuousAcrossTheSchroederFrequency`

`// Both layers carry DiffuseField's level at f_s.`

Spec 9a.4 and 9a.6: the two layers meet at f_s when each carries the level the room's diffuse
field holds there, DiffuseField, the one reference ModalSolver scales its octave to and
LateReverbDesign hands the tail the remainder of. The modal octave at one pose is that level
times the pose's mode-shape factor, which the rays have no counterpart for and which reads under
the volume average at interior poses, so the modal octave is held against the design's own
figure at the pose and the geometric octave against the reference through the high half, both
averaged over six poses as energy and each within 2 dB, in all three reference rooms. The raw
step between the octaves is recorded; it is the reference's own change across the seam, the
4x12's directivity index rising 4.6 dB between the 250 and 500 Hz bands, less the interior
deficit, and no bound of physics. Measured 2026-09-09 before the shared reference: 7.7 dB in the
garage at one pose.

### `RoomNodeSwitchesTheWaveLayerOffInADeadRoom`

`// f_s under 40 Hz switches the modal layer off.`

f_s under 40 Hz gives a design with count zero and a crossover of zero, so the node's output is
the geometric bricks alone: the early renderer's channel 0 plus the tail, sample for sample, on
both outputs, with the crossovers bypassed.

### `RoomNodePublishesNewModalGainsWithoutRebuildingAResonator`

`// A moved cabinet or listener rebuilds no resonator.`

Spec 9a.2: a moved cabinet changes psi_n at one point, a moved listener at the two ears of an
ears set, and no resonator changes frequency or damping. A dimension change rebuilds every
section.

`// 161 of the 187 modes have a non-zero x index.`

Every mode with a non-zero x index feels the cabinet's move along x, 161 of the 187; every mode
with a non-zero y index feels the listener's move along y, 150 of them.

### `RoomNodeCarriesTheStandingWavesAcrossAMovedCabinet`

`// The bank keeps its state; only the source gains ramp.`

The cabinet moves 0.2 m along x. The resonators are the same, so the bank keeps its state and
only the source gains ramp: no step, and the four blocks after the fade hold the arriving pose's
steady energy. Measured: a step ratio of 1.0000 and 0.02 dB. The residual against the blend of
the two steady renders is -29 dB and is not bounded: the resonators hold the old drive and take
their decay time to settle to the new one, as the room's own standing waves would after the
cabinet is moved.

### `RoomNodeRampsTheEarGainsAsTheOutputBlendWould`

`// The bank's output is linear in the ear gains.`

The listener moves 0.3 m along y. The resonators are the same, so the ear gains ramp, and the
bank's output is linear in those gains: ramping them under the raised cosine is the blend of the
two steady renders, sample for sample, -144 dB when nothing else moved. The same listener
carries the taps, the direct path and the tail with it, and what they pass of a 68.6 Hz tone
through the LR4 pair, two octaves under f_s, crossfades from cleared banks and rings from their
zero state after the fade: the geometric leakage's own residual. It measured -95.3 dB when the
ears heard channel 0 at unity, -88.3 dB through the HRIR pair and the decoder, and measures
-72.3 dB with the tail at its diffuse-field level, 12 dB up, its output cascade refitted for the
moved listener's taps on the network's running state; the bound follows that measurement and no
other. The step ratio reads 1 + 6e-8, float rounding of an exact blend.

### `RoomNodeRebuildsTheStandingWavesInAChangedRoom`

`// A changed room starts the spare bank from zero.`

A room 0.1 m wider designs every resonator anew, so the spare bank starts from zero under the
fade and the field takes its decay time to build: the four blocks after the fade hold -16.8 dB
of the steady energy, bounded here at a quarter.

### `RoomNodeReportsTheCrossoverTheSetSplitsAt`

`// The node reports the crossover its seam sits at.`

The garage's 187 modes fit, so the report is Eyring's 278.18 Hz as the set carries it. The 3.0 x
2.5 x 2.4 m concrete room has about 1250 modes under its Eyring figure of 877 Hz, so the solver
lowers the crossover to the 513th mode and the node reports that lowered figure, the one its
seam sits at. The dead room reports Eyring's 22 Hz with the layer off, so a panel still shows
the room's own figure.

### `RoomNodeMicrophoneListenerFillsBothChannelsIdentically`

`// The microphone's two outputs are equal bit for bit.`

The microphone is one point: its taps encode to channel 0 alone, its tail is one signal read
twice, and its standing waves are read at the microphone in both ear gains, so the two outputs
are equal bit for bit. The listener stands elsewhere, and a moved listener mid-render changes
nothing the microphone hears.

### `RoomNodeMicrophoneAtFiveCentimetresLiftsTheBottomEnd`

`// The tilt is expected at 13.68 dB against a point source.`

The cabinet is `point_cabinet()`, one 1 cm cone that never breaks up, so the seven elements of
its line of sight stand within a centimetre and the capsule hears a point. A 4x12's cones are
20 cm from a capsule at the baffle's centre, and its tilt there is under half this figure.

Spec 8: a cardioid's gradient half gains sqrt(1 + (c / (2 pi f r))^2) on a spherical wave, 14.2
dB at 125 Hz for r = 5 cm and 0.1 dB at r = 3 m. The whole chain carries 1 / r as well, 35.6 dB
between the two distances, so the figure that survives is the tilt: the 125 Hz third octave
against the 4 kHz third octave, where the boost is 0.16 dB at 5 cm and nil at 3 m, and air takes
0.08 dB more from 4 kHz over 3 m. The dead room keeps the modal layer off, so the band reaches
the taps unsplit, and the window holds the direct path alone. Expected 13.68 dB; measured 13.34
dB. The plan's 15.5 dB at 100 Hz is the model's own figure and does not reach the output: the
six-band fit is anchored at 125 Hz and its low shelf holds level below it, so the rendered 100
Hz third octave reads 13.79 dB, recorded here and not bounded.

### `RoomNodeMicrophoneHearsLessRoomThanTheEars`

`// The two ratios differ by at least 11.5 dB.`

The garage with the cabinet at (1.5, 2.0, 0.5). A cardioid 5 cm off the cone hears the direct
path at 1 / 0.05 and the nearest reflection, off the floor, at 1 / 1.0 and 140 frames later; the
ears at (3.5, 2.0, 1.2), 2.12 m away, hear the direct path at 1 / 2.12 through the HRIR pair at
the alignment delay, the propagation compensated, and the floor at 1 / 2.63, 71 frames later.
The capsule stands on the baffle's axis, between the four cones, so its direct sound comes from
cones 21 cm away and not from a point 5 cm away.

`// Between the four cones the difference measures 12.20 dB.`

12.20 dB with the cones in place. A single point at the baffle's centre would read 19.95 dB.

### `RoomNodeCarriesOneOverROnTheCabinetsAxis`

`// An omni capsule on a point source's axis reads 1 / r.`

The cabinet is `point_cabinet()`. A 4x12's impulse energy on the baffle's axis falls short of
1 / r at these distances, because above a few kilohertz its 28 elements arrive apart.

The one level in the module that comes from geometry rather than from DiffuseField: an omni
capsule on the cabinet's axis in the dead room reads 1 / r, so its direct window is 0 dB at 1 m
and falls 6.02 dB per doubling. Measured -0.30, -6.64 and -13.32 dB against 0, -6.02 and -12.04;
the shortfall is the air's, 1.3 dB over 4 m at 4 kHz. A test that scales both layers to one
reference cannot see that reference move, so this is the case that holds the module's absolute
level down.

### `RoomNodeMovingAroundTheCabinetChangesTheTone`

`// The listener faces the cabinet at both spots.`

Spec 7: the 4x12's four pistons beam at 2 kHz and are omnidirectional at 125 Hz. The ears 2 m
from the cabinet, on axis and then 60 degrees off it, in the dead room's direct-path window: the
2 kHz third octave drops by more than 6 dB and the 125 Hz third octave moves by under 1 dB, each
within 1 dB of the piston model's own band value. The listener faces the cabinet at both spots,
so the head's response is its front response both times and the difference between the two
renders is the cabinet's alone.

### `RoomNodeEarsHearTheDirectSoundThroughTheHrirPair`

`// The floor image follows at frame 1551.`

In the dead room, whose modal layer is off, the ears 2 m behind the cabinet hear nothing before
the alignment delay and then, over the direct window, the direct path alone: tap 0 read at 128 +
(279.88 - 280) frames through the renderer's kernel, its fitted filter and 1 / r, then the
spherical head's response for a source in the median plane, the same in both ears. The floor
image, 12.17 m long, follows at 1703 - 280 + 128 = 1551.

### `RoomNodeDefaultsDirectDelayToCompensated`

`// Physical timing adds round(d / c * fs) frames.`

A fresh node reports direct_delay as compensated and one decoder partition of latency; physical
adds round(d / c \* fs). The plan's 280 frames assume the listener 2 m from the cabinet, which
the dead room places; the default scene's listener stands 2.596 m away, 363.31 frames, so a
fresh node reports 363 more when physical. The microphone renders at physical timing with no
partition in its path and reports 0 either way.

### `RoomNodeDirectOnsetMovesWithTheDirectDelayChoice`

`// The choice shifts the whole render by 280 frames.`

The rendered direct-path onset, the first frame above 1e-4 of the direct window's peak, sits 280
frames later in physical mode than in compensated mode in the dead room, and the floor image's
onset, after the direct window, moves by the same 280: the choice shifts the whole render and
not the direct sound alone. The index path stores the choice for the next publish, as the
listener's does.

### `RoomNodeReproducesWoodworthsInterauralDelayOnTheFallback`

`// Woodworth's delay is (a / c)(theta + sin theta).`

Spec 9.5: a source 2 m away at 30, 60 and 90 degrees azimuth, the listener facing -x in the dead
room; the lag of the interaural cross-correlation peak over the direct window is Woodworth's (a
/ c)(theta + sin theta), 0.2611, 0.4881 and 0.6558 ms for a = 8.75 cm and c = 343 m/s, within 20
microseconds, one sample at 48 kHz. The grid holds those three azimuths, so the pair is not
interpolated and the figure is the model's own. The window closes at 900 frames, before the
nearest image, off the +x wall at 90 degrees, lands at 1154 - 280 + 128 = 1002. The cabinet
faces the listener, so the direct tap's filter is the on-axis response: 60 degrees off the axis
its 2 kHz notch weighted the correlation toward the low frequencies, where a rigid sphere's
delay runs half again Woodworth's (Kuhn, JASA 62(1), 1977), and the lag read 0.58 ms.

### `RoomNodeReportsSphericalHeadWithNoAssetAndMeasuredWithOne`

`// An empty HRTF set restores the spherical head.`

hrtf_source() is SphericalHead after prepare(); the six-direction fixture installed through
set_hrtf makes it Measured, the node keeps rendering finite audio through the fixture's pair and
decoder, and an empty set restores the fallback.

### `RoomNodeTailIsCoherentLowAndIncoherentHigh`

`// A window 1024 frames after t_c holds the tail alone.`

Spec 6.5: the interaural cross-correlation of the tail alone, measured on the ear outputs, is
above 0.8 in the 125 Hz octave and below 0.3 in the 4 kHz octave. The dead room keeps the modal
layer off and its taps end at t_c = 8397 frames, compensated by 280 and delayed by the
partition, so a window opening 1024 frames after that holds the tail alone; the room's 0.21 s
RT60 has decayed 97 dB by the window's end and the correlation weights the loud start.

### `RoomNodeDecoderMagnitudeMatchesTheMeasuredSet`

`// The case runs only with the SADIE II set present.`

Spec 9.5 as measured, only with the SADIE II set present, 24 directions at -30, 0, +30 degrees:
near ear 3 dB below 5 kHz and 6 dB above, far ear 10 (assets/hrtf/README.md).

### `RoomNodeCarriesTheRearNotchWithAMeasuredSet`

`// The rear notch is within 1 dB of the set's own.`

Spec 9.5 as measured: the ears facing away from the cabinet, the source at 180 degrees, hear the
left ear's rear notch within 1 dB of the set's own (assets/hrtf/README.md).

## `test_room_records.cpp`

### `RoomRecordsAreSmallEnoughToPublishPerGuiFrame`

`// One EarlyTap is 152 bytes and one Mode is 56.`

One EarlyTap is 4 + 4 + 40 + 40 + 64 = 152 bytes, a multiple of the double's alignment, so a
128-tap set is 19456 plus 12 of header, padded to 19472; one Mode is 40 + 4 + 8 = 52 padded to
56, so 512 of them are 28672 plus 12, padded to 28688.

## `test_room_render_set.cpp`

### `BuildRenderSetFillsEveryLayerForTheGarage`

`// The garage keeps 96 of the 129 order-4 images.`

The early set is the solver's own at order 4: 129 images, of which the garage keeps the 96 that
arrive before t_c = 5 \* 2.5 / 343 s, the 12.5 m the shortest dimension allows.

### `BuildRenderSetPutsTheDirectPathAtItsPropagationDelay`

`// 2.596151 m is 363 frames and a gain of 0.385186.`

The source (1.5, 2.0, 0.5) and the listener (4.0, 2.0, 1.2) are sqrt(2.5^2 + 0.7^2) = sqrt(6.74)
= 2.596151 m apart: round(2.596151 / 343 \* 48000) = round(363.31) = 363 frames and 1 / 2.596151
= 0.385186.

### `BuildRenderSetPutsTheMicrophonesDirectPathAtTheMicrophone`

`// 0.1 m is 14 frames and a gain of 10.`

The microphone sits 0.1 m from the source: round(0.1 / 343 \* 48000) = round(13.99) = 14 frames
and a gain of 10.

### `BuildRenderSetTakesTheDirectPairFromTheHrtfSet`

`// The interaural delay is 0.0006558 s, 31.48 frames.`

A source one metre to the listener's left, at ear height, sits at 90 degrees azimuth on the
spherical head: Woodworth's (a / c) (theta + sin theta) with a = 0.0875 m and theta = pi / 2 is
0.0006558 s, 31.48 frames, and the right ear is the later one.

### `BuildRenderSetReadsTheStandingWavesAtTheSetsReceiver`

`// The modal field is read at the set's receiver.`

The modal field is read where the set's geometric receiver stands: a microphone set carries psi
at microphone_position in both ear gains, an ears set the solver's two ears 8.5 cm to either
side of listener_position, so a listener at (5.4, 3.4, 2.0) facing +x reads psi at y = 3.485 and
y = 3.315.

## `test_room_scene.cpp`

### `RoomSceneWeightsAPatchByItsArea`

`// 92 m2 of concrete and 6 m2 of doorway over 98 m2.`

92 m2 of unpainted concrete and 6 m2 of open doorway over 98 m2: 125 Hz (92 \* 0.01 + 6 \* 1.0)
/ 98 = 0.070612; 500 Hz (92 \* 0.02 + 6 \* 1.0) / 98 = 0.080000.

### `RoomSceneComputesTheBedroomsWeightedAbsorption`

`// At 500 Hz the bedroom's mean absorption is 0.071716.`

14 m2 carpet, 51.5 m2 gypsum, 1.5 m2 glass over 67 m2: 500 Hz (14 \* 0.14 + 51.5 \* 0.05 + 1.5
\* 0.18) / 67 = 0.071716.

### `RoomSceneAcceptsEveryReferenceRoom`

`// The default scene clears its floor by a factor of 8.8.`

The default scene is 6.0 x 4.0 x 2.5 m of gypsum board on studs over a carpeted floor: 74 m2 of
board and 24 m2 of carpet over 98 m2 give (74 \* 0.29 + 24 \* 0.02) / 98 = 0.223878 at 125 Hz
and (74 \* 0.05 + 24 \* 0.14) / 98 = 0.072041 at 500 Hz, its least absorbent band, which clears
the 0.008181 floor this shoebox needs by a factor of 8.8.

`// The dead room reads 0.673 against a floor of 0.026477.`

The 12 m carpeted room with four open faces the node tests render, whose absorption runs 0.673
at 125 Hz against a floor of 0.026477.

### `RoomSceneRefusesARoomThatRingsPastTheModulesDecay`

`// The concrete cube rings 32.04 s against the 12 s bound.`

The 12 m cube of unpainted concrete: 0.010000 at 125 Hz against a floor of 0.026477, Eyring's
32.04 s from its surfaces alone against the 12 s bound.

`// The bound is on every band, not on the mid one.`

The bound is on every band, not on the mid one: a 10 m cube of gypsum board on studs is dead at
125 Hz, 0.29, and rings 6.57 s at 1 kHz, so a room whose bass is fine and whose mid is not stays
valid while the same shell in concrete does not.

`// A bare concrete cube crosses the bound at 4.495 m.`

A bare unpainted concrete cube crosses the bound at an edge of 4.495 m, where 0.161 L / 6 over
-ln(1 - 0.01) reaches 12 s; 4.4 m gives 11.75 s and 4.6 m gives 12.28 s.

`// The floor rises with the room's volume over its area.`

Absorption alone does not decide it: the same concrete at 6.0 x 4.0 x 2.5 m passes, since the
floor rises with the room's volume over its area.

## `test_rt_graph_slot.cpp`

### `CanaryNode`

`/// @brief Declares a node whose virtual ports() returns a fixed, checkable value.`

A minimal node whose ports() is a virtual call returning a fixed, checkable value. Poking it
after the owning RigGraph has (incorrectly) been freed exercises the object's vtable pointer --
freed memory that's since been reused for something else will very likely make this crash
outright (invalid/foreign vtable) rather than just quietly return stale data, which is a much
stronger use-after-free signal than a bare null-check on the RigGraph\*.

### `RtGraphSlotConcurrentPublishAndAcquireDoesNotRace`

`// A virtual call catches a dangling graph pointer.`

Actually dereference the returned graph, through a virtual call, rather than just null-checking
the pointer -- a dangling-but-non-null RigGraph\* sails straight through a null-check. Every
published graph here carries exactly one CanaryNode, so nodes() must come back with exactly one
handle, and that node's ports() (a virtual call) must always report { 1, 1 }. If
acquire_for_audio_thread() ever hands back a pointer to a graph reclaim() already freed, this
either crashes outright (the vtable slot now belongs to whatever reused the memory) or the
checks below fail.

## `test_rt_safety.cpp`

### `RigGraphStereoProcessPerformsNoHeapAllocation`

`// The graph covers every path of the edge adaptation.`

Exercises every arithmetic path of the edge adaptation: a stereo producer summed into a mono
consumer, a fan-in summed per channel, and a mono sink widened to a stereo host.

### `TubeScreamerProcessPerformsNoHeapAllocation`

`// A moved knob re-derives the matrices inside process().`

Moving a knob before every block is the path the plain loop misses: a wiper that has changed
marks the block dirty and update_matrices_if_dirty() re-derives inside process(), which is where
a solver would allocate if it were going to. This is the D6c instance of the trap D6b's
DkCircuitPotentiometerUpdatePerformsNoHeapAllocation established, now on a shipping node rather
than on a bare circuit. Both knobs sweep their full travel so every derivation sees a genuinely
new matrix.

### `Jcm800ProcessPerformsNoHeapAllocationWithoutCabinet`

`// The per-block set_high_shelf() is pure arithmetic.`

This is also the trap for Jcm800::process()'s per-block set_high_shelf() call: recomputing the
presence shelf's RBJ coefficients every block looks expensive, but it is pure double arithmetic
(pow/sin/cos/sqrt) with no heap, lock, syscall, or throw behind it, so it must not register
here.

### `CabinetNodeProcessPerformsNoHeapAllocationWithCabinet`

`// 500 blocks wrap the spectrum history at least once.`

Cabinet convolution is now sushidsp.cabinet's own surface, not any amp's: it runs
StreamingConvolution's FFT, spectrum history, and overlap-add buffers, all of which must have
been sized in prepare(). 500 blocks is more than the impulse response's partition count, so the
circular spectrum history wraps at least once under the trap.

### `CabinetNodeSetCabinetSwapsWithoutAllocatingOnTheAudioThread`

`// The trap is armed around the blocks, not set_cabinet().`

A cabinet published from the control thread must reach process() through RtSwapSlot's single
atomic acquire - no allocation, no lock. Only the audio thread's half of a swap has to be
RT-safe (publish() builds the convolver and is expected to allocate), so the trap is armed
around the processing blocks and disarmed across each set_cabinet(). That way every trapped
block is one that picks up a freshly published cabinet, which is the case the name promises - a
version of this test that published everything up front would only be proving that convolving
allocates nothing.

### `File scope`

`// The netlist lives in tests/circuits/jcm_v1a_netlist.hpp.`

The netlist, its component constants and the triode glue live in
tests/circuits/jcm_v1a_netlist.hpp, shared with the D6b stage test and the benchmark.

### `DkCircuitPotentiometerUpdatePerformsNoHeapAllocation`

`// A pot move rebuilds the DK matrices on the audio thread.`

The one audio-thread path that re-runs a derivation (plan Ruling 3): a pot move invalidates the
DK matrices and update_matrices_if_dirty() rebuilds them at the top of the next block. If
anything in that rebuild allocated, Ruling 3 would have to be replaced by a control- thread
rebuild plus an RtSwapSlot hand-off, so this case is the one that decides it.

### `ResamplerProcessPerformsNoHeapAllocation`

`// At the slowest clock interpolate() touches 164 taps.`

The DD-3's decimating leg at the VCO's slowest clock, which is where the Resampler is widest:
the prototype's half width of 34 (input samples; true crossings are fewer at fc = 0.45)
stretches to 34 / (20000 / 48000) = 81.6, so interpolate() runs two loops of 82 and touches 164
taps per output, and prepare() sizes the history ring for exactly that.

### `ResamplerRateChangePerformsNoHeapAllocation`

`// One 256-frame block calls set_rates 32 times.`

The case plan Ruling 3 rests on. The DD-3 re-clocks every 8 frames, so one 256-frame block calls
set_rates 32 times; a ratio change that rebuilt a filter bank would put the node off the audio
thread entirely. The sweep runs the VCO's whole 20-82.5 kHz range, so the ratio reaches both
ends of the clamp prepare() was given.

### `Dd3DelayProcessPerformsNoHeapAllocation`

`// Both MODE changes below move N and pay the std::fill.`

Two things happen inside this trap window that no other node does. process() drives both
Resamplers' set_rates 32 times per block through the 8-frame clock grid, and a MODE change
clears the ring (plan Ruling 14) - once through set_parameter by index, which only records the
mode so that process() applies it at the next block start, and once through set_mode, which
applies it on the spot. Middle holds 4096 samples and Long 16384, so both changes below move N
and both pay the std::fill.

### `TrackRecorderWritePerformsNoHeapAllocation`

`// The trap sees the ring copy and the drop counter.`

The one capture path the audio thread touches. arm() allocates the rings and starts the writer
thread before the trap opens, and the first block is written and drained outside it so that
WavWriter's stream buffer already exists: what the trap then sees is the ring copy and, once the
ring fills, the drop counter.

### `TrackPlayerProcessPerformsNoHeapAllocation`

`// Timelines and converters exist before the trap opens.`

Every timeline is published and every converter sized before the trap window opens, so what is
measured is the mix itself: eight slot acquisitions, eight lanes of sixteen clips walked, the
pan trigonometry, a loop range that splits each block into two runs, and both the unity copy
path and the converted one.

### `TrackPlayerTimelineSwapPerformsNoHeapAllocationOnTheAudioThread`

`// The trap covers the pick-up of a changed lane.`

Every replacement timeline is built before the run, and the window is opened only around
process(), so what is measured is the audio thread picking up a lane that changed under it: one
acquire of a new generation and a converter that has to restart on a clip it was not inside a
block ago.

### `VariRateReaderReadPerformsNoHeapAllocation`

`// The converters are sized before the trap opens.`

The converters are sized before the trap window opens, so what is measured is the read itself: a
set_rates() on every speed change and one push per take frame consumed.

### `MetronomeProcessPerformsNoHeapAllocation`

`// The bursts are built before the trap opens.`

The bursts are built before the trap window opens, so what is measured is the block: two atomic
loads, a beat index and a copy out of a vector already sized.

### `RoomNodeProcessPerformsNoHeapAllocation`

`// The first trapped block adopts the published set.`

prepare() builds the spherical head and publishes the first set outside the trap; the first
trapped block is the one that adopts it, a copy into the node's own bank.

### `RoomNodeProcessPerformsNoHeapAllocationAcrossAPublish`

`// Only the audio thread's half of a publish is trapped.`

Only the audio thread's half of a publish is trapped: set_scene solves and allocates on the
control thread, so the trap closes around it and opens around the blocks that pick the new set
up, start the crossfade and run it to its end. The node renders the microphone, and the four
publishes walk the modal bank through each of its paths: a moved microphone ramps the gains read
at it, a moved listener changes nothing the microphone hears, a changed dimension blends the
spare bank in from zero.

### `ModalBankProcessPerformsNoHeapAllocation`

`// All K_MAX_MODES resonators run at 68.6 Hz, 1 s decay.`

Every one of K_MAX_MODES resonators runs, at the garage's 68.6 Hz mode with a 1 s decay: r =
10^(-3 / 48000) = 0.99985610, a1 = -2 r cos(2 pi 68.6 / 48000), a2 = r^2.

### `CrossoverPairProcessPerformsNoHeapAllocation`

`// set_crossover is arithmetic on the audio thread.`

The pair is placed, run, moved and bypassed inside the trap, as the node does at every adopt;
set_crossover is arithmetic on the audio thread and must stay so.

### `LateReverbProcessPerformsNoHeapAllocation`

`// The trapped blocks include the 512-frame crossfade.`

The second design moves every line by 37 frames, so the trapped blocks include the 512-frame
length crossfade as well as the steady network.

### `BinauralDecoderProcessPerformsNoHeapAllocation`

`// Sixteen blocks cross the 128-frame partition 32 times.`

prepare() builds the 32 partitioned convolvers from the spherical head's decoder outside the
trap; sixteen blocks of 256 frames, which cross the 128-frame partition thirty-two times, run
inside it.

### `RoomNodeBinauralProcessPerformsNoHeapAllocation`

`// The ears render every binaural path inside the trap.`

The ears render every binaural path: the direct pair, the decoder, the diffuse pair and the
alignment delays. set_scene, the direct_delay switch and the fallback's re-install each publish
on the control thread outside the trap; the blocks that adopt each set, fade it in and decode it
run inside, and the index path stores the delay choice alone.

## `test_rt_swap_slot.cpp`

### `Counted`

`/// @brief Declares a payload whose destructor is observable through a live count.`

A payload whose destructor is observable, so the deferred-reclaim contract can be checked by
counting frees rather than by inspecting RtSwapSlot's internals. The canary value is
dereferenced (not just null-checked) by the concurrent case: a dangling-but-non-null pointer
sails straight through a null-check, whereas reading a field of freed memory that has since been
reused shows up as a wrong value or a crash.

### `RtSwapSlotReclaimDestroysRetiredObjectsOnlyOnceAudioThreadHasMovedPast`

`// Generation 1 stays alive while the thread is on it.`

The audio thread is still parked on generation 1, so generation 1's object must stay alive no
matter how often reclaim() runs.

### `RtSwapSlotActiveForPublisherDoesNotAdvanceTheAudioThreadWatermark`

`// Reading the active object must not move the watermark.`

active_for_publisher() exists so the publishing thread can look at the active object without
claiming the audio thread reached it. Reading it must therefore leave reclaim()'s "provably
moved past" bound exactly where it was - if it forged the watermark instead, reclaim() would
free the object an audio thread is still using.

`// The read alone moves nothing.`

The audio thread's last recorded generation is still the second acquire above, so the first
object is genuinely retired here. Re-read it through the publisher accessor and confirm that
read alone did not move anything.

`// Only object 1 may be freed here.`

The audio thread has not acquired since generation 2 was published, so reclaim() may free
nothing published at or after it: object 2 (still the generation the audio thread last recorded)
and object 3 (active) must both survive. Only object 1 may go.

`// Only acquire_for_audio_thread() moves the watermark.`

Once the audio thread does acquire, the earlier object becomes reclaimable - proving the
watermark was still being driven by acquire_for_audio_thread() alone.

## `test_silicon_diode.cpp`

### `SiliconDiodeMatchesShockleyAtThreeOperatingPoints`

`// 1N914: Is = 2.52e-9 A, n = 1.752, Vt = 0.025864 V.`

1N914: Is = 2.52e-9 A, n = 1.752, Vt = 0.025864 V, so n\*Vt = 0.045313728 V exactly.

```text
v = 0.4 V : v/(nVt) = 8.82734...  exp = 6.81817404536e3
            i  = 2.52e-9 * (6818.17404536 - 1)      = 1.71792785943e-5 A
            di = (2.52e-9 / 0.045313728) * 6818.174 = 3.79174244818e-4 S
v = 0.6 V : v/(nVt) = 13.2410...  exp = 5.6299187171e5
            i  = 2.52e-9 * (562991.87171 - 1)       = 1.41873699671e-3 A
            di = (2.52e-9 / 0.045313728) * 562991.9 = 3.13092649696e-2 S
v = 0.7 V : v/(nVt) = 15.4479...  exp = 5.11586582342e6
            i  = 2.52e-9 * (5115865.82342 - 1)      = 1.2891979355e-2 A
            di = (2.52e-9 / 0.045313728) * 5115866  = 2.84504993167e-1 S
```

### `SiliconDiodeReverseCurrentApproachesMinusSaturationCurrent`

`// At v = -0.5 V the current is -2.51995932614e-9 A.`

v = -0.5 V: exp(-11.0342) = 1.61404223972e-5, i = 2.52e-9 \* (1.614e-5 - 1)

```text
= -2.51995932614e-9 A, di = (2.52e-9/0.045313728)*1.614e-5 = 8.97605786067e-13 S
```

### `SiliconDiodeStaysFiniteAndCOneAcrossTheLinearExtrapolation`

`// The knee sits at v_max = 40 * n * Vt = 1.81254912 V.`

The extrapolation knee sits at v_max = 40 \* n \* Vt = 1.81254912 V; the derivative must be
continuous across it. knee_jacobian_ itself is \~1.309e10 S (Is/(nVt) \* exp(40)), so a fixed
absolute tolerance of 1e-3/1e-9 is unsatisfiable at this magnitude for any correct
implementation: measured directly, jacobian(knee - 1e-7) and jacobian(knee + 1e-7) differ by
\~2.89e4 absolute (\~2.2e-6 relative) purely from the Shockley curve's real slope this close to
exp(40), a 1e-4 relative tolerance (\~1.31e6 absolute) left over 45x slack versus that measured
gap, so this uses 1e-5 relative (\~1.31e5 absolute, \~4.5x the measured gap) instead. The
extrapolation identity below only holds to double's rounding: measured directly, the two sides
differ by \~4e-16 relative (\~1 ULP at this magnitude), so this uses 1e-12 relative, \~2500x
the measured gap, still \~1e3x tighter than the 1e-9 absolute tolerance originally in the brief
once rescaled to this \~7e9 magnitude.

### `SiliconDiodeAntiparallelPairMatchesTwoTimesSaturationCurrentTimesSinh`

`// The pair's total current is 2 Is sinh(w / nVt).`

The D6c clip stage's nonlinear port is v[0] = w, v[1] = -w across one pair, so the pair's total
current is Is(e^(w/nVt) - 1) - Is(e^(-w/nVt) - 1) = 2 Is sinh(w / nVt) and its conductance is
jacobian(w) + jacobian(-w). n\*Vt = 1.752 \* 0.025864 = 0.045313728 V, so 1/(n\*Vt) =
22.068367449. Values computed at double precision in the D6c plan:

```text
w = 0.2  : i = 2.080515143e-7 A,  di/dw = 4.592704262e-6 S
w = 0.3  : i = 1.890822805e-6 A,  di/dw = 4.172752069e-5 S
w = 0.38 : i = 1.105058316e-5 A,  di/dw = 2.438683551e-4 S
w = 0.4  : i = 1.718179822e-5 A,  di/dw = 3.791742530e-4 S
w = 0.5  : i = 1.561297429e-4 A,  di/dw = 3.445528538e-3 S
```

### `SiliconDiodeHonoursANonDefaultIdealityFactor`

`// n = 1 makes n*Vt = 0.025864 V.`

n = 1 makes n\*Vt = 0.025864; at v = 0.6 the exponent is 23.1983..., exp = 1.19426e10, so the
current is far larger than the 1N914's at the same voltage.

### `SiliconDiodeAntiderivativeDifferentiatesBackToTheDiodeCurrent`

`// The antiderivative's slope is the diode current.`

The whole point of the antiderivative is that its slope is the current, so a centred difference
of it has to return the current the model itself reports. The sweep crosses both knees (+-1.8125
V for the defaults), so it covers the Shockley branch and both quadratic extrapolations rather
than only the range a Tube Screamer visits.

### `SiliconDiodeAntiderivativeHasNoStepAtEitherKnee`

`// The bound is the largest slope times the width.`

A step in the antiderivative would put a spike straight into the divided difference
antiderivative antialiasing takes, so neither knee may leave one. The bound is the largest slope
on the interval times its width, which is what differentiability across the knee means.

### `SiliconDiodeJointAntiderivativeFormAgreesWithTheSeparateEntryPoints`

`// The joint form must match the two entry points.`

current_and_antiderivative() exists to share one exponential; it must not drift from the two
entry points that forward to it.

### `SiliconDiodeReverseExtrapolationContinuesTheShockleyLawRatherThanMirroringForward`

`// Below the lower knee the current stays at -Is.`

Below the lower knee the model continues the reverse-biased diode it actually is: a current that
stays at -Is and a conductance of (Is/(n\*Vt))\*exp(-40), some 34 orders of magnitude under the
forward tangent's. The two tangents are not each other's mirror image, and the earlier code that
made them one put a step from -2.5 nA to -593 MA right here (D6c C2).

## `test_spherical_head_hrtf.cpp`

### `SphericalHeadHrtfShadowsTheFarEarAboveOneKilohertz`

`// The far ear sits at the sphere's bright spot.`

With the ears on the axis, the far ear sits at the sphere's bright spot, so the level difference
at 90 degrees is the near ear's own rise: 4.7 dB in the 4 kHz octave, and it grows from octave
to octave.

### `SphericalHeadHrtfHearsTheSameDiffuseFieldAtBothEars`

`// The grid mirrors about the median plane.`

The ears sit on the sphere's axis and the grid mirrors about the median plane, so the two power
averages are the same sum in a different order.

### `SphericalHeadHrtfDiffuseFieldCarriesTheRigidSpheresDiffuseGain`

`// The sphere's mean diffuse power is 1.8472, +2.67 dB.`

By Legendre orthogonality the sphere's power averaged over every direction is sum_m (2m+1) /
(mu^4 |h_m'(mu)|^2): 1 at DC, 1.7765 at 4 kHz, rising to 2 at high mu. Its mean over 0 to 48 kHz
is 1.8472, +2.67 dB; the 10-degree grid weighted by solid angle gives 1.8477, and 128 taps hold
all of it to 1e-10.

## `test_take_library.cpp`

### `TakeLibraryHoldsATakeATimelineStillInTheRetireListNames`

`// A retired timeline still names the take.`

The first timeline is retired but the audio thread's watermark has not passed it, so both
timelines still name the take and the take must survive.

## `test_topology_applier.cpp`

### `TopologyApplierOpeningAPathCopiesTheMainPathWithItsParameters`

`// The opened path carries a copy of the main path.`

The main path is untouched and the path that opened carries a copy of it, the knob position
included.

## `test_track_player.cpp`

### `TrackPlayerPlaysAClipAtItsStartFrame`

`// The impulse sits at the clip's fourth frame.`

The impulse sits at the clip's fourth frame, so it lands three frames past the clip's start
frame in the block that holds it.

### `TrackPlayerFadesAClipEdge`

`// A clip cut inside its take carries the ramp.`

The clip starts 1000 frames into the take, so its head is a cut and carries the ramp; a clip
covering a whole take is cut at neither end and stays flat, which is what keeps the single-take
cases above sample for sample.

### `TrackPlayerAtHalfSpeedDropsAnOctave`

`// 24000 frames hold 220 cycles of 440 Hz.`

Half speed halves 880 Hz to 440 Hz; 24000 frames at 48 kHz hold 220 cycles of it, and a cycle
crosses zero twice.

### `TrackPlayerReclaimsLanesBeforeCollectingTheLibrary`

`// The first timeline is alive, so its take is held.`

The audio thread's watermark still names the generation the first timeline was published at, so
that timeline is alive and its take is not collectable.

## `test_triode_12ax7.cpp`

### `Triode12AX7PlateCurrentMatchesKorenAtThreeOperatingPoints`

`// The three points are hand-computed from Koren's law.`

mu = 100, kp = 600, kvb = 300, kg1 = 1060, kx = 1.4.

(1) Vpk = 250, Vgk = -1.5  (the classic 12AX7 quiescent point)

```text
root = sqrt(300 + 62500) = 250.599281722833
t    = 600 * (0.01 - 1.5 / 250.599281722833) = 2.40860901989570
s    = ln(1 + exp(t)) = 2.49473195572810,  E1 = 250 * s / 600 = 1.03947164822004
Ip   = 2 * 1.03947164822004^1.4 / 1060 = 1.99187388684622e-3 A  (1.99 mA, right for a 12AX7)
dIp/dVgk = 2.45547036171309e-3 S,  dIp/dVpk = 2.58169362128740e-5 S
(gm = 2.46 mA/V and rp = 1/2.58e-5 = 38.7 kOhm, both a few percent off the datasheet's
 1.6 mA/V / 62.5 kOhm here - Koren's fit runs hot at this point, a known property of the
 model rather than a transcription error.)
```

(2) Vpk = 100, Vgk = -2.0  (deep toward cutoff)

```text
root = sqrt(300 + 10000) = 101.488915650922,  t = 600 * (0.01 - 2 / root) = -5.82395133797152
s = 2.95154218089098e-3,  E1 = 100 * s / 600 = 4.91923696815163e-4
Ip = 2 * 4.91923696815163e-4^1.4 / 1060 = 4.40941496483551e-8 A
dIp/dVgk = 3.64418892389800e-7 S,  dIp/dVpk = 7.69341309293717e-9 S
```

(3) Vpk = 300, Vgk = 0  (grid at the cathode; the stage is fully on)

```text
root = sqrt(300 + 90000) = 300.499584026334,  t = 600 * 0.01 = 6 exactly
s = ln(1 + exp(6)) = 6.00247568513773,  E1 = 300 * s / 600 = 3.00123784256887
Ip = 2 * 3.00123784256887^1.4 / 1060 = 8.78910615164942e-3 A
dIp/dVgk = 4.08295444882820e-3 S,  dIp/dVpk = 4.10158287076973e-5 S
```

### `Triode12AX7GridCurrentFollowsNormansSoftLaw`

`// Ig = 1e-6 * ln(1 + exp((Vgk - 0.5)/0.15)).`

Ig = 1e-6 \* ln(1 + exp((Vgk - 0.5)/0.15)), dIg/dVgk = (1e-6/0.15) \* logistic(same argument).

```text
Vgk = -1.5 : argument = -13.3333333333333, ln(1+exp) = 1.61959548076714e-6
             Ig = 1.61959548076714e-12 A,  dIg = 1.07972944614873e-11 S (effectively cut off)
Vgk =  0.5 : argument = 0, ln(2) = 0.693147180559945
             Ig = 6.93147180559945e-7 A,  dIg = (1e-6/0.15) * 0.5 = 3.33333333333333e-6 S
Vgk =  1.0 : argument = 3.33333333333333, ln(1+exp) = 3.36838574941280
             Ig = 3.36838574941280e-6 A,  dIg = (1e-6/0.15) * 0.965554 = 6.43703202889193e-6 S
```

### `Triode12AX7IsCutOffAndCOneAtNonPositivePlateVoltage`

`// The piecewise definition is C1 at Vpk = 0.`

Approaching Vpk = 0 from above, plate current and both of its derivatives go to zero, so the
piecewise definition is C1 and not a slope discontinuity like the D5b clamp was.

### `Triode12AX7JointFormAgreesWithTheSeparateEntryPoints`

`// current() and jacobian() forward to the joint form.`

current() and jacobian() forward to the joint form, so these agree exactly by construction
today. Pinning that is the point: the three entry points cannot drift apart later, and a
current() specialised for speed would have to keep matching the form the solver calls.

## `test_triode_stage.cpp`

### `K_SOURCE_RESISTANCE`

`// These constants are the JCM800's V1a-shaped stage.`

The JCM800's V1a-shaped stage: a 10 kOhm source into a 1 MOhm grid leak, biased 1.5 V below the
cathode, gain 60, with a 194 / 115 V plate swing.

### `TriodeStageGridFollowsTheSourceDividerBelowTheCathode`

`// 3 V in is 2.9703 V of grid excursion.`

Below the cathode the Norman law's current is nanoamperes, so the grid sits on the Rg / (Rs +
Rg) divider of the input: 3 V in is 2.9703 V of excursion.

### `TriodeStageGridConductsOnTheNormanLaw`

`// The root is checked against an independent bisection.`

The solver's root is the root of the KCL the header states, checked against an independent
bisection at three inputs: below the 0.5 V knee, on it, and well past it, where the conducting
slope of 150 kOhm against the 10 kOhm source pulls the grid 0.45 V off the divider (8 V in:
7.554 V of excursion against 7.921 V linear).

### `TriodeStagePlateSwingBoundsBothSidesAsymmetrically`

`// The two sides differ by the swing alone.`

Cutoff side: 9.9 V of excursion is a linear plate of 594 V, and 194 \* tanh(594 / 194) is 193.15
V. Conduction side: the grid rises to 7.55 V of excursion, a linear plate of -453 V, and -115 \*
tanh(453 / 115) is -114.99 V. The two sides differ by the swing alone.

### `TriodeStageCutoffIsThePlateCeiling`

`// No solver fence holds the grid short of the ceiling.`

No solver fence holds the grid short of the ceiling: at -30 V in, the 5150's V5A stage reaches
its 90.742 V headroom to the supply, where a 4.136 V fence held it at 74.3 V
(docs/archive/superpowers/specs/2026-09-07-grid-knee-antialiasing-design.md section 3).

### `AntiderivativeAntialiaserInputMapInvertsTheGridSolve`

`// g(u) composed with the bisected solve is the identity.`

g(u) is the input the KCL says puts the grid at u, so composing it with the bisected solve of
that KCL returns the input: the map the divided difference divides by is the stage's own.

### `AntiderivativeAntialiaserAntiderivativeDifferentiatesBackToTheTransfer`

`// dG/du equals bound(-gain u) times dg/du everywhere.`

G is the antiderivative of the plate output with respect to the input, so dG/du equals
bound(-gain u) times dg/du at every excursion, across the knee and past both plate ceilings. The
bound is the table's cubic Hermite error, measured at 7e-9 of the swing.

### `AntiderivativeAntialiaserTransferMatchesThePlateBoundBetweenKnots`

`// Between knots only the Hermite slope error remains.`

G'(u) / g'(u) is what the ill-conditioned branch returns; between the table's knots it may
differ from bound(-gain u) only by the Hermite interpolant's slope error.

### `AntiderivativeAntialiaserDividedDifferenceIsTheMeanOfTheCompositeTransfer`

`// The divided difference of G is the mean transfer.`

Over an input interval the divided difference of G equals the mean of the composite transfer,
checked against Simpson's rule on the bisected transfer for intervals that sit inside the knee,
cross it, and cross both plate ceilings.

### `AntiderivativeAntialiaserIsContinuousWhereConsecutiveSamplesNearlyCoincide`

`// Near-equal samples fall back to the midpoint transfer.`

The divided difference loses its precision as g(u1) - g(u0) shrinks, so under
K_MINIMUM_INPUT_DIFFERENCE_VOLTS it gives way to the transfer at the midpoint. Two identical samples
and two a float ulp apart must return the plate bound, and sweeping the difference through the
threshold must leave no step larger than the float output's own quantisation.

### `AntiderivativeAntialiaserResetDropsItsPreviousSample`

`// The first sample after reset() is the plain transfer.`

Without a previous sample there is no interval to average over, so the first sample after
reset() is the plain transfer at that excursion; the stages lean on that to restart click-free.

### `TriodeStageOutputIsTheAntialiaserOfItsOwnGridSolve`

`// The stage's output is its antialiaser's output.`

The stage's plate output is its antialiaser fed with the grid excursion its solver found: the
same excursions through a bare antialiaser reproduce the stage sample for sample, to the Newton
solver's four-iteration tolerance.

## `test_ts_clip_stage.cpp`

### `DkTsClipBlockSmallSignalResponseMatchesItsClosedForm`

`// At 1e-4 V the diodes stay on zero-bias conductance.`

Driven far below the clipping threshold so the diodes stay on their zero-bias conductance, which
is the regime the closed form (and an LTspice .ac) describes. The pair's conductance grows as
cosh(w / nVt); holding the diode voltage w under 14 mV keeps it within 5 % of the zero-bias
value, and at the highest gain here (107.6) that means an input under 0.0143 / 106.6 = 1.34e-4
V. 1e-4 V is used for all three settings.

Closed-form values from the D6c plan, in dB:

```text
       100 Hz    1000 Hz
0.1   10.1534   25.1395
0.5   18.8134   34.1273
0.9   22.9649   38.2321
```

### `DkTsClipBlockClipsAtTheReferencePeakVoltage`

`// The diode pair, not Rf, sets the peak above threshold.`

The reference transient drives 100 mV at 1 kHz and peaks at 0.481683 / 0.490169 / 0.491758 V for
DRIVE = 0.1 / 0.5 / 0.9 - almost independent of drive, because the diode pair, not the feedback
resistor, is what sets the output above threshold. Back of envelope at DRIVE = 0.1: the peak
drives 0.1 / |4700 - j3386.28| = 17.26 uA into the ground leg, and the diode-pair voltage
carrying that through Rf in parallel with the pair is 0.3886 V, so the output is 0.1 + 0.3886 =
0.4886 V. This case is the legible failure the null test is not: a stamp sign error moves this
number by volts.

### `DkTsClipBlockNewtonConvergesOnNinetyNinePointNineNinePercentOfSamples`

`// The stimulus is a sweep and three bursts at 384 kHz.`

Spec's Newton acceptance, on this block's own material: a 2 s sweep 20 Hz -> 20 kHz at -12 dBFS
in volts (0.251 \* K_FULL_SCALE_VOLTS = 0.502 V), then 1 kHz bursts at 0.05 V, 0.5 V and 5 V, all
at 384 kHz with the drive pot wide open, which is the hardest setting.

The D6c plan instrumented this block at 2.7 Newton iterations per solve on average and 4 worst
case, but it did that on a 0.5 V stimulus, and the 5 V burst is harder than anything the plan
measured. One run before the session was interrupted saw the eight-iteration budget exhausted on
57 of 1 056 000 samples, 0.0054 %, inside the spec's 0.01 % but past the plan's "4 worst case".
That figure is unverified since; the deferred sd test run confirms or corrects it.

### `DkTsClipBlockAdaaPortAlmostDoublesTheNewtonMissRate`

`// The ADAA port measures 0.0105 % against 0.0054 %.`

AdaaDiodePairPort's currents_and_jacobian() is DiodePairPort's character for character, so the
residual Newton solves is the same function and the extra misses are not a harder solve: they
are a different state trajectory arriving at harder open-circuit voltages. The measured rate is
0.0105 % against the plain port's 0.0054 %, which is past the spec's 0.01 % gate. That miss is
recorded rather than accommodated - it is one of the two reasons the shipping node still holds a
DiodePairPort (spec 2026-08-26-aaa-audio-quality-design.md, Model corrections D6c C2).

### `run_clip_transient_null`

`/// @brief Nulls the clip block against ts_clip's LTspice transient through one port.`

Nulls the clip block against ts_clip's LTspice transient through one port.

Templated so the antialiased port is held against the same reference at the same tolerance.
Antiderivative antialiasing evaluates the pair current half a sample early inside the feedback
loop, which is a real change to the modelled circuit; the honest place to see what it costs is
this residual, not an argument (spec 2026-08-26-aaa-audio-quality-design.md, Model corrections
D6c C2).

`// One pre-roll pass matches LTspice's own start.`

tools/spice/export.py runs `tran 0 40m 20m 1u`: LTspice solves the DC operating point, runs 40
ms and saves the last 20 ms. This block sits at zero at DC - there is no supply rail - so a DK
block starting from zero state has LTspice's initial condition exactly, and the saved window is
20 whole cycles of the 1 kHz stimulus. Replaying it ONCE therefore reproduces LTspice's own
first 20 ms, and the second pass is the comparison window. Replaying it more converges this
block to the periodic steady state while the reference is still on its turn-on transient, and
the null gets worse - see the pre-roll table in the D6c plan. This is why jcm_v1a's eight passes
are wrong here: that block has a 360 V rail and a bias point.

Both signals are low-passed at 10 kHz by the same 4th-order Butterworth before comparison, so
the band-limiting cannot flatter either one; the top octave is where LTspice's variable-step
integrator and our fixed-step trapezoidal rule are known to disagree (spec Risks). Computed
while writing the D6c plan with exactly this procedure: -80.3 / -73.7 / -76.0 dB.

### `DkTsClipBlockAdaaPortFallsShortOfTheLtspiceTransientReferenceByItsOwnBandLimiting`

`// The bound is a regression floor, not an acceptance.`

This case does NOT meet the spec's -60 dB null, and its name says so. The reference grid is 1
MHz, where a clipped corner has next to no aliasing left to remove, so all that antialiasing can
do there is spend its half-sample delay and its high-frequency attenuation: measured against the
plain port through this same pipeline the two differ by -36.2 / -31.9 / -30.2 dB, which is the
whole of the gap below. The departure is first order in the timestep - the same comparison reads
-48.6 dB at 1 MHz and -64.6 dB at 4 MHz - so it is the method's known cost and not an error in
it, and the reference is also offered delayed by half a sample to show that realigning does not
recover it either.

Recovering the null needs Holters' delay compensation, which discretises the block at two thirds
of the run rate and would move every corner of the linear part by 1.5x, breaking
DkTsClipBlockSmallSignalResponseMatchesItsClosedForm. That is the open decision recorded in spec
2026-08-26-aaa-audio-quality-design.md, Model corrections (D6c C2), and it is why TubeScreamer
still holds a DiodePairPort. The bound here is a regression floor on the measured numbers, not
an acceptance.

### `DkBlockNodeCarriesTheBlockThroughItsFloatBoundaryUnchanged`

`// The adapter scales by input_scale_volts both ways.`

tests/circuits/dk_block_node.hpp is the adapter every block-level measurement runs through, and
Task 7's product node follows the same rule, so what it owes is exact: the buffer sample times
input_scale_volts is what the block solves, and the block's answer divided by the same constant
is what lands in the output buffer. A non-unity scale is used so a dropped multiply on either
side shows up. The drive is 0.3 rather than the 0.5 the adapter defaults every wiper to, so
prepare()'s set_potentiometer and update_matrices_if_dirty have to have run for the two blocks
to agree.

### `DkTsClipBlockLeavesItsCurrentsInStepWithItsVoltagesAfterAFailedSolve`

`// A stiff diode and a flipping input exhaust the budget.`

Carried from the Task 3 review: DkCircuit re-evaluates the models on the failure path because
DampedNewtonSolver::fail() restores the lowest-residual iterate, which is generally not the
point the models were last evaluated at. The reviewer's recipe was a stiff port plus
set_maximum_iterations(1); DkCircuit exposes no setter for the solver's budget, so the budget is
exhausted the physical way instead - a diode nine times stiffer than a 1N914 and a stimulus that
flips sign every sample, which throws the warm start to the far side of the root. It lives here
rather than in test_dk_circuit.cpp because Task 5 does not touch that file and this is the block
whose port makes the failure reachable.

### `DkTsClipBlockDiodePairPortIsOneScalarPortInDisguise`

`// Branch 1 is branch 0 reversed, to the last bit.`

AdaaDiodePairPort's derivation rests on the pair's second branch being the negative of its
first, which makes the two-branch port one scalar port carrying i(v) - i(-v). The netlist wires
branch 1 as branch 0 reversed, so the claim holds to the last bit in p and in F rather than
approximately, and this is where that is checked.

### `AdaaDiodePairPortAntiderivativeDividesBackToThePairCurrent`

`// d/dp of H(v) - F00*h(v)^2/2 comes back as h(v).`

The identity the method stands on. With v = p + F00\*h(v), the substitution dp = (1 -
F00\*h'(v)) dv turns the integral of h dp into H(v) - F00\*h(v)^2/2, so the derivative of that
with respect to p has to come back as h(v) at every port voltage. A divided difference over a
narrow interval in p is the direct way to say so.

### `AdaaDiodePairPortSinksTheFoldedHarmonicThatSetsTheAliasFloor`

`// The 13th harmonic of 15 kHz folds to 3 kHz at 192 kHz.`

The mechanism task 9 diagnosed, isolated: at the shipping 4x run rate of 192 kHz the 13th
harmonic of a 15 kHz tone lands on 195 kHz and folds to 3 kHz, already in band before any filter
can act. 16384 samples at 192 kHz put both 15 kHz and 3 kHz on exact bins, so the two lines are
read without a window and without leakage between them.

### `AdaaDiodePairPortResetDropsItsSampleIntervalHistory`

`// A port with no history stamps the plain pair current.`

A port with no history has no interval to average over, so it stamps the plain pair current; the
node's reset() leans on that to restart click-free. The pair current is i(v) - i(-v) and the
port splits it evenly across the two branches, so the difference of the two stamped currents is
what has to come back.

### `decimate_to_run_rate`

`/// @brief Converts one signal from the LTspice grid to the shipping run rate.`

Converts one signal from the LTspice grid to the shipping run rate.

`Resampler` is the tree's committed arbitrary-ratio converter (DR1), which matters here because
1 MHz to 192 kHz is 24/125 and no halfband cascade reaches it. It is zero phase, so output time
t carries input time t and nothing has to be shifted back afterwards, and its kernel time-scales
with the ratio, so decimating moves its cutoff to 96 kHz and the reference arrives band-limited
to what a 4x model could represent in the first place.

The converter touches the reference and the stimulus, never a circuit output: both ports are
then driven by the same decimated stimulus and nulled against the same decimated reference, so
whatever colour it leaves below 10 kHz is common to both and cancels in the comparison the case
exists to make.

`// The block is compared on the replay of the stimulus.`

The reference window is the second half of LTspice's 40 ms run, so the block is walked through
the stimulus once and compared on the replay, exactly as the reference-rate case argues.

### `DkTsClipBlockBothPortsNullAgainstTheReferenceDecimatedToTheShippingRunRate`

`// Both ports null against LTspice decimated to 192 kHz.`

The decision measurement for D6c C2. The reference-rate case nulls at 1 MHz, where a clipped
corner carries almost no aliasing and antialiasing can only spend its half-sample delay and its
corner attenuation; it therefore says nothing about whether antialiasing helps at the rate the
pedal actually runs. This case asks the question that decides: decimate LTspice's 1 MHz
transient to the shipping 192 kHz, so the reference is the analogue circuit band-limited to what
a 4x model can represent, then null both ports against it there.

Whichever port comes back closer is the one that models the circuit better at the rate it ships
at. The verdict and its numbers are in spec 2026-08-26-aaa-audio-quality-design.md, Model
corrections (D6c C2).

`// Which port wins is printed, not asserted.`

Both ports must stay recognisably the same circuit; which one wins is what the printout reports
and the spec records, and it is not asserted here in either direction.

### `DkTsClipBlockBothPortsNullAgainstTheConvergedModelOnTheToneThatAliases`

`// The reference is the block's own model at 1.92 MHz.`

The companion to the decimated-LTspice case, and the reason that one cannot decide alone:
ts_clip's stimulus is a 1 kHz sine, and 192000 / 1000 is an integer, so at the shipping rate
every folded harmonic of that tone lands exactly on a harmonic of it. The stimulus is blind to
the aliasing antialiasing exists to remove, and can only charge it for its cost.

This case puts the question on the tone that does alias. The reference is the same block's own
equations at 1.92 MHz, where the 13th harmonic of 15 kHz is nowhere near Nyquist, then decimated
to 192 kHz by the same converter - a converged solution of the model rather than of the circuit,
which is the right reference for asking which discretisation is closer to the answer it is
trying to compute.

`// Both candidates are trimmed by the same amount.`

The head of the decimated reference carries the converter's start-up, and both candidates are
trimmed by the same amount, so neither is favoured.

`// The two-tap mean is a delay and a low-pass.`

null_test aligns an integer lag and a gain, so antialiasing's half sample survives it, and the
antialiased port is also offered the reference delayed by half a sample. Read that third number
carefully: the two-tap mean below is a delay AND a low-pass, cos(pi\*f/fs), which is -0.26 dB at
15 kHz but -2.6 dB at 45 kHz. Antialiasing's error is corner attenuation, so this filter
attenuates the reference's corner too and the confound runs in antialiasing's favour. It is
printed because it still loses under a handicap that helps it, which strengthens the verdict; it
does not separate delay from attenuation, and nothing here should be read as doing so. That
separation would want a Thiran allpass or a linear-phase term in the frequency domain.

`// These are regression floors, not acceptances.`

Regression floors on the measured numbers, not acceptances. Which port is closer is the decision
this case exists to make, and it is recorded in the spec rather than asserted here.

## `test_ts_tone_stage.cpp`

### `DkTsToneBlockResponseMatchesItsClosedForm`

`// The 0.25 dB tolerance is the sweep's own accuracy.`

Closed form of the corrected network, in dB:

```text
       100 Hz    1000 Hz   10000 Hz
0.1   -1.3471    -6.2987   -22.5431
0.5   -0.9946    -4.5852   -22.1373
0.9   -0.6948    -2.2872   -19.7318
```

The old netlist read -30.43 dB at 100 Hz and -34.38 dB at 1 kHz at noon, and moved level rather
than corner with the knob; these numbers check that the correction is in the block.

The 0.25 dB tolerance is the sweep measurement's own accuracy - D6a puts its worst error above
20 Hz at 0.17 dB - not the block's. The block is linear, so any drive level measures the same
response; -40 dBFS is what measure::frequency_response uses.

### `DkTsToneBlockClosedFormCostsUnderOneDecibelAtOneHundredHertzAtNoon`

`// The stage costs under 1 dB at 100 Hz at noon.`

The acceptance the old reference could not have met: its stage was an integrator with unity gain
at 1 / (2\*pi\*10k\*220n) = 72.3 Hz, so it read -30.43 dB at 100 Hz at noon. R.G. Keen on the
real stage: "there is no net signal loss through the tone control stage, a gain of 1". The three
factors at 100 Hz at noon are the filter -1.1957 dB, the gain leg +0.2887 dB and the output
network -0.0875 dB. Of the filter's loss, 0.8279 dB is R7 against the 10 kOhm bias return,
20\*log10(1 / (1 + 1k/10k)); the other 0.3679 dB is C5 and the wiper leg loading that node.

### `DkTsToneBlockClosedFormSweepsSixteenDecibelsAtTheLegsTurnoverFrequency`

`// The wiper leg's turnover sits at about 3.2 kHz.`

Keen puts the wiper leg's turnover where the 220 nF reaches 220 Ohm, "about 3.2KHz", and that is
where the knob has to do its work: at the + end the leg hangs on the passive filter and cuts, at
the - end it is the gain leg and boosts. The old netlist swung 19.08 dB of flat level here while
its corner moved 807 Hz to 760 Hz, which is a volume control, not a tone control.

`// At the bright end the stage passes +0.435 dB at 1 kHz.`

At the bright end the gain leg wins over the passive filter for about an octave around 1.1 kHz
and the whole stage passes more than it is given: +0.435 dB at 1 kHz, peaking at +0.465 dB at
1159 Hz. An integrator 30.43 dB down at 100 Hz cannot do that, so this is the single number that
separates the corrected netlist from the one it replaced.

### `DkTsToneBlockIsMonotonicBetweenThreeHundredHertzAndTwoKilohertzAtNoon`

`// The real network cannot notch above 723 Hz.`

The D5b/D6a tone control crossfaded a Butterworth low-pass against a Butterworth high-pass, both
at 720 Hz, so at noon the two cancelled and left a full null at 720 Hz. The real network cannot
notch: above the passive filter's 723 Hz corner it falls monotonically at every setting. The
closed form is checked at 1e-9 dB, which is what pins the shape; the measured sweep is checked
at 0.25 dB, its own accuracy, which is what pins that this block is that shape. A null like
D6a's is tens of dB deep and fails the loose gate as readily as the tight one.

### `DkTsToneBlockKnobMapsOntoTheReferencesSteppedRange`

`// TONE = 1 is bright and the mapping is the identity.`

TONE is measured from the op-amp's + input, where the wiper leg only cuts, so TONE = 1 is bright
and the mapping is the identity. Before the 2026-09-06 correction this ran backwards and stopped
at 0.1 and 0.9, because the old netlist muted at TONE = 1.

### `DkTsToneBlockPreparesAtEveryTonePositionIncludingBothEndStops`

`// Both end stops are ordinary settings.`

The old netlist muted at wiper position 1 - the pot shorted the wiper to the op-amp output and
the summing junction's equation collapsed - which is why tone_to_position used to stop short of
its own track. On the corrected network both ends are ordinary settings: at 0 the wiper leg
loads the passive filter, at 1 it is the whole gain leg, and neither is degenerate. This is the
case that fails first if someone reinstates the clamp.

### `DkTsToneBlockNullsAgainstTheLtspiceTransientReference`

`// One pre-roll pass matches LTspice's own start.`

tools/spice/export.py runs `tran 0 40m 20m 1u`: LTspice solves the DC operating point, runs 40
ms and saves the last 20 ms. This block sits at zero at DC - the 4V5 rail is an AC ground here -
so a DK block starting from zero state has LTspice's initial condition exactly, and the saved
window is 20 whole cycles of the 1 kHz stimulus. Replaying it ONCE therefore reproduces
LTspice's own first 20 ms, and the second pass is the comparison window. This is the clip
block's procedure, for the same reason.

Both signals are low-passed at 10 kHz by the same 4th-order Butterworth before comparison, so
the band-limiting cannot flatter either one. The block is linear, so a residual anywhere near
the -60 dB gate means a component value or a node index is wrong, not that a solver drifted.

## `test_tube_screamer.cpp`

### `TubeScreamerSmoothingTimeIsBlockSizeIndependent`

`// The 63 % crossing must not depend on the block size.`

Step the level from -60 dB to 0 dB and find, to the sample, when the output envelope first
reaches 63 % of its final settled peak - one time constant. Expressed in milliseconds that
crossing must not depend on the block size: 64 and 4096 frames must agree within 5 %. A 4 kHz
probe keeps the peak-detection granularity (half a period, 6 samples) far below the 1 ms that 5
% of the 20 ms time constant allows.

### `TubeScreamerReportsItsOversamplerLatencyAndTheBlocksAddOnlyPhaseLag`

`// Only the 4x oversampler's round trip is latency.`

latency_samples() reports the one delay a host can compensate: the 4x oversampler's round-trip.
The two DK blocks add group delay of their own, but it is frequency- and tone-knob-dependent, so
it is tone rather than latency and reporting it would over- compensate at every frequency but
one. The impulse peak therefore lands a couple of samples past the reported figure, and never
before it - see the D6c plan, task 8.

### `TubeScreamerThdIncreasesWithDrive`

`// The THD proxy is RMS deviation from a fitted sine.`

THD proxy: RMS deviation of the (settled, tail-half) output from a best-fit sine at the
fundamental, normalized by the output's own RMS. Not a true THD measurement (that needs an
FFT-based harmonic decomposition), but monotonic with clipping severity for this purpose, and
cheap.

### `TubeScreamerDefaultSettingsPeakNearMinusTwelveDbfsForAMinusTwelveDbfsInput`

`// At default drive the pedal is near unity at 82 Hz.`

At default drive the pedal is close to unity at 82 Hz, so the shipped calibration leaves a -12
dBFS low-E sine within a couple of dB of where it came in.

### `TubeScreamerHasNoCornerNearTwentyHertzTheOldBiquadRemoved`

`// The input corner is 0.312069 Hz, so 20 Hz is flat.`

The D6a chain high-passed at 20 Hz with a biquad. The real pedal's input network is 1 uF into
510 kOhm - a 0.312069 Hz corner - so the response either side of 20 Hz is flat, and 30 Hz
against 10 Hz is the pair that says so: a 20 Hz high-pass would put them octaves apart, while
the circuit leaves the ratio near 1. The reference is not 300 Hz, which sits on the clip stage's
own 4.7 kOhm / 0.047 uF boost slope and is 12 dB up for that reason. The chain measures 1.11; a
second-order 20 Hz high-pass would put it near 3.9. Small signal throughout so the clipper does
not compress the comparison.

`// The window is truncated to whole cycles.`

The measurement window is truncated to whole cycles of the tone under test. Blocks 100 to 199
hold 25600 samples, which is 16 cycles at 30 Hz but 5.33 at 10 Hz. The partial cycle biased the
10 Hz RMS low by 1.2 %, and the ratio above with it, from 1.11 to the 1.1233 this case used to
read.

### `TubeScreamerDriveKnobMovesTheClipThresholdWithoutClicking`

`// A drive sweep must not produce a sample-to-sample step.`

The drive pot re-derives the clip block's matrices at block cadence (D6b ruling 3). Sweeping it
across its whole travel over a second must not produce a sample-to-sample step: the largest
first difference stays under the largest one the same render produces with the knob still at
1.0, its loudest end, times two.

## `test_vari_rate_reader.cpp`

### `VariRateReaderDoesNotDriftFarDownTheTake`

`// At half speed 256 outputs cost 128 take frames.`

At half speed the step is exactly 0.5, so 256 outputs cost exactly 128 take frames once the
kernel has filled. An accumulator that loses the fraction across a block boundary shows up here
and not in the first bar.

## `test_wav_file.cpp`

### `WavFileLoadsMonoPcm24SamplesWithSignExtension`

`// The samples cover zero, both extremes and -1 LSB.`

0x000000 = 0, 0x400000 = +0.5 (positive, bit 23 clear), 0x7FFFFF = max positive, 0xFFFFFF = -1
LSB (all-ones, negative), 0x800000 = most-negative (bit 23 set, sign extension).

### `WavFileDownmixesStereoAsAverageOfChannels`

`// Same-sign values make a sum twice the average.`

Same-sign, unequal per-channel values: sum-instead-of-average is off by a factor of \~2 here
(unlike opposite-signed values, which would let sum and average both land near zero and hide an
averaging bug under a loose tolerance).
