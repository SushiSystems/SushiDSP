# tests/foundation/circuits/stages/test_pentode_stage

## `test_behaviour.cpp`

### `PentodeStageSaturatesInsideItsPlateSwing`

`// The plate is bounded by the tube's swing on each side.`

The plate is bounded by the tube's own swing on each side: toward the supply by the idle
current's drop across the reflected load, toward the bottom by the rest of the datasheet's
full-signal swing. A grid driven far past cutoff or far into conduction reaches the tanh ceiling
and nothing else.

### `PentodeStageIsNearlyLinearForSmallInput`

`// The grid sits at 0.867 of the input inside the window.`

Inside the window the grid draws no current, the 220 kOhm leak against the 33.8 kOhm source sets
the grid at 0.867 of the input, and the plate reads that times the gain.

### `PentodeStageGridConductsAtTheCathodeNotAtTheBiasPoint`

`// The grid law is referenced to the cathode.`

The grid law is referenced to the cathode: the whole |Vk| window is clean, and the grid only
draws current once the input has carried it past 0 V. A +30 V swing on a -38 V grid comes back
as the linear plate answer through the tanh, to the float; the same swing on a -20 V grid has
crossed its window, the 150 kOhm conducting slope loads the 33.8 kOhm source, and the plate is
held 4.28 V short of that answer (measured 2026-09-07).

### `PentodePairIsOddSymmetricInsideTheWindowAndABiasDifferenceMovesTheHandover`

`// The composite is odd while both grids stay in window.`

The push-pull composite el34_a(u) - el34_b(-u) is odd in u while both grids stay inside their
windows: the two tubes are the same function of their input. Since the grid law is referenced to
the cathode, two halves with different bias voltages have different windows and the composite
acquires an even part once the drive reaches the nearer window; a cathode-bias difference is a
real handover asymmetry, where the bias-referenced stage of DA4 cancelled it exactly. Measured
2026-09-07: an even part of exactly 0 at equal bias, and 3.411 V for -38 V against -25 V at 60 V
of drive, on an odd reference of 183.5 V.

### `PentodeStageAntialiaserAntiderivativeDifferentiatesBackToTheTransfer`

`// dG/du = bound(-gain u) dg/du on the EL34's table.`

The EL34's knee sits 38.5 V above rest and its plate reaches 280 V toward the bottom, so the
identity the triode's antialiaser keeps, dG/du = bound(-gain u) dg/du, is checked on the power
tube's own table.

## `test_output.cpp`

### `PentodeStageAliasFloorInsideAndAtItsWindow`

`// The EL34 folds under -80 dB at 8 V and -50 dB at 38 V.`

DA5 spec section 11's third acceptance, in the real volts the stage now takes: the 2203's EL34
alone in an 8x region, driven to the 8 V that one model volt of the old stage stood for and to
its whole 38 V window, folds under -80 dB and -50 dB. Measured 2026-09-07: -88.1 dB at 8 V and
-55.1 dB at 38 V, and -101.2 and -84.4 dB once antialiased (DA5 part 4), where the
bias-referenced stage read -49.7 dB at one model volt.
