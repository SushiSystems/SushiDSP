# tests/foundation/circuits/stages/test_triode_stage

## `test_dk_netlist.cpp`

### `File scope`

`// The netlist lives in modules/foundation/circuit_blocks/include/SushiDSP/foundation/circuit_blocks/jcm_v1a_netlist.hpp.`

The netlist, its component constants and the triode glue live in
modules/foundation/circuit_blocks/include/SushiDSP/foundation/circuit_blocks/jcm_v1a_netlist.hpp: the RT-safety traps and the benchmark build the same block,
and one description of it is the only way the three stay the same circuit. The same namespace
carries modules/foundation/spice_reference/include/SushiDSP/foundation/spice_reference/spice_block_compare.hpp's spice_path, band_limit and interpolate_magnitude,
which the Tube Screamer block tests use unchanged.

### `DkTriodeStageSettlesAtTheHandComputedBiasPoint`

`/// @brief Tests that the jcm_v1a stage settles at the hand-computed bias point.`

Spec D6b.4 acceptance: the operating point of the jcm_v1a stage.

Hand solution (independent of DkCircuit, two-variable reduction plus bisection on Ip): Kirchhoff
gives Vgk = -Ip\*Rk and Vpk = 360 - Ip\*(Rp + Rk) with Rk = 2.7k, Rp = 100k, so the whole bias
reduces to the scalar fixed point Ip = koren(-Ip\*Rk, 360 - Ip\*102.7k). koren(...) - Ip is
strictly decreasing over 0 to 3.4 mA (it falls from 11.3 mA at Ip = 0 to zero before 1.7 mA), so
the root is unique; bisection to machine precision gives

```text
Ip = 8.555837e-4 A,  Vk = 2.3101 V,  Vpk = 272.132 V,  Vp = 274.442 V.
```

The brief's advisory band (0.8 V \< Vk \< 2.2 V) was derived for a 1.5k cathode resistor; with
the block's actual 2.7k and a 360 V supply the correct cathode voltage is 2.31 V, and the hand
solve above says so without reference to any DkCircuit output. The assertions below therefore
pin the hand-computed fixed point directly, which is strictly stronger than the advisory band:
any stamp sign error moves the operating point far further than these tolerances allow.

### `DkTriodeStageNewtonConvergesOnNinetyNinePointNineNinePercentOfSamples`

`// The stimulus set is a sweep and three bursts at 384 kHz.`

Spec D6b.4: \<= 0.01 % non-converged, zero NaN, over the full stimulus set. The set is a
2-second sweep 20 Hz -> 20 kHz at -12 dBFS scaled to volts by K_FULL_SCALE_VOLTS, plus a 1 kHz
burst at 0.05 V, 0.5 V and 5 V (well past grid conduction), all at 384 kHz.

### `DkTriodeStageNullsAgainstTheLtspiceTransientReference`

`// Only the last of K_PRE_ROLL_PASSES passes is compared.`

The exporter saves one 20 ms steady-state block per setting. The output high-pass formed by Cc
(0.022 uF) into the \~1 MOhm pot has tau \~ 22 ms, so driving that block once from the quiescent
bias leaves a turn-on DC transient around -43 dB that the plan's 5 ms discard nowhere near
removes. The block is 20 whole cycles of the 1 kHz stimulus, so it repeats seamlessly: it is run
K_PRE_ROLL_PASSES times to give over 160 ms (more than seven time constants, decaying the transient
past -100 dB) and only the final pass is compared, which is why no leading samples are discarded
from it.

### `DkTriodeStageMatchesTheLtspiceAcSweepWithinHalfADecibel`

`// -40 dBFS keeps the stage small-signal.`

frequency_response drives the node at -40 dBFS, which keeps the stage small-signal - the regime
an LTspice .ac analysis linearises around.

`// measure::render calls node.prepare() itself.`

measure::render calls node.prepare() itself, so the derivation and the bias settle happen there;
prepared() is checked afterwards rather than preparing twice.

## `test_behaviour.cpp`

### `K_SOURCE_RESISTANCE`

`// These constants are the JCM800's V1a-shaped stage.`

The JCM800's V1a-shaped stage: a 10 kOhm source into a 1 MOhm grid leak, biased 1.5 V below the
cathode, gain 60, with a 194 / 115 V plate swing.

### `TriodeStageGridFollowsTheSourceDividerBelowTheCathode`

`// 3 V in is 2.9703 V of grid excursion.`

