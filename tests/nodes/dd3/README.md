# tests/nodes/dd3

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
figure (the boss dd3 design dossier research note in the version history section 3). Both the
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
