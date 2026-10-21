# tests/nodes/dd3/test_dd3_delay

## `test_acceptance.cpp`

### `File scope`

`// Every case renders seconds, hence SUSHIDSP_REGRESSION_TEST.`

Every case here is written with SUSHIDSP_REGRESSION_TEST: each one keeps a measured behaviour
from drifting. The alias-floor cases in each amplifier's and stage's `test_output.cpp` do the
same.

### `Dd3AcceptanceClockCurveMatchesThePublishedRange`

`// Row 1 checks fs, which the service note publishes.`

Acceptance row 1: within 2 % of the published 20-82.5 kHz curve. The check is on fs, which is
what the service note publishes (a scope on the sample-and-hold); the derived delay times are
printed beside it, because N = 1024/4096/16384 while the published times were computed with N =
1000/4000/16000, which is dossier open question 4.

`// The only slack is the float that holds log(fs).`

prepare() snaps the R56/C52 pole, so the only slack here is the float the clock keeps log(fs)
in, which is 7e-7 of the answer.

### `Dd3AcceptanceMeasuredEchoLandsWhereTheClockCurveSaysItWill`

`// An impulse's echo cannot agree with a wrong fs.`

Row 1 again, as a measurement rather than as arithmetic: wet_delay_seconds() is derived from the
same clock the prediction uses, so on its own it could agree with a wrong fs. An impulse's echo
cannot. TIME 0.5 is the midpoint the endpoints do not cover, and mode L there is 403 ms of ring,
which is where a factor-of-four error in N would show first.

`// The floor is absolute: 48 samples below 4800.`

Two 7 kHz cascades spread the echo's peak and land it about 13 samples late, so the floor is
absolute. It is 48 rather than the 32 test_dd3_delay.cpp uses at TIME 0 because the peak is
broader here: TIME 0.5 clocks the ring at 40620 Hz, below the host rate, so each conversion's
kernel covers 34/40620 s = 40 host samples where at TIME 0 it covers 34/48000 s = 34. The centre
of that spread is already in the prediction; what widens is how far the argmax can sit from it.
Above 4800 samples the published curve's own 2 % is the looser bound, and 1 % is what stays
inside it.

### `Dd3AcceptancePitchSlursRatherThanJumping`

`// HOLD freezes the ring so only the clock moves the pitch.`

Acceptance row 4: "frequency trace under a time-knob sweep shows the R56/C52 glide, not a jump".
Fill the ring with a steady tone, freeze it with HOLD so the clock is the only thing left that
can move the pitch, then turn D.TIME across its whole range and trace the wet signal's upward
zero crossings.

The pole itself is pinned exactly in test_dd3_clock.cpp: tau = 0.594 ms, 24.4657 % of the
remaining distance per 8-frame sub-block. A single full-scale TIME step is not something a
zero-crossing trace can resolve, because the glide ends before the trace takes its first
reading: at 48 kHz it is over in about 3 ms, while the probe has already arrived at the
destination clock, where a 1 kHz tone reads out at 242.4 Hz and its period is 198 samples, or
4.1 ms. The transcription in the task report puts the first crossing at sample 148 and every
crossing after it on that settled 198-sample grid. What the trace can resolve, and what the row
is about, is the knob being turned: it must walk the whole 4.125:1 range without a step, where
an ungliding clock would move it 4.125x between one period and the next.

`// 397 cycles per ring pass join the loop without a step.`

397 cycles per ring pass, so the frozen loop joins its own tail without a phase step. The read
pointer does lap the ring during the case - the 20 ms settle spends 1650 ticks at 82500 Hz and
the sweep 15474 more, against a 16384-sample ring - and that continuity is what makes the lap
harmless rather than one anomalous period in the trace.

`// One crossing sample is 4 %, so the bound is 0.90.`

Monotone downward in pitch. The trace quantises the period to whole samples and it starts at 24,
so one crossing sample is 4 % and the bound cannot be tighter than that. It is 0.90 rather than
0.96 because 4 % is the quantum of a single reading and two adjacent readings can round opposite
ways: the transcription's worst dip is 23/24 = 0.9600, and the extra 6 % is what the resamplers'
own smoothing of the trace is allowed to cost.

### `Dd3AcceptanceProducesNoNonFiniteSampleUnderTheStimulusBattery`

`// HOLD runs twice: from a cold ring and entered halfway.`

The D6b battery at feedback 0.0, 0.5 and 1.0 - the last of which is past self-oscillation - in
every mode. HOLD gets two runs: one from a cold ring, and one entered halfway so that what it
freezes is real content rather than the zeros reset() left.

`// The bound is a tripwire for divergence.`

