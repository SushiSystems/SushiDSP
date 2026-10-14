# tests/nodes/diezel_vh4/test_diezel_vh4

## `test_behaviour.cpp`

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
