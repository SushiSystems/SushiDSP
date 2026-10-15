# tests/foundation/math/dk/test_dk_circuit

## `test_behaviour.cpp`

### `magnitude_at`

`/// @brief Returns the magnitude of the DTFT of a real impulse response at one frequency.`

Magnitude of the DTFT of a real impulse response at one frequency.

|H(e^{jw})| = |sum_n h[n] e^{-jwn}|. Evaluated directly rather than by FFT because the resonance
test needs magnitudes on an arbitrarily fine frequency grid, at full double precision, and the
impulse response is short.

### `DkCircuitInstantiatesEveryShapeD6cAndD6dNeed`

`/// @brief Tests that every DkCircuit shape D6c and D6d need compiles, prepares and steps.`

Compile-and-smoke gate for the three DkCircuit shapes D6c and D6d instantiate. Tasks 6 and 7 own
the numerical validation; this case only proves every shape compiles, prepares and steps.

`// This is the widest shape D6d instantiates.`

The widest D6d-style shape: four states, a two-branch nonlinear port, two sources (signal plus
supply rail), one probe, and a potentiometer to drive re-derivation.

### `DkCircuitRcLowpassMatchesTheBilinearTransformExactly`

`// Trapezoidal DK reproduces the bilinear transform.`

Node 1 = source, node 2 = output. R from 1 to 2, C from 2 to ground, probe node 2. Trapezoidal
DK must reproduce the bilinear transform of 1/(1 + sRC) exactly:

```text
Gc = 2C/T,  H(z) = (1 + z^-1) / ((1 + R*Gc) + (1 - R*Gc) z^-1)
```

With R = 1 kOhm, C = 100 nF, fs = 48000: Gc = 2\*1e-7\*48000 = 9.6e-3 S and R\*Gc = 9.6, so

```text
pole      = -a1/a0 = (R*Gc - 1)/(R*Gc + 1) = 8.6/10.6 = +0.811320754717
DC gain   = (1 + 1)/((1 + 9.6) + (1 - 9.6)) = 2/2 = 1 exactly
Nyquist   = (1 - 1)/(...) = 0 exactly
```

Note the sign: a lowpass pole must approach +1 as fs -> infinity, so the pole is (R\*Gc -
1)/(R\*Gc + 1) and NOT (1 - R\*Gc)/(1 + R\*Gc); the latter is a Nyquist pole. The -3 dB point of
the analogue prototype is 1/(2\*pi\*R\*C) = 1591.549431 Hz.

### `DkCircuitSeriesRlcResonatesWhereTheAnalogueCircuitDoes`

`// The magnitude peak sits at w0*sqrt(1 - 2*zeta^2).`

Source -> R -> L -> C -> ground, probing across C: H(s) = 1 / (1 + sRC + s^2 LC). With R = 100
Ohm, L = 10 mH, C = 100 nF:

```text
w0    = 1/sqrt(LC)            = sqrt(1e9)                 = 31622.776602 rad/s
                                                          -> f0   = 5032.921210 Hz
zeta  = (R/2)*sqrt(C/L)       = 50*sqrt(1e-5)             = 0.158113883, zeta^2 = 0.025
w_d   = w0*sqrt(1 - zeta^2)   = sqrt(1/(LC) - R^2/(4L^2)) = 31224.989990 rad/s
                                                          -> f_d  = 4969.611 Hz
w_pk  = w0*sqrt(1 - 2*zeta^2) = sqrt(1/(LC) - R^2/(2L^2)) = 30822.070014 rad/s
                                                          -> f_pk = 4905.485 Hz
```

The MAGNITUDE peak of a second-order lowpass sits at w0\*sqrt(1 - 2\*zeta^2), not at the damped
natural frequency w0\*sqrt(1 - zeta^2); the two differ by 1.3 % here, well outside the 0.5 %
tolerance, so the damped resonance is the wrong golden for a measured peak. Peak magnitude is
1/(2\*zeta\*sqrt(1 - zeta^2)) = 3.202563.

