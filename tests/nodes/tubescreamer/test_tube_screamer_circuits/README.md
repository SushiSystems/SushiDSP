# tests/nodes/tubescreamer/test_tube_screamer_circuits

## `test_clip_stage.cpp`

### `DkTsClipBlockSmallSignalResponseMatchesItsClosedForm`

`// At 1e-4 V the diodes stay on zero-bias conductance.`

Driven far below the clipping threshold so the diodes stay on their zero-bias conductance, which
is the regime the closed form (and an LTspice .ac) describes. The pair's conductance grows as
cosh(w / nVt); holding the diode voltage w under 14 mV keeps it within 5 % of the zero-bias
value, and at the highest gain here (107.6) that means an input under 0.0143 / 106.6 = 1.34e-4
V. 1e-4 V is used for all three settings.

Closed-form values from the D6c plan, in dB:

```text
       100 Hz    1000 Hz
0.1   10.1534   25.1395
0.5   18.8134   34.1273
0.9   22.9649   38.2321
```

### `DkTsClipBlockClipsAtTheReferencePeakVoltage`

`// The diode pair, not Rf, sets the peak above threshold.`

The reference transient drives 100 mV at 1 kHz and peaks at 0.481683 / 0.490169 / 0.491758 V for
DRIVE = 0.1 / 0.5 / 0.9 - almost independent of drive, because the diode pair, not the feedback
resistor, is what sets the output above threshold. Back of envelope at DRIVE = 0.1: the peak
drives 0.1 / |4700 - j3386.28| = 17.26 uA into the ground leg, and the diode-pair voltage
carrying that through Rf in parallel with the pair is 0.3886 V, so the output is 0.1 + 0.3886 =
0.4886 V. This case is the legible failure the null test is not: a stamp sign error moves this
number by volts.

### `DkTsClipBlockNewtonConvergesOnNinetyNinePointNineNinePercentOfSamples`

`// The stimulus is a sweep and three bursts at 384 kHz.`

Spec's Newton acceptance, on this block's own material: a 2 s sweep 20 Hz -> 20 kHz at -12 dBFS
in volts (0.251 \* K_FULL_SCALE_VOLTS = 0.502 V), then 1 kHz bursts at 0.05 V, 0.5 V and 5 V, all
at 384 kHz with the drive pot wide open, which is the hardest setting.

The D6c plan instrumented this block at 2.7 Newton iterations per solve on average and 4 worst
case, but it did that on a 0.5 V stimulus, and the 5 V burst is harder than anything the plan
measured. One run before the session was interrupted saw the eight-iteration budget exhausted on
57 of 1 056 000 samples, 0.0054 %, inside the spec's 0.01 % but past the plan's "4 worst case".
That figure is unverified since; the deferred sd test run confirms or corrects it.

### `DkTsClipBlockAdaaPortAlmostDoublesTheNewtonMissRate`

`// The ADAA port measures 0.0105 % against 0.0054 %.`

AdaaDiodePairPort's currents_and_jacobian() is DiodePairPort's character for character, so the
residual Newton solves is the same function and the extra misses are not a harder solve: they
are a different state trajectory arriving at harder open-circuit voltages. The measured rate is
0.0105 % against the plain port's 0.0054 %, which is past the spec's 0.01 % gate. That miss is
recorded rather than accommodated - it is one of the two reasons the shipping node still holds a
DiodePairPort (spec 2026-08-26-aaa-audio-quality-design.md, Model corrections D6c C2).

### `run_clip_transient_null`

`/// @brief Nulls the clip block against ts_clip's LTspice transient through one port.`

Nulls the clip block against ts_clip's LTspice transient through one port.

Templated so the antialiased port is held against the same reference at the same tolerance.
Antiderivative antialiasing evaluates the pair current half a sample early inside the feedback
loop, which is a real change to the modelled circuit; the honest place to see what it costs is
this residual, not an argument (spec 2026-08-26-aaa-audio-quality-design.md, Model corrections
D6c C2).

`// One pre-roll pass matches LTspice's own start.`

tools/spice/export.py runs `tran 0 40m 20m 1u`: LTspice solves the DC operating point, runs 40
ms and saves the last 20 ms. This block sits at zero at DC - there is no supply rail - so a DK
block starting from zero state has LTspice's initial condition exactly, and the saved window is
20 whole cycles of the 1 kHz stimulus. Replaying it ONCE therefore reproduces LTspice's own
first 20 ms, and the second pass is the comparison window. Replaying it more converges this
block to the periodic steady state while the reference is still on its turn-on transient, and
the null gets worse - see the pre-roll table in the D6c plan. This is why jcm_v1a's eight passes
are wrong here: that block has a 360 V rail and a bias point.

