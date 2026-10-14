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