Run rate 384 kHz so trapezoidal frequency warping is negligible: the digital peak sits at
(2/T)\*atan(w_pk\*T/2) = 30805.6 rad/s, 0.053 % below the analogue peak - an order of magnitude
inside the tolerance, so the assertion is about the circuit, not the warping.

The response is measured as the DTFT of the impulse response rather than with
measure::frequency_response: the resonance is broad (Q = 3.16), so a 0.5 % frequency error costs
only 0.004 dB of magnitude. Locating the peak by argmax therefore needs a magnitude accuracy far
below the \~0.05 dB a swept-sine measurement delivers, and a sweep-based assertion here would be
measuring the sweep rather than the circuit.

`// 4096 samples are 53 time constants of the envelope.`

The envelope decays as exp(-zeta\*w0\*t), one time constant every 76.8 samples at 384 kHz, so
4096 samples is 53 time constants: the truncated tail is below 1e-23 of the peak.

### `DkCircuitDiodeClipperReachesTheTrueRootOfTheDiodeEquation`

`// The oracle is the bisected root of the diode equation.`

The circuit solves v = Vs - Rs\*Is\*(exp(v/Vt) - exp(-v/Vt)). Node 1 = source, node 2 = the
diode node, Rs from 1 to 2, and TWO antiparallel SiliconDiode branches from 2 to ground - a
pair, not a single symmetric element, so D6c can make it asymmetric later. Using SiliconDiode
with n = 1 and the same Vt reproduces that law exactly except for its -1 offsets, which cancel
between the two antiparallel branches.

D6b's DiodeClipperCircuit used to sit here too, as a second, independently written
implementation to agree with. D6c's plan (docs/archive/superpowers/plans/
2026-09-05-d6c-tubescreamer-on-dk.md, Task 10) retired it: it ran four fixed float Newton
iterations with no convergence test and drifted up to 2.8 mV from the root at high slew, so
agreeing with it was a weaker claim than agreeing with the equation. Bisecting the scalar
equation itself, in double precision, is an independently written \*root\* rather than an
independently written approximation, and it is the oracle this case now compares against
exclusively.

`// |residual| / slope bounds the distance to the root.`

The scalar equation, evaluated in double precision, and its slope. |residual| over the slope is
a first-order bound on the distance from a candidate voltage to the equation's true root, which
is the oracle: it compares the DK solution against the equation itself rather than against
another approximation of it.

### `DkCircuitPotentiometerChangesTheResponseAndCostsNothingWhenUnchanged`

`// The DC gain from source to wiper is the position.`

A 10 kOhm pot as a voltage divider into a 100 nF cap. The track runs from ground (the pot's a
terminal) to the source (its b terminal), so the a-to-wiper section is position\*10k and the DC
gain from the source to the wiper is exactly the position: a DC input of 1 V settles to p at the
output. Positions 0.25, 0.5, 0.9 give 0.25, 0.5, 0.9.

`// tau is at most 250 us, so 40000 samples settle it.`

The worst-case source impedance seen by the cap is the parallel combination of the two sections,
at most 2.5 kOhm, so tau \<= 250 us. 40000 samples is 0.83 s, over 3000 tau.

`// K_MINIMUM_POT_OHMS floors a shorted section at 1 Ohm.`

Both end stops must still derive and produce finite output. K_MINIMUM_POT_OHMS floors the shorted
section at 1 Ohm rather than 0, so the divider reads 1/10001 and 10000/10001 instead of exactly
0 and 1; that 0.01 % is the floor, and it is what keeps the augmented nodal matrix non-singular
at the stops.

### `DkCircuitNeverEmitsANonFiniteSampleAndCountsItsFallbacks`

`// One inf sample must not poison the capacitor's state.`

An RC lowpass with a linear nonlinear branch across the cap, so there is a state a poisoned
sample could corrupt. A nonlinearity that deliberately returns inf on one sample must not poison
that state: the trajectory has to return to that of an identical circuit that was never
poisoned.

