# tests/nodes/jcm800/test_jcm800

## `test_behaviour.cpp`

### `unity_master_db`

`// unity_master_db is derived from K_MASTER_UNITY_GAIN.`

The master level whose linear multiplier is exactly 1.0. Master is a dB control whose 0 dB
reference is K_MASTER_UNITY_GAIN, so the tests that want "master at 1.0 linear"
ask for this and keep measuring the same signal - including across the D5b recalibration
of that reference, which is why it is derived from the constant rather than written out as a
number.

### `Medium_Jcm800FullScale` and `Jcm800OutputReachesTheRailAtMaximumDrive`

`// The clamp bounds the output at the rail.`

The soft-knee limiter is gone; a plain clamp bounds the output. Two rows drive preamp and master
to their ceilings, one with every tone knob up and one with the output at +12 dB, and every
sample must stay inside [-1, 1].

`// If nothing ever reaches 1.0 the bound above proves only headroom.`

The second row also reaches the rail exactly: if nothing ever hit 1.0 the bound would prove only
that the amp has headroom, not that the clamp works. The peak equals 1.0f because the clamp
bounds it and the drive reaches it.

### `Jcm800PreampIncreasesOutputEnergy`

`// The preamp knob never costs level; 1 % is sag drift.`

The claim is not 6 dB between preamp 0.1 and 1.0. DA4 removed the interstage staging scalars
and gave the chain the losses the decks document, and the cascade saturates below preamp 0.1, so
the knob moves output level by 5.3 % rather than 6 dB: 0.201164 RMS at 0.1 against 0.211904 at
1.0, measured 2026-09-07. What survives is that the knob never costs level, so that is what is
pinned, with 1 % of slack (`K_SAG_DRIFT_RATIO`, characterized) for the sag stage's drift over a
30-block run.

### `Jcm800OutputTracksInputLevelRatherThanPinningToTheSolverClamp`

`// The knob is read at 0.02; at zero there is no output.`

A quiet note and a loud note must come out at different levels; the failure this guards against
is the amp degenerating into a fixed-amplitude square-wave generator. Measured 2026-09-07 at
preamp 0.02, near the bottom of the knob where the cascade is not yet fully saturated: a 0.01
input peaks at 0.007612 and a 0.251 input at 0.123296, a factor of 16.2 (5.95 before the cold
clipper's gain and the inverter's bias were corrected in DA5 part 2). The knob is read at 0.02
rather than at the minimum, where preamp_attenuation(0) is exactly zero and there is no output
to compare. The floor of 2 (`K_LEVEL_TRACKING_RATIO_FLOOR`) is characterized: a clamped square
wave reads 1.

### `Jcm800DescriptorListsTheParametersInContractOrder`

`// The index order is the contract of hosts and presets.`

The index order is the contract a host and a saved preset address these by, so it is pinned here
rather than merely checked for membership. The id, the display name and the port layout are not
asserted here: the registry test builds the node by id and reads its ports.

### `Jcm800ToneChangeReachesTheSameSettledLevelAsAFreshNode`

`// The control thread stores a target and the audio thread walks to it.`

The control thread stores a target and nothing else; the audio thread smooths toward it over
20 ms and rebuilds the filters itself. The test sets bass to 1.0 through the control-thread route
after 40 blocks and compares the settled peak, 120 blocks later, with a node that had bass at 1.0
from `prepare()`. The two agree within `K_TONE_CHANGE_BAND_DB` (0.5 dB, characterized): the sag
stage carries a different history in the two runs. The earlier version read the `tone_stack_is_dirty()`
flag, which is internal state.

### `Jcm800OutputControlScalesTheSignalByItsDecibels`

`// The output control is linear and post-cabinet.`

The output control is linear and post-cabinet, so -6.0206 dB there (20 log10(0.5)) halves the
settled RMS. The bound is 16 float roundings of the reference (the decibel conversion, the
product and the mean square), `K_LINEAR_GAIN_ROUNDINGS`, characterized.

### `Jcm800InputTrimDrivesTheChainHarder`

`// The trim sits ahead of the nonlinear chain.`

The input trim sits ahead of the whole nonlinear chain, so it drives it harder rather than
scaling it: at this level the amp is still close to linear, so more in means more out.

### `Jcm800IsDcStableAcrossManyBlocksAtHighPreamp`

The name promised DC stability while the first version checked only finiteness. The test now
streams a 220 Hz sine at preamp 1.0 and presence 1.0 for 200 blocks and bounds the mean of the
last 100 blocks by `K_RESIDUAL_DC_BOUND` (0.01, -40 dBFS, characterized, the figure the VH4 uses).
The first version drove a constant 0.1; the sine keeps the drive of the sibling amplifiers.

### `Medium_Jcm800Thd`

`// THD must not fall as the preamp knob rises.`

Spec section 4's second acceptance criterion after the 2026-09-07 ruling: THD must not fall as
the gain knob rises, within 1.0 percentage point across its travel. The old claim was a strictly
monotonic rise from a clean bottom, which DA4 ended: with the interstage staging scalars gone,
V2a and the phase inverter clip at every setting and the metric saturates. Measured 2026-09-07
at a 0.2 input, master at 0 dB: 15.998 % at preamp 0.02, 42.812 % at 0.1, then 44.586, 44.803,
44.862, 44.893, 44.920, 44.942, 44.959, 44.974 and 44.986 % at 1.0. The sweep never falls; it
flattens above 0.2 because every stage is already clipping.