Both signals are low-passed at 10 kHz by the same 4th-order Butterworth before comparison, so
the band-limiting cannot flatter either one; the top octave is where LTspice's variable-step
integrator and our fixed-step trapezoidal rule are known to disagree (spec Risks). Computed
while writing the D6c plan with exactly this procedure: -80.3 / -73.7 / -76.0 dB.

### `DkTsClipBlockAdaaPortFallsShortOfTheLtspiceTransientReferenceByItsOwnBandLimiting`

`// The bound is a regression floor, not an acceptance.`

This case does NOT meet the spec's -60 dB null, and its name says so. The reference grid is 1
MHz, where a clipped corner has next to no aliasing left to remove, so all that antialiasing can
do there is spend its half-sample delay and its high-frequency attenuation: measured against the
plain port through this same pipeline the two differ by -36.2 / -31.9 / -30.2 dB, which is the
whole of the gap below. The departure is first order in the timestep - the same comparison reads
-48.6 dB at 1 MHz and -64.6 dB at 4 MHz - so it is the method's known cost and not an error in
it, and the reference is also offered delayed by half a sample to show that realigning does not
recover it either.

Recovering the null needs Holters' delay compensation, which discretises the block at two thirds
of the run rate and would move every corner of the linear part by 1.5x, breaking
DkTsClipBlockSmallSignalResponseMatchesItsClosedForm. That is the open decision recorded in spec
2026-08-26-aaa-audio-quality-design.md, Model corrections (D6c C2), and it is why TubeScreamer
still holds a DiodePairPort. The bound here is a regression floor on the measured numbers, not
an acceptance.

### `DkBlockNodeCarriesTheBlockThroughItsFloatBoundaryUnchanged`

`// The adapter scales by input_scale_volts both ways.`

modules/foundation/circuit_blocks/include/SushiDSP/foundation/circuit_blocks/dk_block_node.hpp is the adapter every block-level measurement runs through, and
Task 7's product node follows the same rule, so what it owes is exact: the buffer sample times
input_scale_volts is what the block solves, and the block's answer divided by the same constant
is what lands in the output buffer. A non-unity scale is used so a dropped multiply on either
side shows up. The drive is 0.3 rather than the 0.5 the adapter defaults every wiper to, so
prepare()'s set_potentiometer and update_matrices_if_dirty have to have run for the two blocks
to agree.

### `DkTsClipBlockLeavesItsCurrentsInStepWithItsVoltagesAfterAFailedSolve`

`// A stiff diode and a flipping input exhaust the budget.`

Carried from the Task 3 review: DkCircuit re-evaluates the models on the failure path because
DampedNewtonSolver::fail() restores the lowest-residual iterate, which is generally not the
point the models were last evaluated at. The reviewer's recipe was a stiff port plus
set_maximum_iterations(1); DkCircuit exposes no setter for the solver's budget, so the budget is
exhausted the physical way instead - a diode nine times stiffer than a 1N914 and a stimulus that
flips sign every sample, which throws the warm start to the far side of the root. It lives here
rather than in test_dk_circuit.cpp because Task 5 does not touch that file and this is the block
whose port makes the failure reachable.

### `DkTsClipBlockDiodePairPortIsOneScalarPortInDisguise`

`// Branch 1 is branch 0 reversed, to the last bit.`

AdaaDiodePairPort's derivation rests on the pair's second branch being the negative of its
first, which makes the two-branch port one scalar port carrying i(v) - i(-v). The netlist wires
branch 1 as branch 0 reversed, so the claim holds to the last bit in p and in F rather than
approximately, and this is where that is checked.

### `AdaaDiodePairPortAntiderivativeDividesBackToThePairCurrent`

`// d/dp of H(v) - F00*h(v)^2/2 comes back as h(v).`

