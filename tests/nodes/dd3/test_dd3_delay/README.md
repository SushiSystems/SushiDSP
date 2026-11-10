# tests/nodes/dd3/test_dd3_delay

## `test_acceptance.cpp`

The clock curve itself (the endpoints and the geometric law) is pinned exactly in
`test_dd3_clock.cpp`; nothing here repeats it.

### `Dd3AcceptanceMeasuredEchoLandsWhereTheClockCurveSaysItWill`

`// An impulse's echo cannot agree with a wrong fs.`

wet_delay_seconds() is derived from the same clock the prediction uses, so on its own it could
agree with a wrong fs. An impulse's echo cannot. TIME 0.5 is the midpoint the endpoints do not
cover, and mode L there is 403 ms of ring, which is where a factor-of-four error in N would show
first.

The slack is `K_ECHO_PEAK_SLACK_SAMPLES` (48, characterized) plus the clock's own float: the
clock keeps log(fs) in a float, which moves the delay by `expm1(u * ln fs)` of itself. Two 7 kHz
cascades spread the echo's peak and land it about 13 samples late. TIME 0.5 clocks the ring at
40620 Hz, below the host rate, so each conversion's kernel covers 34/40620 s = 40 host samples;
the centre of that spread is already in the prediction, and what widens is how far the argmax can
sit from it. The renders are 0.6 s each, so the test is medium.

### `Dd3AcceptancePitchSlursRatherThanJumping`

`// HOLD freezes the ring so only the clock moves the pitch.`

Acceptance row 4: "frequency trace under a time-knob sweep shows the R56/C52 glide, not a jump".
Fill the ring with a steady tone (397 cycles per pass of the long ring at the fast clock, so the
frozen loop joins its own tail without a phase step), freeze it with HOLD so the clock is the
only thing left that can move the pitch, then turn D.TIME across its whole range over 0.35 s and
trace the wet signal's upward zero crossings. The probe is 1999 Hz at the fast clock, a period of
24 samples, and 99 at the slow end (the ratio 82500/20000 = 4.125).

The bounds are derived from the quantisation of the trace. A reading is a difference of two
crossing indices, each within one sample of the true crossing, so a reading lies within one sample
of the true period P. The true period never shrinks while the clock only falls. Two readings of
the same true period therefore differ at most by (P - 1)/(P + 1), which rises with P, so the
first period of 24 gives the floor 23/25 = 0.92. The log of the clock is linear in the knob, so
one period of at most 100 samples grows the true period by 4.125^(100/16800) = 1.0084, and the
ceiling is (24 \* 1.0084 + 1)/23 = 1.096; an ungliding clock would step by 4.125. The distance
travelled is bounded below by the same reading error and by the trail of the glide: the clock
follows the knob through tau = 0.594 ms (28.5 samples), and the last reading is centred about
1.5 periods before the end of the trace. The test is medium: it renders about one second.

### `Medium_Dd3DelayBattery` (row table)

The D6b battery, 5.0 s long, at feedback 0.0, 0.5 and 1.0 (the last of which is past
self-oscillation) in every mode. HOLD gets two runs: one from a cold ring, and one entered halfway
(`LongToHold`) so that what it freezes is real content rather than the zeros reset() left. Each
row is one run of 240000 samples, so one row is one medium test.

The tripwire `K_DIVERGENCE_TRIPWIRE` (32 full scale, characterized) is for divergence: the
converter's own rails bound the loop at about 1.15 full scale.

### `Dd3AcceptanceKeepsTheDryPathAtFrameZeroAndDelaysOnlyTheWetPath`

`// The dry path is sample-aligned; only the wet one delays.`

Ruling 5. The node reports no latency, and what backs the claim is an impulse at frame 0: it
comes out of frame 0 unchanged (the dry sample is scaled by 2 V and back, both exact, and the wet
leg has returned nothing yet), every frame up to the ring's length at the fast clock is exactly
zero because every stage after the ring is causal, and the wet echo follows. The ring at TIME 0 in
mode S is 1024/82500 s = 595.8 samples at 48 kHz.

### `Dd3AcceptanceAliasFloorStaysUnderItsBoundAtTheServiceNoteStimulus`

`// The sweep runs at the service note's stimulus level.`

Ruling 7 deferred oversampling of the analog chain and promised a number instead of a paragraph.
alias_floor() sweeps 1-20 kHz tones and reports the worst non-harmonic line. The bound
`K_ALIAS_FLOOR_BOUND_DB` (-40 dB, characterized) is deliberately loose: the DD-3 is a 12-bit
converter behind a 7 kHz wall and is not supposed to be clean. The measured floor is -55.32 dB and
the revisit threshold of Ruling 7 is -60 dB, which the node misses by 4.7 dB; 16 sub-steps in the
rectifier would reach -65.13 dB, and the cost curve that chose 8 is in the dd3 spx90 design spec
in the version history, Model corrections.

