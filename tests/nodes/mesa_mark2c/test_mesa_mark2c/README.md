# tests/nodes/mesa_mark2c/test_mesa_mark2c

## `test_behaviour.cpp`

### `MesaMark2cDescriptorListsTheParametersInContractOrder`

`// The index order is the contract of hosts and presets.`

The index order is the contract a host and a saved preset address these by, so it is pinned here
rather than merely checked for membership.

`// The three switches follow the fifteen knobs.`

The three panel switches follow the fifteen knobs, so that a preset saved without them still
addresses the same parameter at every index. The id, the display name and the port layout are
not asserted here: the registry test builds the node by id and reads its ports.

### `MesaMark2cIndexedParameterSetMatchesTheDescriptorOrder`

`// A switch reads back the position it selects.`

The switches read back the position they select, not the value that selected it, so each is
given a value that is already one of its own positions. The generic round trip by id reuses the
same table for the fifteen knobs; an index of 99 changes no parameter.

### `Medium_MesaMark2cSettled`

`// The settled DC stays under -40 dBFS and stops moving.`

Every knob is driven to both of its stops, one row each, and held there for 200 blocks. The
output must stay finite, its settled DC term must stay under -40 dBFS (`K_RESIDUAL_DC_BOUND`),
and that term must stop moving (`K_DC_DRIFT_BOUND`, 0.01, characterized).

The stage's secondary carries no mean, as a real secondary winding does, and the high stop
reads -0.00009 (-80.7 dBFS, 2026-09-07). See modules/nodes/mesa_mark2c/README.md.

At the low stop Master 1 sits at -60 dB, so a bound on the DC alone would pass on attenuation.
The drift check is what makes that extreme mean something: the DC of the third quarter and of
the last must agree, which a slowly diverging state would not.

### The grid probe tests

`// Seven stages report; an out-of-range index answers 0.`

The probe pair is what the README's grid table and the plate-swing work are read through, so it
needs its own cover, one promise per test: seven stages, a zero reading at rest, a reading above
zero for every stage after audio, an out-of-range index answering 0 rather than reading off the
end of the array, and a zero reading again after `reset()`. Nothing asserts a signal order, so
the names do not promise one.

### `Medium_MesaMark2cThd`

`// The sweep starts at 0.05, where the knob passes signal.`

Spec section 4: the metric saturates once every stage clips, so a rising spectrum is not
required of Lead Drive; what is required is that turning it up never makes the amp cleaner,
within the 1.0 percentage point that section allows for the wave squaring off. Measured with the
shared THD analyser at a bin-exact 1 kHz and a 0.2 input.

The sweep starts at 0.05, the first position that passes signal: audio_taper(0) is exactly zero,
so the knob's floor is a hard mute and the analyser would read noise against noise. One row per
neighbouring pair of the sweep; `MesaMark2cThdAtMaximumLeadDriveIsSaturated` holds the top of the
knob above 20 % (`K_SATURATED_THD_FLOOR_PERCENT`), so the sweep cannot pass on near-zero readings.

### `MesaMark2cNeverExceedsFullScaleWithoutALimiter`

`// The output stays in [-1, 1] and reaches the rail.`

A plain clamp is what bounds the output, so driving Lead Drive, Master 1 and Output to their
ceilings must leave the peak at exactly 1.0f: the clamp bounds it and the drive reaches it.
A peak below 1.0f would prove headroom rather than the clamp.

### `MesaMark2cHasACleanEndAtMinimumGain`

`// Lead Drive 0.02 at -40 dBFS reads 0.20 % THD.`

The clean end the effects loop network and Master 1 at node F give the amp: at Lead Drive 0.02
and -40 dBFS the chain reads 0.20 % THD (2026-09-08). The ceiling is 10 %
(`K_CLEAN_END_THD_CEILING_PERCENT`, characterized). Lead Drive 0.02 is the lowest
position this test uses, not a threshold: the control passes signal below it too.

### `MesaMark2cDefaultSettingsPeakNearMinusSixDbfsForAMinusTwelveDbfsInput`

`// A -12 dBFS 82 Hz sine comes out at about -6 dBFS.`

The shipped calibration point, and what pins MesaMark2c::K_MASTER_UNITY_GAIN: a -12 dBFS 82 Hz sine
into an untouched amp comes back out at about -6 dBFS, the frequency all four amplifiers pin at
(DA5 spec section 6). The window is 1 dB either way (`K_CALIBRATION_BAND_DB`, characterized).

### `MesaMark2cDryLatencyIsOneOversampledRegion`

`// The latency is one Oversampler8x round trip.`