Below the cathode the Norman law's current is nanoamperes, so the grid sits on the Rg / (Rs +
Rg) divider of the input: 3 V in is 2.9703 V of excursion.

### `TriodeStageGridConductsOnTheNormanLaw`

`// The root is checked against an independent bisection.`

The solver's root is the root of the KCL the header states, checked against an independent
bisection at three inputs: below the 0.5 V knee, on it, and well past it, where the conducting
slope of 150 kOhm against the 10 kOhm source pulls the grid 0.45 V off the divider (8 V in:
7.554 V of excursion against 7.921 V linear).

### `TriodeStagePlateSwingBoundsBothSidesAsymmetrically`

`// The two sides differ by the swing alone.`

Cutoff side: 9.9 V of excursion is a linear plate of 594 V, and 194 \* tanh(594 / 194) is 193.15
V. Conduction side: the grid rises to 7.55 V of excursion, a linear plate of -453 V, and -115 \*
tanh(453 / 115) is -114.99 V. The two sides differ by the swing alone.

### `TriodeStageCutoffIsThePlateCeiling`

`// No solver fence holds the grid short of the ceiling.`

No solver fence holds the grid short of the ceiling: at -30 V in, the 5150's V5A stage reaches
its 90.742 V headroom to the supply, where a 4.136 V fence held it at 74.3 V
(the grid knee antialiasing design spec in the version history section 3).

### `AntiderivativeAntialiaserInputMapInvertsTheGridSolve`

`// g(u) composed with the bisected solve is the identity.`

g(u) is the input the KCL says puts the grid at u, so composing it with the bisected solve of
that KCL returns the input: the map the divided difference divides by is the stage's own.

### `AntiderivativeAntialiaserAntiderivativeDifferentiatesBackToTheTransfer`

`// dG/du equals bound(-gain u) times dg/du everywhere.`

G is the antiderivative of the plate output with respect to the input, so dG/du equals
bound(-gain u) times dg/du at every excursion, across the knee and past both plate ceilings. The
bound is the table's cubic Hermite error, measured at 7e-9 of the swing.

### `AntiderivativeAntialiaserTransferMatchesThePlateBoundBetweenKnots`

`// Between knots only the Hermite slope error remains.`

G'(u) / g'(u) is what the ill-conditioned branch returns; between the table's knots it may
differ from bound(-gain u) only by the Hermite interpolant's slope error.

### `AntiderivativeAntialiaserDividedDifferenceIsTheMeanOfTheCompositeTransfer`

`// The divided difference of G is the mean transfer.`

Over an input interval the divided difference of G equals the mean of the composite transfer,
checked against Simpson's rule on the bisected transfer for intervals that sit inside the knee,
cross it, and cross both plate ceilings.

### `AntiderivativeAntialiaserIsContinuousWhereConsecutiveSamplesNearlyCoincide`

`// Near-equal samples fall back to the midpoint transfer.`

The divided difference loses its precision as g(u1) - g(u0) shrinks, so under
K_MINIMUM_INPUT_DIFFERENCE_VOLTS it gives way to the transfer at the midpoint. Two identical samples
and two a float ulp apart must return the plate bound, and sweeping the difference through the
threshold must leave no step larger than the float output's own quantisation.

### `AntiderivativeAntialiaserResetDropsItsPreviousSample`

`// The first sample after reset() is the plain transfer.`

Without a previous sample there is no interval to average over, so the first sample after
reset() is the plain transfer at that excursion; the stages lean on that to restart click-free.

### `TriodeStageOutputIsTheAntialiaserOfItsOwnGridSolve`

`// The stage's output is its antialiaser's output.`

The stage's plate output is its antialiaser fed with the grid excursion its solver found: the
same excursions through a bare antialiaser reproduce the stage sample for sample, to the Newton
solver's four-iteration tolerance.

## `test_output.cpp`

### `TriodeStageAliasFloorAtThreeAndTwelveTimesItsWindow`

`// V5A alone folds under -60 dB at 6 V and -40 dB at 24 V.`

DA5 spec section 11's first acceptance: the 5150's V5A stage alone in an 8x region, its 2.0 V
window driven 3x and 12x past, folds under -60 dB and -40 dB. On the Norman grid law it measured
-81.2 dB at 6 V and -53.7 dB at 24 V on 2026-09-07, and -102.4 and -79.8 dB once the stage's
composite transfer was antialiased (DA5 part 4); the room-temperature Shockley knee read -37.3
dB and -13.7 dB at the same drives.
