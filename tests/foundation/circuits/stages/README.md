# tests/foundation/circuits/stages

## `test_long_tailed_pair.cpp`

### `LongTailedPairDrivesItsTwoPlatesInOppositeDirections`

`// Matched amplitudes are not asserted.`

Whether the two amplitudes come out matched is not asserted: each grid sits behind its own
source and leak, and dossier question 6 leaves the pair's DC point open.

## `test_output_transformer_stage.cpp`

### `OutputTransformerStageSecondaryCarriesNoMean`

`// A transformer's secondary passes no mean.`

A push-pull pair driven through asymmetric clipping delivers a square whose two halves are not
equally long, and a saturating core compresses the taller half more than the shorter, so the
saturated waveform carries a mean even after the rolloff has removed the square's own. A
transformer's secondary sees d(flux)/dt and passes no mean at all (DA5 spec section 14).

Stimulus: a 220 Hz square of amplitude 2.2 at 40 % duty, two seconds at 48 kHz, the mean taken
over the second second. With the saturation as the last element the stage read -0.063500
(2026-09-07); with the secondary's mean removed it reads +0.000005.

## `test_pentode_stage.cpp`

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

## `test_power_supply_sag_stage.cpp`

### `PowerSupplySagStageRecoversTowardUnityAfterSilence`

`// The sag envelope decays over silence.`

With zero input the sag envelope should decay, so a subsequent unity-input sample should be
closer to unity than the deeply-sagged sample was.
