# Plan: the Mark IIC+ model, the SOLID debt, and v0.1.0

**Status:** open, written 2026-09-20. Supersedes nothing. The backlog entries it creates live in
[`docs/design/REMAINING_WORK.md`](../../design/REMAINING_WORK.md); the bench-blocked questions it
cannot close stay in the dossier's §8.

The owner's hard requirement for v0.1.0 is the accuracy of the Mesa/Boogie Mark IIC+ model. This
plan puts that first, then pays the SOLID debt the audit found, then clears the release blockers.
Three phases, in that order, because phase B changes the interfaces phase A writes against and
phase C describes what A and B leave behind.

---

## A correction to the audit

The audit reported the graphic equaliser's band centres as a defect: 71.5, 371.8, 979.5, 1576 and
4823 Hz in `MesaMark2c.cpp:155-157` against the panel's 80, 240, 750, 2200 and 6600 Hz. That
reading was wrong. Dossier §4.2 states that the panel labels are wrong on the amp itself, by 11 %
to 37 %, and that three of the five derived centres land on the Mark IV's independently measured
values to three digits. The code matches the dossier; the labels do not match the hardware.

Two of the five centres were wrong all the same, for a reason neither the audit nor the dossier
could see: S1's scan gives the first and third bands 1.5 H and 0.12 H, and jrb32's traced schematic
gives them 1.0 H and 0.22 H. On the traced values all five centres land on S6's independently
derived Mark IV figures, so the corrected set is in the code and `REPORT.md` carries the reading.
The remaining defect is the one §4.2 names — each band is a fixed biquad at the trap's own
resonance, and "the table gives the trap, not the curve" — and A4 below says why it stands.

One more correction, this time to the dossier. Its §8 item 24 says the module applies Master 1
after the power section, where S1 draws it at node F; `MesaMark2c.cpp:476-479` already puts it at
node F, between V2B's coupling capacitor and the equaliser. The commit that moved it did not close
the item. Every task below was checked against the source before it was written, which is why the
audit's read of the equaliser centres and the dossier's read of Master 1 both appear here as
corrections rather than as work.

---

## What this plan does not fix

Twelve of the dossier's open questions need an LCR meter, a voltmeter or a better scan of the
1983 factory sheet. No amount of code closes them. They are §8 items 3, 5, 6, 7, 8, 9, 12, 13, 14,
18, 21 and 22; items 1, 2, 11 and 16 closed on the traced schematic the day this plan was written.
Two of the twelve carry most of the audible weight:

- **Where V4A's 0.047 uF sits** (item 18). Across the 170 K or in parallel with the 68 K. The two
  readings differ by 10.9 dB across the guitar band and set V4A's grid drive.
- **The lead output divider's shunt, 100 K on S1 or 3.3 K on S8** (item 21). It moves V2A's drive
  by 26.5 dB, and the dossier's own probe reads 9.6 % THD one way and 0.2 % the other at the clean
  end.

Item 15 is the one that matters most and is the least fixable: no bench measurement of a Mark IIC+
appears to have been published, ever. The model ships calibrated against arithmetic.

Task A1 writes every one of them into the module README as a named assumption with its §8 number,
so a reader of the model knows which numbers were read off a drawing and which were chosen. That
is the honest version of "accurate": the model is as accurate as the sources, and the sources are
listed.

---

## Phase A — the Mark IIC+ model

### A1. The assumption ledger

Add a table to `modules/mesa_mark2c/README.md` listing every constant in `MesaMark2c.cpp` that is
not read off S1, with what it is, where it came from, and the §8 item that would settle it. Add
the fifteen bench-blocked questions to `REMAINING_WORK.md` as one section, and close §8 items 19,
20 and 24, which the loop-network commit already answered.

*Acceptance:* every numeric literal in `MesaMark2c.cpp` either cites a dossier section in its
comment or appears in the README table. *Build:* `sd check`.

### A2. The FMV stack becomes a netlist