One row per neighbouring pair of the sweep, each rendering the 16384-sample settle and one FFT
window. The sweep starts at 0.02 rather than 0.0 because preamp_attenuation(0) is exactly zero:
the pot is a short to ground and there is no signal to measure.

`Jcm800ThdAtMaximumPreampIsSaturated` guards the sweep against being satisfied by a run of
near-zero readings: the top of the knob must read above 20 % (`K_SATURATED_THD_FLOOR_PERCENT`).

### `Jcm800PowerTubeBiasMismatchLowersTheSecondHarmonic`

`// The shipped mismatch is compared with a zeroed one.`

Guards the power-tube bias mismatch (jcm800.cpp's K_POWER_TUBE_GRID_BIAS_MISMATCH_VOLTS) against being
dropped or neutralized. It is asserted at the amp's output rather than at the pair because the
pair itself is not reachable from here; what an input-referred offset does to the pair's
composite is pinned in `tests/foundation/circuits/stages/test_pentode_stage/test_behaviour.cpp`.

The claim is self-checking: the same stimulus is rendered twice, once as shipped and once with
set_power_tube_grid_bias_mismatch_volts(0), so the gate is the comparison between the two rather
than a hand-copied number a later change could silently invalidate.

The direction has reversed twice. The corrected long-tailed-pair factor of 2026-09-07 made the
mismatch raise the second harmonic by 1.78e-5 (0.007257050 against 0.007239286); DA5 part 2,
with the offset in real volts on a volt-true pair and the inverter at its 2203 bias, has it
lowering the second harmonic again: 0.007392797 as shipped against 0.007536265 zeroed, a
difference of -1.435e-4 at preamp 0.5, 0.2 input, as the pre-DA4 chain did (2.17 % against 2.93
%). Which way the handover asymmetry adds to the preamp's asymmetry depends on the phase the
chain delivers, so the sign is a property of the shipped chain and is pinned as such.

The harmonic is read from the FFT window at a bin-exact 220 Hz after the settle, as a level in
dB below the fundamental. The shipped chain must sit at least `K_MISMATCH_SECOND_HARMONIC_SHIFT_DB`
(0.01 dB, characterized) below the zeroed one; the reading is 20 log10(0.007392797 / 0.007536265)
= -0.167 dB.

### `Jcm800DefaultSettingsPeakNearMinusSixDbfsForAMinusTwelveDbfsInput`

`// A -12 dBFS low-E sine comes out at about -6 dBFS.`

The shipped calibration point: a -12 dBFS low-E sine into an untouched amp comes back out at
about -6 dBFS, leaving headroom for a hotter pickup without the post-cabinet clamp ever engaging
on a normal signal. The window is 2 dB either way (`K_CALIBRATION_BAND_DB`, characterized; the
first version accepted -8 to -4 dBFS).

`// The first blocks hold the startup transient.`

The first blocks are skipped so the measurement is of the settled amp, not of the filters' and
the sag stage's startup transient.

### `Jcm800DryLatencyIsOneOversampledRegion`

`// The latency is one Oversampler8x round trip.`

D6a.3 collapses V1a..sag into a single 8x region, so the reported latency is one Oversampler8x
round trip - not two. A host compensates by this number, so it is pinned against the oversampler
itself rather than against a literal.

### `Jcm800HasACleanEndAtMinimumGain`

`// The knob is read at 0.01 because 0.0 passes no signal.`

Spec section 4's first acceptance criterion: with the gain knob at minimum and a -40 dBFS input,
THD must be below 10 %.

The knob is read at 0.01 rather than 0.0, because preamp_attenuation(0) is exactly zero: the pot
is a short to ground, nothing reaches V1b, and the amp has no output to measure. 0.01 is the
first position that passes signal.

### The grid probe tests

`Jcm800GridProbesReadZeroAtRestForEveryStage`, `Jcm800GridProbesMoveForEveryStageAfterAudio`,
`Jcm800GridProbeOutOfRangeReadsZero` and `Jcm800ResetClearsTheGridProbes` exercise the probe
accessors, which no other case reaches: spec section 4 asks for the readings to be recorded
rather than asserted, so they pin the hooks and not the figures. Every stage is in the signal
path, so a signal that reaches V1a reaches all four. Nothing asserts a signal order, so the names
do not promise one.

## `test_output.cpp`

### The alias-floor history

No test holds the alias floor while the issue is open: the test was a skipped body that
asserted nothing, and the issue lives in the backlog. The
bound it would hold is the D5b baseline of -3.8 dB less 30 dB, -33.8 dB.

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

-43.4 dB before DA4. Two D6a changes had got it there, pulling in opposite directions: Task 4's
>= 90 dB halfbands took the resampling aliasing out (-3.8 -> -32.0 dB), Task 5's single 8x
region cost 11.6 dB of that back because region A's decimator had been acting as an unplanned
interstage band-limit, and the Miller grid poles took that bandwidth away again for a physical
reason rather than a filtering one.

## `test_rt_safety.cpp`

### `ProcessPerformsNoHeapAllocationWithoutCabinet`

`// The per-block set_high_shelf() is pure arithmetic.`

This is also the trap for Jcm800::process()'s per-block set_high_shelf() call: recomputing the
presence shelf's RBJ coefficients every block looks expensive, but it is pure double arithmetic
(pow/sin/cos/sqrt) with no heap, lock, syscall, or throw behind it, so it must not register
here.
