# tests/nodes/dd3

## Measuring a filter in these tests

A probe drives a unit sine for 10 ms to let the poles settle, then reads the amplitude of the next
0.05 s as sqrt(I^2 + Q^2) \* 2/N with I and Q the sums of the output against cos and sin. Every
probe frequency is a multiple of 20 Hz, so the window holds a whole number of cycles and the
estimate is exact for a steady sine.

The filters keep double state and round to float at the input and at each section's output. A
chain of m sections has m + 1 rounding events, each at most u = 2^-24 of the signal at that point.
Taking the signal at every point as at most the L1 norm of the whole chain's impulse response (the
largest of the partial norms for low-pass chains), the error of a sample is at most (m + 1) \* u \*
L1, and the quadrature estimate turns an error of D per sample into at most 2D. The bound is
therefore 2 \* (m + 1) \* u \* L1, with L1 summed over 4096 samples of the filter's own impulse
response (every pole decays in under 0.3 ms). The bound is already a worst case, so the tests hold the error to a ratio of 1.


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

### `Dd3EmphasisConstantsMatchTheComponentValues`

`// The zero and the pole follow from R7, R8 and C4, and the shelf gain from 33/47.`

The zero is 1/(2\*pi\*(R7+R8)\*C4) and the pole 1/(2\*pi\*R8\*C4) with R7 47K, R8 10K and C4 4.7 nF.
The pole sits (R7+R8)/R8 = 5.7 times above the zero, so the shelf's high-frequency gain is
33/47 \* 5.7 = 1881/470 = 4.0021277, which the emphasis constants test derives instead of pasting.

### `Dd3EmitterFollowerDampingLowersEveryQ`

`// Q = 1/(1/q + 2*(1-K)*q) for a buffer of gain K.`

Q = 1/(1/q + 2\*(1-K)\*q) for the equal-R Sallen-Key with a buffer of gain K, which is what
takes the cascade from +6.45 dB at 7 kHz to inside the published window.
The oracle is built from the capacitors, not from the stored Q: the equal-R Sallen-Key with a buffer
of gain K has Q = 1/(2\*sqrt(C2/C1) + (1-K)\*sqrt(C1/C2)), which is 0.5\*sqrt(C1/C2) at K = 1 and
the code's expression after substituting q = 0.5\*sqrt(C1/C2).

### `Dd3PrewarpIsExactAtThePassbandEdgeAndScalesEveryCornerAlike`

`// One prewarp reference serves the whole cascade.`

One prewarp reference for the whole cascade, so tan(pi\*fd/fs) stays proportional to the
analogue f0 and the sections keep their stagger at every rate.

### `Dd3AntiAliasCascadeMatchesItsDigitalResponse`

`// Each section is the bilinear low-pass with its corner prewarped.`

Each section is the bilinear low-pass with its corner prewarped, so its exact response is
1/(1 - x^2 + jx/Q) with x = tan(pi\*f/fs)/tan(pi\*fc/fs), and the cascade is the product. The
filter is measured by a quadrature estimate over whole cycles and differs from that response only
by the float conversions of the input and of each section's output; the bound is derived from the
filter's own L1 norm in the test.

### `Dd3AntiAliasCascadeMatchesItsDampedPrototype`

`// The prewarp holds the passband close to the analogue prototype.`

The discretisation is checked against the analogue prototype at both rates the node runs at and
at 192 kHz. The prewarp holds the passband to under 0.5 % of the prototype, which is the whole
reason it is anchored at 7 kHz rather than per section. The bound is characterized: an offline
evaluation of both responses reads 0.446 % at worst, and it has no closed form yet.

### `Dd3ReconstructionCascadeMatchesItsDigitalResponse`

The same check as the anti-alias cascade, for the two reconstruction sections.

### `Dd3ReconstructionCascadeMatchesItsDampedPrototype`

`// The 4th-order filter is still rising at the band edge.`

The 4th-order filter alone is still rising at the band edge, which is why the 6th-order one
carries the roll-off. The agreement with the analogue prototype is the same characterized bound
as the anti-alias cascade's; the worst offline reading is 0.15 %.

### `Dd3AntiAliasAloneMeetsThePublishedResponseWindow`

`// The window is +1/-3 dB of 1 kHz from 40 Hz to 7 kHz.`

The acceptance: 40 Hz - 7 kHz within +1/-3 dB of 1 kHz, from the DD-3's published delay-path
figure (the boss dd3 design dossier research note in the version history section 3). The
anti-alias cascade alone is held to it at 48 and 96 kHz. The probes are sixth-octave steps
rounded to the 20 Hz grid that keeps a whole number of cycles in the measuring window.

### `Dd3DelayPathMeetsThePublishedResponseWindow`

`// On the board the wet signal passes through both cascades.`

The two cascades in series are held to the same window, because on the board the wet signal
passes through both.

### `Dd3AntiAliasCascadeRejectsAboveTheBandEdge`

`// 20 kHz is further down at 48 kHz than at 96 kHz.`

What the filter is for. The RBJ low-pass carries a zero at Nyquist the analogue prototype does
not, so 20 kHz is further down at 48 kHz than at 96 kHz. The digital response is the oracle at
10 kHz and 20 kHz at both rates (0.1336 and 4.8e-6 at 48 kHz; 0.2077 and 8.3e-4 at 96 kHz), so
a change of discretisation is caught rather than absorbed, to within the float conversions.

### `Dd3EmphasisPairIsTheIdentity`

`// The pair is unity by construction: 33/47 * 47/33 = 1.`

The pre-emphasis zero is the de-emphasis pole and vice versa, and 33/47 \* 47/33 = 1, so the
cascade is unity by construction. The error power on broadband noise is held to the square of the
worst-case float rounding error over the noise power, which the test builds from the two shelves'
L1 norms; no tolerance-based response check would catch a mismatch this small.

### `Dd3PreEmphasisMatchesItsDigitalShelfAtThreeProbes`

`// Both corners are prewarped separately.`

Both corners are prewarped separately, so the exact response is the shelf with each ratio f/fc
replaced by tan(pi\*f/fs)/tan(pi\*fc/fs). The probe stays at fs/16 or below; at fs/4 a
peak-of-sine estimator would read 99.58 % of the digital response, which hides the stretch. The
quadrature estimate reads the response exactly.

### `Dd3PreEmphasisHitsItsThreeDerivedGains`

`// The digital shelf sits above the analogue one as the tangent stretches the probe.`

The analogue shelf reads 0.7025132 below the zero, 1.3184111 at 1 kHz and 3.5023974 well above
the pole. The discretised one sits above it, because the bilinear tangent stretches the shelf as
the probe climbs (3.5234141 at 6 kHz). The gap is bounded by exp(m^2/2) - 1 with m the larger
of the probe and the pole angle, 1.9 % at 6 kHz.

### `Dd3DeEmphasisIsTheReciprocalOfThePreEmphasis`

`// The de-emphasis swaps the two corners and inverts the low-frequency gain.`

The de-emphasis swaps the two corners and inverts the low-frequency gain, so its exact digital
response is 1 over the digital pre-emphasis response. The measured response is held to it to
within the float conversions. Dd3EmphasisPairIsTheIdentity is what pins the pair on noise.

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