The identity the method stands on. With v = p + F00\*h(v), the substitution dp = (1 -
F00\*h'(v)) dv turns the integral of h dp into H(v) - F00\*h(v)^2/2, so the derivative of that
with respect to p has to come back as h(v) at every port voltage. A divided difference over a
narrow interval in p is the direct way to say so.

### `AdaaDiodePairPortSinksTheFoldedHarmonicThatSetsTheAliasFloor`

`// The 13th harmonic of 15 kHz folds to 3 kHz at 192 kHz.`

The mechanism task 9 diagnosed, isolated: at the shipping 4x run rate of 192 kHz the 13th
harmonic of a 15 kHz tone lands on 195 kHz and folds to 3 kHz, already in band before any filter
can act. 16384 samples at 192 kHz put both 15 kHz and 3 kHz on exact bins, so the two lines are
read without a window and without leakage between them.

### `AdaaDiodePairPortResetDropsItsSampleIntervalHistory`

`// A port with no history stamps the plain pair current.`

A port with no history has no interval to average over, so it stamps the plain pair current; the
node's reset() leans on that to restart click-free. The pair current is i(v) - i(-v) and the
port splits it evenly across the two branches, so the difference of the two stamped currents is
what has to come back.

### `decimate_to_run_rate`

`/// @brief Converts one signal from the LTspice grid to the shipping run rate.`

Converts one signal from the LTspice grid to the shipping run rate.

`Resampler` is the tree's committed arbitrary-ratio converter (DR1), which matters here because
1 MHz to 192 kHz is 24/125 and no halfband cascade reaches it. It is zero phase, so output time
t carries input time t and nothing has to be shifted back afterwards, and its kernel time-scales
with the ratio, so decimating moves its cutoff to 96 kHz and the reference arrives band-limited
to what a 4x model could represent in the first place.

The converter touches the reference and the stimulus, never a circuit output: both ports are
then driven by the same decimated stimulus and nulled against the same decimated reference, so
whatever colour it leaves below 10 kHz is common to both and cancels in the comparison the case
exists to make.

`// The block is compared on the replay of the stimulus.`

The reference window is the second half of LTspice's 40 ms run, so the block is walked through
the stimulus once and compared on the replay, exactly as the reference-rate case argues.

### `DkTsClipBlockBothPortsNullAgainstTheReferenceDecimatedToTheShippingRunRate`

`// Both ports null against LTspice decimated to 192 kHz.`

The decision measurement for D6c C2. The reference-rate case nulls at 1 MHz, where a clipped
corner carries almost no aliasing and antialiasing can only spend its half-sample delay and its
corner attenuation; it therefore says nothing about whether antialiasing helps at the rate the
pedal actually runs. This case asks the question that decides: decimate LTspice's 1 MHz
transient to the shipping 192 kHz, so the reference is the analogue circuit band-limited to what
a 4x model can represent, then null both ports against it there.

Whichever port comes back closer is the one that models the circuit better at the rate it ships
at. The verdict and its numbers are in spec 2026-08-26-aaa-audio-quality-design.md, Model
corrections (D6c C2).

`// Which port wins is printed, not asserted.`

Both ports must stay recognisably the same circuit; which one wins is what the printout reports
and the spec records, and it is not asserted here in either direction.

### `DkTsClipBlockBothPortsNullAgainstTheConvergedModelOnTheToneThatAliases`

`// The reference is the block's own model at 1.92 MHz.`

The companion to the decimated-LTspice case, and the reason that one cannot decide alone:
ts_clip's stimulus is a 1 kHz sine, and 192000 / 1000 is an integer, so at the shipping rate
every folded harmonic of that tone lands exactly on a harmonic of it. The stimulus is blind to
the aliasing antialiasing exists to remove, and can only charge it for its cost.

This case puts the question on the tone that does alias. The reference is the same block's own
equations at 1.92 MHz, where the 13th harmonic of 15 kHz is nowhere near Nyquist, then decimated
to 192 kHz by the same converter - a converged solution of the model rather than of the circuit,
which is the right reference for asking which discretisation is closer to the answer it is
trying to compute.

`// Both candidates are trimmed by the same amount.`

The head of the decimated reference carries the converter's start-up, and both candidates are
trimmed by the same amount, so neither is favoured.

`// The two-tap mean is a delay and a low-pass.`

null_test aligns an integer lag and a gain, so antialiasing's half sample survives it, and the
antialiased port is also offered the reference delayed by half a sample. Read that third number
carefully: the two-tap mean below is a delay AND a low-pass, cos(pi\*f/fs), which is -0.26 dB at
15 kHz but -2.6 dB at 45 kHz. Antialiasing's error is corner attenuation, so this filter
attenuates the reference's corner too and the confound runs in antialiasing's favour. It is
printed because it still loses under a handicap that helps it, which strengthens the verdict; it
does not separate delay from attenuation, and nothing here should be read as doing so. That
separation would want a Thiran allpass or a linear-phase term in the frequency domain.

`// These are regression floors, not acceptances.`

Regression floors on the measured numbers, not acceptances. Which port is closer is the decision
this case exists to make, and it is recorded in the spec rather than asserted here.

## `test_tone_stage.cpp`

### `DkTsToneBlockResponseMatchesItsClosedForm`

`// The 0.25 dB tolerance is the sweep's own accuracy.`

Closed form of the corrected network, in dB:

```text
       100 Hz    1000 Hz   10000 Hz
0.1   -1.3471    -6.2987   -22.5431
0.5   -0.9946    -4.5852   -22.1373
0.9   -0.6948    -2.2872   -19.7318
```

The old netlist read -30.43 dB at 100 Hz and -34.38 dB at 1 kHz at noon, and moved level rather
than corner with the knob; these numbers check that the correction is in the block.

The 0.25 dB tolerance is the sweep measurement's own accuracy - D6a puts its worst error above
20 Hz at 0.17 dB - not the block's. The block is linear, so any drive level measures the same
response; -40 dBFS is what measure::frequency_response uses.

### `DkTsToneBlockClosedFormCostsUnderOneDecibelAtOneHundredHertzAtNoon`

`// The stage costs under 1 dB at 100 Hz at noon.`

The acceptance the old reference could not have met: its stage was an integrator with unity gain
at 1 / (2\*pi\*10k\*220n) = 72.3 Hz, so it read -30.43 dB at 100 Hz at noon. R.G. Keen on the
real stage: "there is no net signal loss through the tone control stage, a gain of 1". The three
factors at 100 Hz at noon are the filter -1.1957 dB, the gain leg +0.2887 dB and the output
network -0.0875 dB. Of the filter's loss, 0.8279 dB is R7 against the 10 kOhm bias return,
20\*log10(1 / (1 + 1k/10k)); the other 0.3679 dB is C5 and the wiper leg loading that node.

### `DkTsToneBlockClosedFormSweepsSixteenDecibelsAtTheLegsTurnoverFrequency`

`// The wiper leg's turnover sits at about 3.2 kHz.`

Keen puts the wiper leg's turnover where the 220 nF reaches 220 Ohm, "about 3.2KHz", and that is
where the knob has to do its work: at the + end the leg hangs on the passive filter and cuts, at
the - end it is the gain leg and boosts. The old netlist swung 19.08 dB of flat level here while
its corner moved 807 Hz to 760 Hz, which is a volume control, not a tone control.

`// At the bright end the stage passes +0.435 dB at 1 kHz.`

At the bright end the gain leg wins over the passive filter for about an octave around 1.1 kHz
and the whole stage passes more than it is given: +0.435 dB at 1 kHz, peaking at +0.465 dB at
1159 Hz. An integrator 30.43 dB down at 100 Hz cannot do that, so this is the single number that
separates the corrected netlist from the one it replaced.

### `DkTsToneBlockIsMonotonicBetweenThreeHundredHertzAndTwoKilohertzAtNoon`

`// The real network cannot notch above 723 Hz.`

The D5b/D6a tone control crossfaded a Butterworth low-pass against a Butterworth high-pass, both
at 720 Hz, so at noon the two cancelled and left a full null at 720 Hz. The real network cannot
notch: above the passive filter's 723 Hz corner it falls monotonically at every setting. The
closed form is checked at 1e-9 dB, which is what pins the shape; the measured sweep is checked
at 0.25 dB, its own accuracy, which is what pins that this block is that shape. A null like
D6a's is tens of dB deep and fails the loose gate as readily as the tight one.

### `DkTsToneBlockKnobMapsOntoTheReferencesSteppedRange`

`// TONE = 1 is bright and the mapping is the identity.`

TONE is measured from the op-amp's + input, where the wiper leg only cuts, so TONE = 1 is bright
and the mapping is the identity. Before the 2026-09-06 correction this ran backwards and stopped
at 0.1 and 0.9, because the old netlist muted at TONE = 1.

### `DkTsToneBlockPreparesAtEveryTonePositionIncludingBothEndStops`

`// Both end stops are ordinary settings.`

The old netlist muted at wiper position 1 - the pot shorted the wiper to the op-amp output and
the summing junction's equation collapsed - which is why tone_to_position stopped short of
its own track there. On the corrected network both ends are ordinary settings: at 0 the wiper leg
loads the passive filter, at 1 it is the whole gain leg, and neither is degenerate. This is the
case that fails first if someone reinstates the clamp.

### `DkTsToneBlockNullsAgainstTheLtspiceTransientReference`

`// One pre-roll pass matches LTspice's own start.`

tools/spice/export.py runs `tran 0 40m 20m 1u`: LTspice solves the DC operating point, runs 40
ms and saves the last 20 ms. This block sits at zero at DC - the 4V5 rail is an AC ground here -
so a DK block starting from zero state has LTspice's initial condition exactly, and the saved
window is 20 whole cycles of the 1 kHz stimulus. Replaying it ONCE therefore reproduces
LTspice's own first 20 ms, and the second pass is the comparison window. This is the clip
block's procedure, for the same reason.

Both signals are low-passed at 10 kHz by the same 4th-order Butterworth before comparison, so
the band-limiting cannot flatter either one. The block is linear, so a residual anywhere near
the -60 dB gate means a component value or a node index is wrong, not that a solver drifted.
