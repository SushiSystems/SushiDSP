# Known issues

Defects the team has measured, understood, and deliberately left open, with the test that
documents each one and what closes it. This is not a bug tracker for the unmeasured; an entry
here has a figure, a mechanism, and a named exit.

## Alias floor at 8× after volt-true grid staging

The JCM800 measures −6.7 dB through `measure::alias_floor()` at preamp 1.0 and a −12 dBFS input,
against the −33.8 dB bound (−43.4 dB before DA4). DA4 removed the level-staging devices that used to keep every grid off
its clipping bound, so each stage now clips at the level its own circuit sets, and the 8×
oversampled region folds the extra harmonics that clipping produces. The same mechanism applies
to all four amplifiers, since all four share `TriodeStage` and lost their staging in the same
phase. The ledger's readings point at which stage folds: adding the two documented circuit
losses *raised* the floor, from −12.5 dB to −3.0 dB, so the harmonics come from stages leaving
hard limiting and producing more distinct products, not from clipping more deeply. Documented by
`Regression_Dsp.Jcm800AliasFloorImprovedAtLeast30dBOverD5b`
(`tests/functional/unit/test_measure.cpp`), skipped with the measured figure printed. Closes with
DA5's antialiasing work — antiderivative antialiasing in `TriodeStage`, or 16× oversampling.

## Mesa Mark IIC+ has no clean setting

The dossier's §2 places an EFFECTS SEND / RETURN network between V2A's plate and V2B's grid,
and its §3.7 gives no value for that link, so the model has no loop network at all — neither the
series path nor the shunt one, of which the missing return capacitor's 7.23 Hz corner is only a
part: V2A's plate drives
V2B's grid window about twenty-one times past it, and the node limits at every input level with
no clean end on the knob. Three tests are skipped, not weakened: `MesaMark2cHasACleanEndAtMinimumGain`,
`MesaMark2cToneStackSitsBeforeTheLeadStages` and `MesaMark2cEqBandBoostsItsCentre`
(`tests/functional/unit/test_mesa_mark2c.cpp`), each printing what it measures today against
what it will re-assert. Closes when the send/return network is read off the schematic — dossier
open questions 19 and 20.

## Diezel VH4's phase inverter and power section are a JCM800 stand-in

The dossier establishes nothing about the VH4's phase inverter or power section (§5: "nothing
established"), so the module keeps the JCM800's long-tailed-pair degeneration and power-tube
grid figures in their place rather than a VH4-specific derivation. Every number downstream of
that link is borrowed, not measured from the VH4 itself. Closes when a VH4 power-amp schematic
becomes available.