`FmvToneStack::recompute` carries a hand-derived third-order closed form that assumes an ideal
source and an open output (`src/circuits/FmvToneStack.cpp:76-89`). The repository already solves
circuits from netlists: `include/SushiDSP/math/dk/DkNetlist.hpp` stamps resistors, capacitors,
inductors and potentiometers, re-derives its matrices when a wiper moves, and the TubeScreamer runs
on it in production. Declare the stack as a netlist on that brick, with the source and load
resistors as ordinary elements, and the hand algebra goes.

The netlist is confirmed: an MNA of the topology below matches the current closed form to 1.2e-14
across 480 operating points, five treble by four bass by four middle positions at six frequencies.

```
in ── C1 ── 2 ──[ treble pot R1, wiper = out ]── 3
in ── R4 ── S ── C2 ── 3 ──[ bass rheostat l·R2 ]── M ──[ mid rheostat m·R3 ]── gnd
              S ── C3 ── M
```

*Acceptance:* the netlist's response equals the closed form to 1e-12 with the source at 0 and the
load absent, at the 480 points above, with the closed form kept in the test as the reference.
*Build:* `sd test`.

### A3. The IIC+ feeds its real impedances

The stack hangs on V1A's plate and is loaded by Volume 1's 1 M, and the model gives it neither.
Measured on the verified netlist with a 44 kohm source and a 1 M load: the level drops 4.13 dB at
1 kHz with the controls at noon, and the tilt across 41 Hz to 10 kHz is 0.87 dB. So this is a
gain-staging error, not a tone error — V1B is driven 4 dB harder than the circuit drives it, and
`kMasterUnityGain` hides the level at the output while leaving the distortion's distribution
between the stages wrong.

`kV1aSourceResistance` is not the constant to change. It feeds `TriodeStage`'s grid-conduction
divider (`src/circuits/stages/TriodeStage.cpp:106`), which is V1A's *grid* drive, a different
quantity from its plate impedance. The plate impedance is not computed anywhere today.

*Acceptance:* the stack receives `rp ∥ 150 k` from V1A's solved operating point and Volume 1's
load; the test records the level and tilt against the unloaded model. *Build:* `sd test`.

### A4. The equaliser becomes its trap network — deferred past 0.1.0

The task was to declare the five R-L-C traps as a netlist on the DK brick, as A2 did for the tone
stack. It was written against dossier §4.2, which could not reconstruct how the four PNP
transistors around the traps connect, and §8 item 11, which concluded that without them the EQ
"can be modelled as anything better than five biquads". Both were answered the same day: jrb32's
traced schematic draws the stage in full, down to R47 22.1 K and the two buses the slider bank
hangs between.

What stopped it is no longer the sources but the size. Five traps on five sliders are the easy half;
the other half is four transistors, which is a Newton solve with its own operating point, its own
convergence behaviour on the audio thread and its own reference to be checked against. That is a
task beside A2, not a step after A5, and 0.1.0 does not wait for it.

What the audit and this plan both got wrong about the code: `MesaMark2c.cpp:158` already passes five
per-band Q values derived from the real components, not one figure.

*Status:* unblocked the same day and still unbuilt. jrb32's traced schematic came down as a vector
PDF and draws the active stage in full, which closes §8 item 11; it also corrected two of the five
trap inductors, and those two band centres are now in the code
(`REPORT.md`). What remains is the work itself: four transistors are a Newton solve of their
own size, not an afternoon on top of A5. The five biquads ship in 0.1.0 and
`modules/mesa_mark2c/README.md` says so.

### A5. A real negative-feedback loop, and presence inside it

This was written as one task and is two, because the loop had nowhere to arrive. The inverter was a
single `TriodeStage` divided by a hand-set degeneration constant, so it had no second grid; S8's
feedback reaches V5B's grid through C65. The pair had to become real first. The dossier's §5.2 is
also wrong twice over, and REPORT.md carries both corrections: presence is a series rheostat, not a
shunt, and the leg's components are not the 3300 / 6.8 K ∥ 0.022 uF / 22 K written above.

**A5a. The phase inverter becomes a long-tailed pair.** `LongTailedPair` takes two grids and returns
two plates, each half a `TriodeStage` on its own plate load and driven by its own grid-to-cathode
voltage, the shared cathode following the summed grids through the tail. S8's 82.5 K and 90.9 K
replace the "82 K" both halves carried, and R67's 470 moves from a shunt between the cathodes to
series in the tail. `kPhaseInverterDegeneration` is gone: the split is derived.

