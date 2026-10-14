# tests/nodes/mesa_mark2c/test_mesa_mark2c

## `test_behaviour.cpp`

### `MesaMark2cDescriptorExposesEighteenParametersInContractOrder`

`// The index order is the contract of hosts and presets.`

The index order is the contract a host and a saved preset address these by, so it is pinned here
rather than merely checked for membership.

`// The three switches follow the fifteen knobs.`

The three panel switches were appended on 2026-09-21, after the fifteen knobs, so that a preset
saved before them still addresses the same parameter at every index.

### `MesaMark2cIndexedParameterSetMatchesTheDescriptorOrder`

`// A switch reads back the position it selects.`

The switches read back the position they select, not the value that selected it, so each is
given a value that is already one of its own positions.

### `MesaMark2cIsDcStableAcrossManyBlocksAtHighGain`

`// The settled DC stays under -40 dBFS and stops moving.`

Every knob is driven to both of its stops and held there for 200 blocks. The output must stay
finite, its settled DC term must stay under -40 dBFS, and that term must stop moving.

The bound was 0.25 while OutputTransformerStage ran its tanh after its own rolloff and the
rectification the tanh introduced at the extreme setting was never blocked: the high stop read
-0.0506. Since DA5 part 3 the stage's secondary carries no mean, as a real secondary winding
does, and the high stop reads -0.00009 (-80.7 dBFS, 2026-09-07). See
modules/nodes/mesa_mark2c/README.md.

At the low stop Master 1 sits at -60 dB, so a bound on the DC alone would pass on attenuation.
The drift check is what makes that extreme mean something: the DC of the third quarter and of
the last must agree, which a slowly diverging state would not.

### `MesaMark2cGridProbesReportEveryStageInSignalOrder`

`// Seven stages report; an out-of-range index answers 0.`

The probe pair is what the README's grid table and the plate-swing work are read through, so it
needs its own cover: seven stages, every one of them reporting a grid that moved, and an
out-of-range index answering 0 rather than reading off the end of the array.

### `MesaMark2cThdDoesNotFallAsLeadDriveRises`

`// The sweep starts at 0.05, where the knob passes signal.`

Spec section 4: the metric saturates once every stage clips, so a rising spectrum is not
required of Lead Drive; what is required is that turning it up never makes the amp cleaner,
within the 1.0 percentage point that section allows for the wave squaring off. Measured with the
shared THD analyser at a bin-exact 1 kHz and a 0.2 input.

The sweep starts at 0.05, the first position that passes signal: audio_taper(0) is exactly zero,
so the knob's floor is a hard mute and the analyser would read noise against noise.

### `MesaMark2cNeverExceedsFullScaleWithoutALimiter`

`// The output stays in [-1, 1] and reaches the rail.`

A plain clamp is what bounds the output, so driving Lead Drive, Master 1 and Output to their
ceilings must still leave every sample inside [-1, 1] and must actually reach the rail;
otherwise the test proves headroom rather than the clamp.

### `MesaMark2cHasACleanEndAtMinimumGain`

`// Lead Drive 0.02 at -40 dBFS reads 0.20 % THD.`

The clean end the effects loop network and Master 1 at node F give the amp: at Lead Drive 0.02
and -40 dBFS the chain reads 0.20 % THD (2026-09-08), where it read 40.7 % with V2A's plate
meeting V2B's grid directly and Master 1 scaling the output. Lead Drive 0.02 is the lowest
position this test uses, not a threshold: the control passes signal below it too.

### `MesaMark2cDefaultSettingsPeakNearMinusSixDbfsForAMinusTwelveDbfsInput`

`// A -12 dBFS 82 Hz sine comes out at about -6 dBFS.`

The shipped calibration point, and what pins MesaMark2c::K_MASTER_UNITY_GAIN: a -12 dBFS 82 Hz sine
into an untouched amp comes back out at about -6 dBFS, the frequency all four amplifiers pin at
(DA5 spec section 6).

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

Since 2026-09-20 the deck is terminated by V1A's 44 k plate impedance and Volume 1's 1 M, which
costs 4.1 dB ahead of the four lead stages, so the same measurement sits lower on the knee and
the difference is +3.32 pp. The threshold moved with it. The property the test exists for is
unchanged: the stack is ahead of the distortion, so treble moves THD.

### `MesaMark2cEqBandBoostsItsCentre`

`// At Master 1 -20 dB the power section is linear.`

The 750 Hz slider's full lift reaches the output only where the power section is linear, so
Master 1 sits 20 dB down: the lift read 16.93 dB there against 16.09 dB at 0 dB, where the
inverter's compression takes the rest (2026-09-08). The same repair the VH4's Deep test needed.
The centre moved from 979.5 Hz on 2026-09-20 with the band's inductor
(docs/agent/2026_09_20_V0_1_0_RELEASE/REPORT.md).

### `MesaMark2cFallsSilentAtEveryPresencePositionWhenTheDriveStops`

`// The ratio sat at 0.92 while the loop oscillated.`

The power amplifier's feedback loop oscillated at 11.8 kHz on 2026-09-21 when its depth was set
from the presence sweep alone, and the ratio below sat at 0.92 instead of falling.
docs/agent/2026_09_20_V0_1_0_RELEASE/REPORT.md carries the measurement.

### `MesaMark2cSweepSoundsTheSameWhateverTheHostBlockSize`

`// Coefficients are rebuilt once per control period.`

Coefficients are rebuilt once per control period, not once per block, so a knob under a sweep
reaches the audio at the same moments however the host cuts the work up. Rebuilding per block
made the 256-frame render lag the 32-frame one by up to a whole block.

### `MesaMark2cPullDeepStopsV2bRemovingBass`

`// V2B's cathode shelf is 0.6972 deep, -3.13 dB.`

V2B's cathode shelf is 0.6972 deep, -3.13 dB. Out, its zero sits at 339 Hz and 80 Hz is below
it; in, the zero drops to 10.3 Hz and 80 Hz climbs back to the shelf's top.

### `MesaMark2cPullBrightLiftsTrebleAtBothOfItsCapacitors`

`// The switch tilts the amplifier; it does not raise it.`

One switch, two capacitors: a second 250 pF beside the tone stack's treble cap and 180 pF across
Volume 1's top section. Both pass treble past their own corner, so what the switch does is tilt
the amplifier, not raise it.
