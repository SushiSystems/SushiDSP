# tests/foundation/signal_measure

The tests of the measurement helpers in `modules/foundation/signal_measure/`. Each test node a
case drives is defined in the one file that uses it.

## `test_alias_floor.cpp`

### `UnoversampledTanhIsPoor`

`// Folded harmonics count as aliases.`

A 20 kHz tone through tanh at 48 kHz folds its harmonics straight back into band; the folded
third harmonic (60 kHz -> 12 kHz) is tens of dB up. This pins that the sweep counts folded
harmonics as aliases rather than masking them.

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

### `HandlesPolarityInversion`

`// The lag search matches on correlation magnitude.`

b is inverted as well as delayed and scaled, as it would be after an odd number of inverting
stages. The lag search has to match on correlation magnitude to find 37 here; matching on the
signed correlation picks whatever lag is least negative and the residual never nulls.
