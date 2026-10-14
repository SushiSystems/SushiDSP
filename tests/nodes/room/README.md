# tests/nodes/room

## `test_ambisonic_encoder.cpp`

### `AmbisonicEncoderIsOrthonormalOverTheSphere`

`// The quadrature is exact here; the residual is rounding.`

Under SN3D the sphere mean of Y_i Y_j is 1 / (2n + 1) on the diagonal and zero off it; 42
Gauss-Legendre elevations times 61 azimuths, 2562 points, integrate every product of two order-3
harmonics exactly, so the residual is float rounding alone.

## `test_band_response_fit.cpp`

### `BandResponseFitReachesADipBetweenFlatBands`

`// Each section has a peak, since two shelves cannot dip.`

Two shelves cannot dip, so the model gives each section a peak as well as a step. The first two
targets are one band cut with its neighbours flat; the third is the reference garage's direct
path through the 4x12, whose 2 kHz octave the array null pulls down. Measured: 0.20, 0.45 and
0.10 dB.

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
