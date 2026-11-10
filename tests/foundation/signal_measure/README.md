# tests/foundation/signal_measure

The tests of the measurement helpers in `modules/foundation/signal_measure/`. Each test node a
case drives is defined in the one file that uses it.

## `test_alias_floor.cpp`

### `UnoversampledTanhIsPoor`

`// Folded harmonics count as aliases.`

A 20 kHz tone through tanh at 48 kHz folds its harmonics straight back into band; the folded
third harmonic (60 kHz -> 12 kHz) is tens of dB up. This pins that the sweep counts folded
harmonics as aliases rather than masking them. The case passes a settle of 0.0 s: tanh has no
state, so every output sample is tanh of its own input and no transient precedes the first
analysis frame. The frame is bin-exact, so the lines' magnitudes do not depend on the tone's phase at
the frame start, and the -40 dB bound reads the same claim as with one second of settling; it is
not widened. The folded lines sit about -23 dB (see the next case), 17 dB above it.

### `OneToneOfAnUnoversampledTanhIsPoorAtFifteenKilohertz`

`// The 3rd harmonic of 15 kHz folds to 3 kHz at 48 kHz.`

The drive into the tanh is 4 x 0.251189 = 1.005, so the stimulus is tanh(sin t) to within half a
percent. The Fourier series of tanh(sin t) gives a first harmonic of about 0.80 and a third of about
0.055, which is -23 dB. At 15 kHz the only harmonic below 24 kHz is the fundamental, so the third
harmonic at 45 kHz folds to 3 kHz, outside every mask, and the line reads about -23 dB, above the
-40 dB the sweep case also uses. The case exists for `alias_floor_at_tone`, which the tube screamer
case at its worst tone calls.

## `test_frequency_response.cpp`

### `TracksAOnePoleLowpass`

`// The expectations are the difference equation's own |H|.`

Expectations are the difference equation's own |H|, not the analogue RC it is derived from: with
a = exp(-2\*pi\*1000/48000), |H(w)| = (1 - a) / |1 - a\*exp(-jw)| is -3.004 dB at 1 kHz and
-19.415 dB at 10 kHz. The analogue RC's -20.04 dB at 10 kHz is 0.63 dB away, which is the
impulse-invariant transform's warping, not measurement error.

### `IsAccurateAtTheBottomOfTheAudioBand`

`// The sweep starts at 5 Hz, two octaves below the band.`

The ESS carries no energy below its own start frequency, and its abrupt onset there leaves the
first bins of the deconvolved response contaminated by that truncation. When the sweep started
at 20 Hz this measurement read a 87 Hz first-order highpass at 20 Hz as -27.250 dB against an
exact -25.552 dB, an error of -1.699 dB, which is more than three times the half-decibel band
the LTspice AC comparisons are held to. Starting the sweep at 5 Hz instead - two octaves below
the lowest frequency any caller asks for - brings that same point to within 0.068 dB. This pins
the fix: it fails if the sweep's low edge is ever raised back into the reported band.

## `test_null_test.cpp`

### `AlignedDelayedCopyNullsCompletely`

`// The lag search matches on correlation magnitude.`

The rows are a noise copy at delay 3 and a tone copy at delay 37 with gain 0.25 and -0.25. In
the last row b is inverted as well as delayed and scaled, as it would be after an odd number of
inverting stages. The lag search has to match on correlation magnitude to find 37 here; matching on the
signed correlation picks whatever lag is least negative and the residual never nulls.

## Sizes, 2026-11-10

The four `frequency_response` cases took 0.22 to 0.97 s in the Debug run at the default two-second
sweep. They now pass `sweep_seconds = 0.5` (`K_SWEEP_SECONDS`), with a quarter second of tail, which
cuts the FFT of each to a quarter: the three 48 kHz cases (`IdentityIsFlat`,
`HalfGainReadsAsMinusSixDecibels`, `TracksAOnePoleLowpass`) are estimated at 0.06 s and are
`Small_FrequencyResponse`, and the 192 kHz bottom-of-the-band case at 0.24 s stays
`Medium_FrequencyResponse`. A float64 replica of the algorithm read at most 0.043 dB over the four
cases at 0.5 s, against 0.039 dB at two seconds and 0.119 dB at 0.4 s, which is why the sweep stops
at 0.5 s. `K_SWEEP_ERROR_DB` is a characterised constant. Each case records the `worst_sweep_error_db`
property; the Debug run of 2026-11-10 read 0.043106 dB at worst, and the bound is that figure plus
half of its last decimal.

`UnoversampledTanhIsPoor` took 0.115 s (20 tones of one second and a frame). It now passes
`settle_seconds = 0.0` to `alias_floor()`, which renders the analysis frame alone per tone, and is
`Small_AliasFloor`. The new time is not yet measured; the orchestrator's next Debug run reads it.