*Acceptance:* the pair splits a single-ended input within 0.25 dB across its two plates, rejects a
common-mode move, and bounds each plate by its own swing. *Build:* `sd test`.

**A5b. The feedback leg closes the loop.** `PowerAmpFeedback` solves the leg on DK with presence as
a rheostat, and the loop closes at the 8x rate on the previous sample, 2.6 us. The post-amp shelf is
deleted. What is not settled is the loop's depth: the leg needs the volts at the tap and the
transformer stage works in units, so `kTapToFeedbackVolts` stands in until one calibration run.

*Acceptance:* the DK solve nulls against the same ladder solved by hand within 0.05 dB at four
frequencies and four presence positions. *Build:* `sd test`.

### A6. The JCM800 borrowings go

The output transformer's 60 Hz and 8 kHz corners (`:179-180`), the sag model and `kEl34`'s
`source_resistance_ohms` and `cathode_bias_volts` convention all came from the JCM800. Derive what
§7 supports from the dossier's assumed 3.4 kohm Raa, mark the Raa as the assumption it is, and
close §8 item 4 by reading the JCM800 dossier.

*Acceptance:* no comment in `MesaMark2c.cpp` cites `modules/jcm800`; every remaining stand-in is
in A1's table. *Build:* `sd test`.

### A7. Coefficients move within the block

`prepare()` and the parameter path rebuild filter coefficients once per block
(`MesaMark2c.cpp:684-732`), so a slider sweep steps at block boundaries. Smooth the coefficient
targets per sample.

*Acceptance:* a test sweeps the 80 Hz slider across a block boundary and the sample-to-sample
difference stays below the step's amplitude. *Build:* `sd test`.

### A8. The switches the panel has

PULL BRIGHT on both the 250 pF and the 180 pF, PULL SHIFT, PULL DEEP on V2B's cathode, the
equaliser's OUT / IN / AUTO positions, and CLASS A.

*Acceptance:* one test per switch, each asserting the direction and magnitude §3 gives. *Build:*
`sd test`.

### A9. Calibration, then the owner's ears

Re-calibrate Master 1's dB range against the new loop gain. The JCM800's tone is an ear-verified
anchor and nothing in A2 through A8 may move it.

*Acceptance:* the JCM800's golden tests are byte-identical; the IIC+ at Master 1 noon lands on the
calibration target. *Build:* `sd test`. The listening pass is the owner's, on request.

---

## Phase B — the SOLID debt

### B1. One parameter table per node

A node's parameters live in three places today: the `strcmp` chain in `Jcm800.cpp:538-558`, the
index switch beside it, and `PanelLibrary.cpp:34-133`. One declarative table — name, index,
range, unit, default — feeds all three.

*Acceptance:* both of `Jcm800`'s dispatch bodies are gone and a new parameter is one row.
*Build:* `sd test`.

### B2. Adding an amp adds a file

`NodeRegistry.cpp:28-46`, `PanelLibrary`, `PowerTubePresets.hpp` and `RigTopology.cpp:98,115,141`
each carry a list every new amp must extend. Replace them with registration at static
initialisation, the panel derived from B1's table.

*Acceptance:* a test asserts the registry holds every built module without naming one, and the
four edit sites hold no amp names. *Build:* `sd test`.

### B3. `RigController` splits

1916 lines and 92 members in `apps/host_gui/RigController.cpp`. Boundary decision, below.

### B4. `RoomNode` splits

1374 lines orchestrating 27 focused bricks, and doing work of its own. Boundary decision, below.

### B5. `include/` stops exporting generated tables

`include/SushiDSP/math/ResamplerCoefficients.hpp` is 4418 generated lines in the public surface.
It moves under `src/`.

*Acceptance:* no public header reaches it. *Build:* `sd build`.

### B6. The two `set_parameter` overloads say their thread

`IAudioNode`'s `const char*` overload is control-thread only and its `uint16_t` overload is
audio-thread only, and the signatures say neither. Rename both to carry the contract, and fix the
dead `Jcm800::set_cabinet()` reference at `IAudioNode.hpp:92`.

