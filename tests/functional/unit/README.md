# tests/functional/unit

One test file stays here until its unit has a file of its own: `test_measure.cpp`, which
covers the measurement helpers of `signal_measure`.

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