The converter's own rails bound the loop at about 1.15 full scale, so this is a tripwire for
divergence rather than a level the model approaches.

### `Dd3AcceptanceReportsZeroLatencyAndDelaysOnlyItsWetPath`

`// The dry path is sample-aligned; only the wet one delays.`

Ruling 5. The node reports no latency, and what backs the claim is that the dry path is
genuinely sample-aligned: at E.LEVEL 0 the node is bit-close to a wire, which
test_dd3_delay.cpp's Dd3DelayAtZeroLevelIsAWire pins. The wet delay is reported apart from it,
because it is the effect rather than something a host should compensate.

### `Dd3AcceptanceAliasFloorIsMeasuredNotAssumed`

`// The sweep runs at the service note's stimulus level.`

Ruling 7 deferred oversampling of the analog chain and promised a number instead of a paragraph.
alias_floor() sweeps 1-20 kHz tones and reports the worst non-harmonic line. The assertion is
deliberately loose: the DD-3 is a 12-bit converter behind a 7 kHz wall and is not supposed to be
clean. The printed figure is the point - if it comes back worse than -60 dB, Ruling 7 is
revisited with data.

The sweep runs at the service note's stimulus, not at alias_floor()'s -12 dBFS default. -12 dBFS
is 20 dB above the level this pedal is specified at, and the compressor puts 7.26 V peak on a
converter whose rails are +-2.4988 V: 78 % of the converter's samples clip and the sweep then
measures that clipping rather than the conversion, reading -20.4 dB where an 8 kHz tone's fifth
harmonic folds about the 40620 Hz clock onto 619 Hz.

Nothing clips at the note's level, and what was left there was the NE570's rectifier aliasing
rather than a fold of the test tone: |sin| carries a harmonic at every 2k\*f0 and 6\*8000.24 Hz
folded to 1.46 Hz, inside the rectifier's own 23.4 Hz pole. The Ruling 7 revisit took that
reading and bought Ne570Compander::K_RECTIFIER_OVERSAMPLING with it, which moved this row from
-35.86 dB to -55.32 dB. The number still misses the -60 dB the printf names, by 4.7 dB; 16
sub-steps would reach -65.13 dB, and the cost curve that chose 8 is in
the dd3 spx90 design spec in the version history, Model corrections.

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

`// 32 samples catch a wrong N, fs or round trip.`

The echo has been through two 7 kHz cascades, so its peak spreads over a few samples and lands
about 13 late; 32 samples is tight enough to catch a wrong N, a wrong fs or a missed round trip.

### `Dd3DelayRepeatsDecayAndDarkenWithFeedback`

`// Each repeat loses level and treble together.`

Dossier 3, "Self-oscillation": every repeat is re-band-limited by both 7 kHz cascades and
re-quantised, so repeats lose level and lose treble together. The take-off sits at the wet leg's
output, so the emphasis pair cancels inside the loop and the tilt is the filters'.

### `Dd3DelaySelfOscillatesOnlyAtTheTopOfTheKnob`

`// The tail sustains only at the top of the knob.`

K_FEEDBACK_PATH_GAIN exists so that K_MAXIMUM_FEEDBACK is the loop gain it claims to be: the loop's
forward gain peaks at 6.3184 at 3278 Hz, and on the mix resistors alone the node would
regenerate from F.BACK 0.22 upward. Below the top of the knob the tail dies; at the top it
sustains.

### `Dd3DelayHitsCheckPointFive`

`// 0.025 full scale in gives 0.035 full scale of wet out.`

Service note ladder, 1 kHz at 100 mVp-p in -> [5] 140 mVp-p of delay signal (dossier 1.0). In
this tree's units: 0.025 full scale in, 0.035 full scale of wet out.

## `test_output.cpp`

### `render_dd3`

MODE M at D.TIME 0.4 clocks the ring at 46804.2 Hz, so 4096 samples of it are 87.5 ms and the
whole wet path is 89.1 ms, several repeats inside the stimulus. F.BACK 0.45 sits inside the
loop's stable region while still recirculating: a change to either resampler, to the compander
or to the converter moves the file. Every knob is set before `prepare()` so the smoothers snap
and the file holds the settled pedal rather than its ramp.

## `test_rt_safety.cpp`

### `ProcessPerformsNoHeapAllocation`

`// Both MODE changes below move N and pay the std::fill.`

Two things happen inside this trap window that no other node does. process() drives both
Resamplers' set_rates 32 times per block through the 8-frame clock grid, and a MODE change
clears the ring (plan Ruling 14) - once through set_parameter by index, which only records the
mode so that process() applies it at the next block start, and once through set_mode, which
applies it on the spot. Middle holds 4096 samples and Long 16384, so both changes below move N
and both pay the std::fill.
