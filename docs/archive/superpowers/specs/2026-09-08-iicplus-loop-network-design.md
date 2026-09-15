# The Mark IIC+ effects loop network and Master 1's place

**Status:** Designed 2026-09-08, not built. Two decisions for the maintainer in section 8; the
implementation is one task once the tree is free (section 10). Evidence in
`.superpowers/sdd/amp-gain-staging/iicplus-loop-network.md`. Closes dossier open questions 19 and
20 and opens 21 to 24 (`docs/reference/research/2026-09-06-mesa-mark-iic-plus-design-dossier.md`
§3.6, §3.6a, §3.7, §8). Follows DA5 (`2026-09-07-grid-knee-antialiasing-design.md`).

## 1. Goal

The IIC+ module has a clean end on its knobs, its Lead Master and Master 1 do what the panel
says, and the three skipped tests in `tests/functional/unit/test_mesa_mark2c.cpp` re-assert
their figures against a link the schematic draws rather than one the model invented.

## 2. Why

`modules/mesa_mark2c` connects V2A's plate to V2B's grid through a 0.022 uF stand-in and nothing
else, because dossier §3.7 printed no value for the effects loop. V2A's plate, ceilinged at 87 V,
meets a 4.14 V grid window, the node limits at every input level, and three tests are skipped:
`MesaMark2cHasACleanEndAtMinimumGain` (41.5 % THD at Lead Drive 0.02 and −40 dBFS against a
10 % bar), `MesaMark2cToneStackSitsBeforeTheLeadStages` (−0.09 pp of treble effect against 5 pp)
and `MesaMark2cEqBandBoostsItsCentre` (0.009 dB of lift against 17 dB). The maintainer's word for
the amp was "broken and unplayable".

The network exists and is on the schematic. The 1983 factory sheet (dossier S1) draws every part
of it; its `47K`, `150K` and `4.7K` glyphs were mis-assigned on 2026-09-06, and the Lead Master was
put ahead of V2A's grid where it is not. jrb32's traced schematic (S8), confirmed on an RP10A
chassis, gives the same network with reference designators, and three Boogie Board threads (S10)
and the Mark III manual (S9) corroborate its topology and level. Nothing here is a stand-in
except the pot's taper and the LDR's on resistance (section 5).

A probe compiled against a copy of the module (section 6) measured what the network does inside
the model, and it found a second fault behind the first: with V2B relieved, the phase inverter's
grid still receives 21 V because the model applies Master 1 after the power section, where it
cannot take drive out of the inverter. The schematic puts Master 1 at node F, ahead of the EQ and
the inverter. The network alone leaves the clean-end test at 40.6 % THD; the network with Master 1
at node F brings it to 9.6 % or 0.2 % depending on one disputed resistor (section 8). This spec
therefore carries both changes, and states which is established and which is a modelling
decision.

## 3. The network, as established

Factory tube names. Sources and confidence in dossier §3.6a.

```
V2A plate ─ 47K (R105) ─ 0.047u (C9) ─ W
W ─ 250K LEAD MASTER, a rheostat ─ LDR4 (on in Lead mode) ─ ground
W ─ 150K (R102) ─ P
P ─ 4.7K (R101) ─ ground
P ─ 2.2K (R12) ─ SEND tip;  RETURN tip ─ V2B grid ─ 47K (R103) ─ ground
```

The jacks normal SEND to RETURN with nothing plugged in, so P and V2B's grid are one node through
the 2.2K. The reverb take-off at W and its return at P are not modelled. The 120 pF from V2B's grid
to cathode corners at 224 kHz into the 5.91K source and is omitted.

V2A itself changes with the network, because the network is its load. The cathode is an
unbypassed 1.5K (S1 draws no capacitor; S8 draws the 15 uF dashed as the ++ mod; S10 says the ++
mod adds it), the grid shunt is 680K ∥ 47 pF, and the Lead Master no longer sits ahead of the
grid, so the grid's source resistance is the lead output divider's Thevenin, 220K ∥ 100K =
68.75K.

Derived on the Koren 12AX7 of `docs/reference/spice/models/12ax7_koren.sub`, Lead Master at
maximum, loop normalled (dossier §3.6, §3.6a; script and output in the report):