*Acceptance:* no call site uses the old names; the RT-safety traps still pass. *Build:* `sd test`.

### B7. The copied amp skeleton becomes bricks

`Peavey5150.cpp:177-208`, `MesaMark2c.cpp:167` and `DiezelVh4.cpp:180` carry comments saying their
code was adopted from `Jcm800.cpp`. Extract the coupling-pole helper, the output clamp and the
oversampling harness into named units and delete the copies.

*Acceptance:* no source comment in any amp module names another amp module. *Build:* `sd test`.

### B8. `modules/` grows tiers

All 12 modules sit directly under `modules/`, so every one carries the empty tier and
`rule_upward_includes` has no direction to enforce: nothing stops `node_registry` from including
an amp or an amp from including the host. Propose a tier assignment, agree it, move the modules,
and declare `K_TIER_ORDER`.

*Acceptance:* the checker reports every tier as declared and no upward include. *Build:* `sd test`.

---

## Phase C — v0.1.0

- **C1.** Fix the broken cited paths. `design/sushidsp.md` against `docs/design/SUSHIDSP.md` in
  `CONTRIBUTING.md:11,68,124` and `OVERVIEW.md:6,86`, then the `docs/superpowers/` and
  `docs/reference/research/` paths that commit `2e65f33` moved under `docs/archive/`. The audit put
  the second group at 25; a full sweep of `modules/`, `src/`, `include/`, `apps/`, `tests/` and the
  manual counts 105 references in 60 files, in source comments as well as in links. Also retire the
  `.superpowers/sdd/` paths cited from source, which are not in the repository at all.
- **C2.** Add `NOTICE`, the nominative trademark note for the modelled amplifiers, and Apache
  headers to the 18 Python files under `tools/` and `cli/`.
- **C3.** Set `VERSION 0.1.0` in `CMakeLists.txt:2` and cut `## Unreleased` to `## [0.1.0]`.
- **C4.** Declare `K_TIER_ORDER` in `check_layering.py` and fix `rule_module_readmes` in
  `check_docs_layout.py`, which reports false positives on the flat `modules/` layout.
- **C5.** Resolve `ci.yml:103`, which checks out `SushiSystems/SushiStack` to install `sushicore`.
  A public clone of SushiDSP cannot run that job. External dependency decision, below.
- **C6.** Move `CODE_OF_CONDUCT.md` and `SECURITY.md` where GitHub finds them, add issue
  templates, and replace the 26-line `README.md`.

Each of C1 through C6 closes with `sd check`.

---

## Decisions the owner made, 2026-09-20

1. **Execution.** Phase A runs inline, one session in hierarchy order, because each circuit step
   changes the sound the next one is calibrated against. Phases B and C run in waves over disjoint
   file sets.
2. **`FmvToneStack`'s interface.** A `SourceLoad` value carries the source and load impedance, so
   the signature holds as more amps need it.
3. **The equaliser's home.** `src/circuits`, which is where `GraphicEqualizer` already lives, so
   A4 extends that brick instead of adding one.
4. **C5.** The `cli` job leaves CI. A public clone builds green and the `sushicore` check stays
   local.
5. **The solvers.** `FmvToneStack` and `GraphicEqualizer` both become netlists on
   `math/dk`, the brick the TubeScreamer already runs on. The closed form stays in the tests as
   the reference the netlist must reproduce. The tone stack landed in A2. The equaliser half of
   this decision cannot be carried out on the sources the repository has; A4 says why.
6. **Tiers.** `modules/` grows tier folders, so `rule_upward_includes` has a direction to enforce.
   All 12 modules move; the tier assignment itself is the decision still open below.

## Decisions still open

- **Where B1's parameter table lives.** `include/SushiDSP/core/ParameterTable.hpp` makes it part
  of the public node contract, which is what B2 needs.
- **`RigController` and `RoomNode`.** Both splits fix module boundaries. The split lines are
  agreed before either file is touched, after phase A.
- **Which tiers, and which module sits in each.** Task B8 proposes an assignment for the 12
  modules; nothing moves before it is agreed.