The sweep runs at the service note's stimulus, not at alias_floor()'s -12 dBFS default. -12 dBFS
is 20 dB above the level this pedal is specified at, and the compressor puts 7.26 V peak on a
converter whose rails are +-2.4988 V: 78 % of the converter's samples clip and the sweep then
measures that clipping rather than the conversion. Nothing clips at the note's level, and what was
left there was the NE570's rectifier aliasing: |sin| carries a harmonic at every 2k\*f0 and
6\*8000.24 Hz folded to 1.46 Hz, inside the rectifier's own 23.4 Hz pole. The test is medium: the
sweep renders twenty tones.

## `test_behaviour.cpp`

### `Dd3DelayWetDelayMatchesThePlansArithmetic`

`// The two conversions cost 68 / min(fs, 48000), not 64.`

N2, with the resampler round trip read off the committed prototype rather than off the plan's
draft: Task 1.1 grew the half width from 32 to 34 zero crossings, so the two conversions cost 68
/ min(fs, 48000) between them, not 64.

```text
Mode L at TIME = 0: 16384/82500 + 68/48000 + 8/48000
  = 0.198593939 + 0.001416667 + 0.000166667 = 0.200177273 s = 9608.51 samples.
Mode S at TIME = 0: 1024/82500 + 68/48000 + 8/48000 = 0.013995455 s = 671.78 samples.
```

The test computes the sum from the formula, not from these figures, and compares it to the float
that holds log(fs) plus three double roundings.

### `Dd3DelayEchoLandsAtTheWetDelay`

`// 32 samples catch a wrong N, fs or round trip.`

The echo has been through two 7 kHz cascades, so its peak spreads over a few samples and lands
about 13 late; `K_ECHO_PEAK_SLACK_SAMPLES` (32, characterized) is tight enough to catch a wrong
N, a wrong fs or a missed round trip. The arithmetic and the echo are two tests, both medium since
the echo renders 0.5 s.

### `Dd3DelayRepeatsDecayAndDarkenWithFeedback`

`// Each repeat loses level and treble together.`

Dossier 3, "Self-oscillation": every repeat is re-band-limited by both 7 kHz cascades and
re-quantised, so repeats lose level and lose treble together. The take-off sits at the wet leg's
output, so the emphasis pair cancels inside the loop and the tilt is the filters'. The ratio
`K_CENTROID_FALL_RATIO` (0.95) is characterized.

### `Dd3DelayTailDiesBelowTheTopOfTheKnob` and `Dd3DelaySelfOscillatesAtTheTopOfTheKnob`

`// The tail sustains only at the top of the knob.`

K_FEEDBACK_PATH_GAIN exists so that K_MAXIMUM_FEEDBACK is the loop gain it claims to be: the loop's
forward gain peaks at 6.3184 at 3278 Hz, and on the mix resistors alone the node would
regenerate from F.BACK 0.22 upward. Below the top of the knob the tail dies (at 0.7 the loop gain
is 0.77, which leaves about 1e-8 of the burst after a second); at the top it sustains. The two rms
thresholds `K_DEAD_TAIL_RMS` (1e-4) and `K_SUSTAINED_TAIL_RMS` (5e-3) are characterized. Each is
its own medium test since each renders 2 s.

### `Dd3DelayHitsCheckPointFive`

`// 0.025 full scale in gives 0.035 full scale of wet out.`

Service note ladder, 1 kHz at 100 mVp-p in -> [5] 140 mVp-p of delay signal (dossier 1.0). In
this tree's units: 0.025 full scale in, 0.035 full scale of wet out. The band
`K_CHECK_POINT_FIVE_BAND` (5 %, characterized) holds the sampling of the peak (0.2 %), the
converter's half step and the compander's envelope ripple.

### `Dd3DelayResetReturnsItToTheStateOfAFreshNode`

`Dd3DelayAtZeroLevelIsAWire` compares bit for bit: the dry sample is scaled by 2 V and back by
2 V, both exact in binary, and the wet term is multiplied by a level of exactly 0.

A node that has run a loud sine and been reset renders silence exactly as a fresh node does; the
test compares the two renders bit for bit. The block-size tests compare bit for bit as well: the
clock grid is counted in frames across blocks and every stage runs one sample at a time.

## `test_output.cpp`

### `render_dd3`

MODE M at D.TIME 0.4 clocks the ring at 46804.2 Hz, so 4096 samples of it are 87.5 ms and the
whole wet path is 89.1 ms, several repeats inside the stimulus. F.BACK 0.45 sits inside the
loop's stable region while still recirculating: a change to either resampler, to the compander
or to the converter moves the file. Every knob is set before the render prepares the node so the
smoothers snap and the file holds the settled pedal rather than its ramp. The residual bound
`K_GOLDEN_RESIDUAL_BOUND_DB` (-80 dB) is characterized.

## `test_rt_safety.cpp`

### `ProcessPerformsNoHeapAllocation`

`// Both MODE changes below move N and pay the std::fill.`

Two things happen inside this trap window that no other node does. process() drives both
Resamplers' set_rates 32 times per block through the 8-frame clock grid, and a MODE change
clears the ring (plan Ruling 14) - once through set_parameter by index, which only records the
mode so that process() applies it at the next block start, and once through set_mode, which
applies it on the spot. Middle holds 4096 samples and Long 16384, so both changes below move N
and both pay the std::fill.