| Quantity | Value |
|---|---|
| V2A idle | 1.2902 mA, Vk 1.935 V, Va 263.0 V; local gm 1.954 mA/V, r_a 46.85K, mu 91.56 |
| V2A AC load | 100K ∥ (47K + 250K ∥ (150K + 4273)) = 58.75K |
| V2A gain, unbypassed | 91.56 x 58.75 / (58.75 + 46.85 + 92.56 x 1.5) = 22.005 |
| V2A plate swing | 75.795 V up (1.2902 mA x 58.75K); 114.171 V down to Vgk = 0 on the AC load line |
| V2A output resistance | 100K ∥ (46.85K + 138.8K) = 65.0K |
| Coupling corner | 1 / (2 pi x 0.047 uF x (65.0K + 142.4K)) = 16.33 Hz |
| Plate to V2B grid | 0.6699 x 0.02770 x 0.9533 = 0.01769, −35.04 dB |
| Plate to SEND | 0.01856, −34.63 dB; 1.41 V peak, 1.0 V RMS at full swing |
| V2B source resistance | 47K ∥ (2.2K + 4.7K ∥ (150K + 250K ∥ (47K + 65.0K))) = 5.909K |
| V2B grid drive at V2A's full swing | 1.34 V peak up-side, 0.75 V down-side, against a window of +1.26 / −2.79 V |

The Lead Master law and V2A's load against the knob are tabulated in dossier §3.6a; the knob's
top half moves the level by 1.9 dB and its bottom fifth by 26 dB.

## 4. The change to `modules/mesa_mark2c`

Constants in `src/MesaMark2c.cpp`, each carrying its dossier section:

| Constant | Today | Becomes | Dossier |
|---|---|---|---|
| `kV2aGain` | 61.5 | 22.005 | §3.6 |
| `kV2aCathodeBias` | −1.5 | −1.9353 | §3.6 |
| `kV2aSourceResistance` | 62500 (Lead Master wiper) | 68750 (220K ∥ 100K); 3250 under question 21 | §3.5, §3.6 |
| V2A's grid leak | `kTriodeGridLeak` 1 M | a new `kV2aGridLeak` 680000 | §3.6 |
| `kV2aPlateSwing` | {100, 163.581} | {75.795, 114.171} | §3.6 |
| `kV2bSourceResistance` | 100000 | 5909 | §3.6a |
| `kV2bLoopReturnCouplingHz` | 7.2343 (0.022 uF, a stand-in) | 16.33 (C9 0.047 uF) | §3.6a, q.19 |
| new `kLoopSeriesOhms`, `kLoopShuntToWOhms`, `kLoopWToPDivider`, `kLoopPToGridDivider` | — | 47000, 154273, 0.02770, 0.9533 | §3.6a |
| new `kLeadMasterPotOhms` | — | 250000 | §3.6a |

The process order moves the Lead Master. Today `v2a_input = lead_output_divider(...) *
lead_master_gain`; the pot leaves that line, and the network takes the sample after V2A:

```
v2a_out    = v2a_(v2a_grid_miller_(v2a_input))
loop_gain  = (z_w / (47000 + z_w)) * 0.02770 * 0.9533,   z_w = pot ∥ 154273,  pot = 250000 * taper(lead_master)
v2b_in     = v2b_loop_return_coupling_(v2a_out) * loop_gain
```

`loop_gain` is computed once per block from the smoothed `lead_master`, as `lead_master_gain` is
today. At `lead_master` 0 the pot is 0 Ω and `loop_gain` is 0, the same hard mute
`audio_taper(0)` gives today, so `MesaMark2cThdDoesNotFallAsLeadDriveRises`'s reason for starting
at 0.05 still holds.

V2B's Miller corner is set from the resistance that drives the grid; today that is
`kV2aPlateLoadOhms` (100K) and it gives 11.6 kHz. With the network the driving resistance is
5.9K and the corner lands near 200 kHz, above the oversampled Nyquist. The implementation keeps
`miller_corner_hz(kV2bGain, kV2aPlateLoadOhms)` and says in its comment that the pole is a
placeholder for a corner the band does not reach; deleting the filter is the cleaner choice and
is the maintainer's to make.

