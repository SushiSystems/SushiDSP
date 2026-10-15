# tests/nodes/tubescreamer/test_tube_screamer

## `test_behaviour.cpp`

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

## `test_output.cpp`

The source carried this line above both `TubeScreamerGoldenFile48k` and
`TubeScreamerGoldenFile96k`: "Expected red until Task 8 re-pins against the DK chain at 4x - see
the D6c plan." `modules/foundation/golden_compare/README.md` lists a tube_screamer pin by D6c Task 8 dated
2026-09-07, so the note is out of date for both cases. Neither case was run for this page;
`sd test -s regression` shows whether they are green.

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

## `test_rt_safety.cpp`

### `ProcessPerformsNoHeapAllocation`

`// A moved knob re-derives the matrices inside process().`

Moving a knob before every block is the path the plain loop misses: a wiper that has changed
marks the block dirty and update_matrices_if_dirty() re-derives inside process(), which is where
a solver would allocate if it were going to. This is the D6c instance of the trap D6b's
DkCircuitPotentiometerUpdatePerformsNoHeapAllocation established, now on a shipping node rather
than on a bare circuit. Both knobs sweep their full travel so every derivation sees a genuinely
new matrix.
