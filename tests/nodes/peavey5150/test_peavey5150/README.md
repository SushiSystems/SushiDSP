# tests/nodes/peavey5150/test_peavey5150

## `test_behaviour.cpp`

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

## `test_output.cpp`

### `Peavey5150AliasFloorAtItsDefaultIsUnderMinusThirtyDb`

`// The case is skipped as a known issue but still prints.`

Skipped as a known issue; the measurement below still runs and still prints its figure.

DA5 spec section 11's second acceptance for the 5150: pre gain 0.5 at -12 dBFS under -30 dB. It
read +26.0 dB before DA5 part 1, -17.2 dB after it, -11.4 dB after part 2 and -26.8 dB with
every stage antialiased (part 4); the worst line is the 20 kHz tone's 19th harmonic folding
about 384 kHz to 3995 Hz, made by stages fed an already-clipped waveform.