**Master 1 at node F** (section 8, decision 1). `master_gain` multiplies the sample between
`v2b_coupling_` and `equalizer_bank_`, inside the oversampled region, and the post-downsample
line keeps only the presence shelf. `kMasterUnityGain` is re-pinned by the existing procedure
(a −12 dBFS 82 Hz sine at default knobs out at −6.000 dBFS); the probe's bisection gives
**0.0016617** with the 100K lead output shunt and **0.0038178** with 3.3K. The header comment on
the constant records the move and the previous four values as it does today.

## 5. What is a stand-in, labelled

- **The Lead Master's taper.** The pot is a rheostat (S1 and S8 both draw it so); its taper is
  not stated by any source. The model uses `audio_taper` (n·√n), the same law as the other pots,
  and the module README says so. Dossier §8 question 14.
- **The LDR4's on resistance.** Not read (dossier question 23). The model's floor is the pot's 0 Ω,
  a mute; a real IIC+ leaks a few hundred ohms' worth of signal at Lead Master 0.
- **V2A's load held at the pot's maximum.** The rheostat changes V2A's AC load from 58.75K to
  32.3K across its travel, which moves the gain from 22.0 to 13.6 and the swing from 75.8 to
  41.7 V (dossier §3.6a table). `TriodeStage` takes one gain and one `PlateSwing` at construction,
  so the model holds V2A's constants at Lead Master maximum and lets `loop_gain` carry the whole
  law. The error is under 4.2 dB of drive at the knob's bottom, where the stage is 26 dB down
  anyway. Tracking the load would need a `TriodeStage` whose gain and swing move per block; that
  is a brick change and out of scope here.
- **The lead output shunt.** 100K is S1's reading and stays the model's; S8's 3.3K is dossier
  question 21. Section 8, decision 2.

## 6. What the probe measured

A copy of `MesaMark2c.cpp` and its header, patched with section 4 behind `LOOP_VARIANT` and
`MASTER_AT_F` macros, compiled with the toolchain against a snapshot of `include/`, the 24
sources of `sushi_dsp.lib` and `tests/measure/`, at 48 kHz, 256-frame blocks, the test file's own
stimuli. The full recipe, patch and outputs are in the report. Variants: 0 the tree as it
stands, 1 the network with the 100K shunt, 2 the network with the 3.3K shunt; suffix f applies
Master 1 at node F with `kMasterUnityGain` re-pinned to −6.000 dBFS.

| Measurement | 0 (today) | 1 | 2 | 1f | 2f |
|---|---|---|---|---|---|
| Clean end: THD, Lead Drive 0.02, −40 dBFS, bar < 10 % | 40.70 % | 40.62 % | 29.78 % | **9.62 %** | **0.20 %** |
| THD at Lead Drive 0.05 / 0.10 / 0.20, −40 dBFS | 40.7 / 40.7 / 40.9 | 40.8 / 40.6 / 40.9 | 38.9 / 40.6 / 41.2 | 26.3 / 33.6 / 36.1 | 3.0 / 15.1 / 32.6 |
| Tone stack: bright − dark, Lead Drive 1.0, 0.2 amplitude, bar > 5 pp | −0.06 pp | −0.08 pp | −0.06 pp | −0.11 pp | +0.03 pp |
| Tone stack at Lead Drive 0.05, 0.01 amplitude | — | — | — | +17.56 pp | +8.50 pp |
| Tone stack at Lead Drive 0.20, 0.01 amplitude | — | — | — | +2.59 pp | +24.16 pp |
| EQ: 979.5 Hz lift, slider +1, bar 17 ± 1 dB | 0.00 dB | 0.00 dB | −0.13 dB | 13.69 dB | 16.09 dB |
| THD does not fall as Lead Drive rises (0.05 to 1.0) | 40.8 → 40.7 | 40.7 → 40.7 | 40.9 → 40.6 | 36.2 → 36.2 | 40.4 → 42.0 |
| Lead Master 1.0 → 0.05, output at Lead Drive 0.5, −12 dBFS | −12.4 → −12.4 dBFS | −12.4 → −12.4 | −12.4 → −12.3 | −9.5 → −28.3 | −5.7 → −25.4 |
| Full-scale clamp reached | yes | yes | yes | yes | yes |
| Grid excursions V2A / V2B / inverter at default | 16.3 / 27.9 / 21.1 V | 41.0 / 1.62 / 21.1 | 13.0 / 1.20 / 21.1 | 41.0 / 1.62 / 0.076 | 13.0 / 1.20 / 0.115 |
| Alias floor, default, −12 dBFS | −14.8 dB | −15.5 dB | −22.0 dB | −11.1 dB | −19.8 dB |
| Alias floor, Lead Drive 0.02, −12 dBFS | −22.3 dB | −30.0 dB | −31.9 dB | −41.6 dB | −33.9 dB |
| `kMasterUnityGain` for −6.000 dBFS | 0.2331434 | 0.2334318 | 0.2482159 | 0.0016617 | 0.0038178 |