V1A through the sag stage is a single 8x region, so the reported dry latency is one
Oversampler8x round trip; it is pinned against the oversampler rather than a literal.

### `MesaMark2cToneStackSitsBeforeTheLeadStages`

`// The stack is ahead of the distortion: treble moves THD.`

A brighter stack drives the lead stages harder, which shows as THD only while the chain is on
its knee: at Lead Drive 0.05 and a 0.01 input (-40 dBFS) treble 0 against 1 read 0.41 against
8.91 %, +8.50 pp (2026-09-08). At Lead Drive 1.0 and 0.2 every stage from V3B on is a square
with or without treble and the same difference is 0.03 pp; that is the amplifier, 110 dB of gain
fed a hot line level, so the operating point is the knee, not the ceiling.

The deck is terminated by V1A's 44 k plate impedance and Volume 1's 1 M, which costs 4.1 dB
ahead of the four lead stages, so the same measurement sits low on the knee and the difference
is +3.32 pp. The floor is 2.5 pp (`K_TREBLE_THD_RISE_FLOOR_PERCENTAGE_POINTS`, characterized).

### `MesaMark2cEqBandBoostsItsCentre`

`// At Master 1 -20 dB the power section is linear.`

The 750 Hz slider's full lift reaches the output only where the power section is linear, so
Master 1 sits 20 dB down: the lift read 16.93 dB there against 16.09 dB at 0 dB, where the
inverter's compression takes the rest (2026-09-08). The same repair the VH4's Deep test needed.
The centre is 723.43 Hz (`K_EQ_BAND_CENTRE_HZ`). The lift of 17 dB within 1 dB
(`K_EQ_BAND_LIFT_DB` and `K_EQ_BAND_LIFT_BAND_DB`) is characterized; the band's inductor has no
closed form here.

### `Medium_MesaMark2cTail`

`// The ratio sat at 0.92 while the loop oscillated.`

The power amplifier's feedback loop oscillated at 11.8 kHz when its depth was set from the
presence sweep alone, and the ratio below sat at 0.92 instead of falling.
docs/agent/2026_09_20_V0_1_0_RELEASE/REPORT.md carries the measurement. One row per Presence
position: the peak of the last block must be under 1e-3 of the first block after the drive stops
(`K_TAIL_DECAY_RATIO`, characterized). The name states the ratio, not silence.

### `MesaMark2cSweepSoundsTheSameWhateverTheHostBlockSize`

`// Coefficients are rebuilt once per control period.`

Coefficients are rebuilt once per control period, not once per block, so a knob under a sweep
reaches the audio at the same moments however the host cuts the work up. Rebuilding per block
would make the 256-frame render lag the 32-frame one by up to a whole block. The renders agree to
1e-3 of the largest sample (`K_BLOCK_SIZE_AGREEMENT`, characterized).

### `MesaMark2cPullDeepStopsV2bRemovingBassAtEightyHertz`

`// V2B's cathode shelf is 0.6972 deep, -3.13 dB.`

V2B's cathode shelf is 0.6972 deep, -3.13 dB (`K_V2B_SHELF_RATIO`; the expected shift is
-20 log10(0.6972) = +3.13 dB). Out, its zero sits at 339 Hz and 80 Hz is below it; in, the zero
drops to 10.3 Hz and 80 Hz climbs back to the shelf's top. The band is 0.4 dB
(`K_DEEP_SHIFT_BAND_DB`, characterized).

`MesaMark2cPullDeepDoesNothingAtThreeKilohertz`: at 3 kHz both settings are far above either
zero, so the switch moves the gain by no more than 0.1 dB (`K_DEEP_HIGH_BAND_DB`, characterized).

### The equalizer switch tests

With the bus lifted (`Out`) the 80 Hz slider reaches nothing, so both extremes give one gain.
Engaged (`In`), the same slider lifts the band it is wired to by more than 6 dB. `Auto` is the
`In` position for a node that is only ever the lead channel, and the two agree to 0.01 dB
(`K_EQUIVALENT_POSITION_BAND_DB`, `K_SLIDER_LIFT_FLOOR_DB`, characterized).

### `MesaMark2cPullBrightLiftsTrebleAtBothOfItsCapacitors`

`// The switch tilts the amplifier; it does not raise it.`

One switch, two capacitors: a second 250 pF beside the tone stack's treble cap and 180 pF across
Volume 1's top section. Both pass treble past their own corner, so what the switch does is tilt
the amplifier, not raise it. The 6 kHz lift must exceed the 200 Hz lift by 1 dB
(`K_BRIGHT_TILT_FLOOR_DB`, characterized).