`// The fallback costs one sample's worth of state update.`

The fallback holds the previous state rather than advancing it, so the poisoned sample costs
exactly one sample's worth of state update - a bounded step off the reference trajectory, not a
poisoned state. The bound below is the size of one sample's output change on this stimulus
(\~0.04 V at 300 Hz through a 100 us time constant); measured 0.022. The two then reconverge at
the circuit's own pole, 0.811 per sample here, so 190 samples later the deviation is 0.811^190 =
6e-18 of that step, i.e. gone. A state that had actually been poisoned would have stayed
non-finite, or diverged, instead.

### `DkCircuitClampsItsStateUnderAnAbsurdInputAndStaysFinite`

`// The output stays finite and the state stops at a limit.`

1e30 V into the RC lowpass. The output is allowed to be enormous - it is a linear circuit driven
by an enormous source - but it must stay finite, and the state must stop at the configured limit
rather than running away.

## `test_op_amp.cpp`

### `DkCircuitIdealOpAmpInvertingAmplifierHasGainMinusRfOverRin`

`// V(3) = -(Rf/Rin) V(1) = -10 V(1) at every frequency.`

Node 1 = source, 2 = the summing junction, 3 = the output. The nullator senses ground against
node 2, so V(2) = 0; the norator drives node 3. KCL at node 2 is then -V(1)/Rin - V(3)/Rf = 0,
so V(3) = -(Rf/Rin) V(1) = -10 V(1) at every frequency.

### `DkCircuitIdealOpAmpNonInvertingAmplifierHasGainOnePlusRfOverRg`

`// 1 + 51000/4700 = 11.851063829787234 exactly.`

The Tube Screamer clip stage with its capacitors removed and the drive pot at zero: 1 +
51000/4700 = 11.851063829787234 exactly.

### `DkCircuitIdealOpAmpLowPassMatchesTheBilinearTransformExactly`

`// The pole is 95/97 and the DC gain is -10 exactly.`

Inverting stage with Rf in parallel with Cf: H(s) = -(Rf/Rin) / (1 + s Rf Cf). Trapezoidal DK
must reproduce its bilinear transform exactly. With Rin = 10 kOhm, Rf = 100 kOhm, Cf = 10 nF, fs
= 48000: a = 2 fs Rf Cf = 2\*48000\*1e-3 = 96, so

```text
H(z) = -(Rf/Rin) (1 + z^-1) / ((1 + a) + (1 - a) z^-1)
```

with pole (a - 1)/(a + 1) = 95/97 = 0.979381443298969 and DC gain -10 exactly. The analogue
corner is 1/(2 pi Rf Cf) = 159.154943 Hz.

### `DkCircuitRejectsAnOpAmpWithNoFeedbackPath`

`// Without a feedback path the system is rank deficient.`

An op-amp whose output does not reach its own input leaves the augmented system rank deficient.
prepare() must say so rather than derive nonsense.

## `test_rt_safety.cpp`

### `File scope`

`// The netlist lives in modules/foundation/circuit_blocks/include/SushiDSP/foundation/circuit_blocks/jcm_v1a_netlist.hpp.`

The netlist, its component constants and the triode glue live in
modules/foundation/circuit_blocks/include/SushiDSP/foundation/circuit_blocks/jcm_v1a_netlist.hpp, shared with the D6b stage test and the benchmark.

### `PotentiometerUpdatePerformsNoHeapAllocation`

`// A pot move rebuilds the DK matrices on the audio thread.`

The one audio-thread path that re-runs a derivation (plan Ruling 3): a pot move invalidates the
DK matrices and update_matrices_if_dirty() rebuilds them at the top of the next block. If
anything in that rebuild allocated, Ruling 3 would have to be replaced by a control- thread
rebuild plus an RtSwapSlot hand-off, so this case is the one that decides it.