Variant 0's alias floor reads −14.8 dB where DA5 part 3 recorded −0.1 dB; the snapshot carries
the other agent's uncommitted ADAA and double-`Biquad` work (`git status` at 00:23), so column 0
is today's tree, not the shipped commit.

## 7. Predictions

Read from the table; the arithmetic behind each line is section 3.

- **`MesaMark2cHasACleanEndAtMinimumGain`** passes only with Master 1 at node F: 9.62 % against
  the 10 % bar with the 100K shunt, 0.20 % with 3.3K. The network alone leaves it at 40.6 %,
  because the inverter's grid still receives 21 V. With 100K the margin is 0.4 pp, one build's
  rounding away from failing; with 3.3K it is 9.8 pp.
- **`MesaMark2cEqBandBoostsItsCentre`** asserts 17 ± 1 dB and reads 13.69 dB (1f) or 16.09 dB (2f).
  Neither is inside the bar at the test's setting (Volume 1, Lead Drive and Lead Master all 0.2,
  0.005 amplitude): the residual is the power section's compression at the level the re-pinned
  master delivers. With 3.3K the test is 0.9 dB short; with 100K, 3.3 dB. The test can be made to
  read the bank's full lift by measuring at −20 dB of Master 1, where the inverter is linear, or
  its bar can be widened; either is a test change, section 8.
- **`MesaMark2cToneStackSitsBeforeTheLeadStages`** does not pass in any variant at its own setting
  (Lead Drive 1.0, 0.2 amplitude): every stage from V3B on is a square there, treble or no treble,
  and THD sits at 36 to 42 % either way. That is the amplifier, not the model: 0.2 amplitude is
  −14 dBFS, a hot line level, into a chain of 110 dB. At Lead Drive 0.05 and 0.01 amplitude the
  same assertion reads +17.56 pp (1f) and +8.50 pp (2f); at Lead Drive 0.20 and 0.01, +2.59 and
  +24.16 pp. The test's premise holds once its operating point is on the knee; its parameters are
  the maintainer's, section 8.
- **`MesaMark2cThdDoesNotFallAsLeadDriveRises`** keeps passing in every variant (its 1 pp tolerance
  covers 1f's flat 36.2 % and 2f's 40.4 → 42.0 %), and its guard `previous > 20` holds.
- **Alias floor at the default setting** moves from −14.8 dB (today's tree) to −11.1 dB (1f) or
  −19.8 dB (2f). It gets worse with the 100K shunt because V2A's grid is driven 41 V past its
  window (68.75K source) and its fold now reaches the output unmasked by the inverter's own
  re-squaring; with the 3.3K shunt V2A's drive drops to 13 V and the floor improves by 5 dB. At
  Lead Drive 0.02 the floor is −41.6 or −33.9 dB. The −33.8 dB bar of DA5 section 11 stays unmet
  at the default in both.
- **The unity constant and the goldens.** `kMasterUnityGain` moves in every variant; with Master
  1 at node F it becomes 0.0016617 or 0.0038178. The regression goldens
  `mesa_mark2c_48k` and `mesa_mark2c_96k` move and are regenerated once, after the build.
- **The maintainer's complaint.** In 1f and 2f the Lead Master spans 19 to 20 dB of output, the
  Lead Drive spans 9.6 → 36 % or 0.2 → 42 % of THD at −40 dBFS, and the inverter runs clean at the
  default; the amp has a gain knob and a clean end.

## 8. Decisions for the maintainer

1. **Master 1 at node F.** Without it no variant has a clean end; with it "0 dB" on the Master
   1 control is a pot fraction of 0.0017 to 0.0038 by the same n·√n taper, which is where a real
   IIC+ sits when its power section is asked to stay clean at −6 dBFS out for −12 dBFS in. The
   alternative that keeps Master 1 after the power section is a fixed pad of the same value at
   node F, labelled a stand-in; it gives the same measurements and takes the control's meaning
   away. This spec recommends the move. The JCM800 keeps its own arrangement; nothing in this
   decision reaches it.
2. **The lead output shunt, 100K or 3.3K.** S1 against S8; dossier question 21. The clean end
   reads 9.62 % or 0.20 %, the EQ lift 13.7 or 16.1 dB, the alias floor −11.1 or −19.8 dB. This
   spec recommends keeping 100K until an ohmmeter reads R30, and says so in the README, because a
   value chosen for its test results is an invented number. If the maintainer prefers S8's
   hardware trace over S1's hand lettering, the change is one constant pair
   (`kLeadOutputDivider` 0.0148, `kLeadOutputDividerPoleHz` 65300) and `kV2aSourceResistance`
   3250, and the predictions are column 2f.
3. **The two tests' operating points.** `MesaMark2cToneStackSitsBeforeTheLeadStages` at Lead Drive
   0.05 and 0.01 amplitude, and `MesaMark2cEqBandBoostsItsCentre` measured with Master 1 at −20 dB
   or with its bar widened to the measured lift. Both are the tests describing the amplifier
   rather than the amplifier meeting the tests, and both need the maintainer's word.

## 9. Out of scope, stated

The reverb take-off and return; the send and return jacks as a real insert point; the LDR's on
resistance; a `TriodeStage` whose load follows the Lead Master; the 120K / 1M reading of V2B's
plate load and the Master pot (dossier question 22); the IIC++'s bypassed V2A.

## 10. Acceptance, and what implementing it takes

Acceptance: the three tests un-skipped and green at the operating points the maintainer chooses
under decision 3; `MesaMark2cThdDoesNotFallAsLeadDriveRises`, `MesaMark2cNeverExceedsFullScale
WithoutALimiter` and `MesaMark2cDefaultSettingsPeakNearMinusSixDbfsForAMinusTwelveDbfsInput`
still green; the goldens regenerated once; the alias floor at the default recorded in
`docs/reference/KNOWN_ISSUES.md` at its new value.

Files, one task, one build:

- `modules/mesa_mark2c/src/MesaMark2c.cpp`: the constants of section 4, the new loop constants,
  `loop_gain` per block, the Lead Master's move, Master 1 at node F, the V2B Miller comment.
- `modules/mesa_mark2c/include/SushiDSP/mesa_mark2c/MesaMark2c.hpp`: `kMasterUnityGain` and its
  comment; `set_lead_master`'s brief ("the 250 k rheostat on V2A's coupled plate").
- `tests/functional/unit/test_mesa_mark2c.cpp`: remove the three `GTEST_SKIP`s; the two operating
  points of decision 3.
- `tests/functional/regression/`: `mesa_mark2c_48k` and `mesa_mark2c_96k` regenerated.
- `modules/mesa_mark2c/README.md`: the stage table's V2A, loop and V2B rows; the Lead Master row;
  the "Known issue" paragraph replaced by a paragraph on the network and the Master's position;
  the stand-ins of section 5 listed where the VH4's are.
- `docs/reference/KNOWN_ISSUES.md`: the "Mesa Mark IIC+ has no clean setting" entry closed and
  the alias-floor entry's IIC+ figure updated.
- `docs/reference/CHANGELOG.md`: one line.
- `docs/design/sushidsp.md`: the DA5 bullet's IIC+ sentence.
- This spec's status line.
