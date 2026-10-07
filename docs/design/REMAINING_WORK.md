# Remaining work

The single backlog. An entry here names work that is not started or not finished; a defect that
is measured, understood and deliberately left alone lives in
[`docs/reference/KNOWN_ISSUES.md`](../reference/KNOWN_ISSUES.md) instead, and the entries below
link to it rather than repeat its figures.

Order inside a section is the order the work should be taken in, not a priority ranking.

## Waiting on the maintainer's ears

The room's mix control has not been played since the tail's handover moved. Before
2026-09-09 the first tail energy arrived 5198 frames after the direct sound, 108 ms at 48 kHz,
because the input delay carried the handover time on top of the shortest line; the shortest line
is the handover now. The maintainer played the old build at `mix = 0.09` with +6 dB of make-up
and called anything above that "gaipten geliyor". Two questions close this: whether the gap is
gone, and whether `mix = 1.0` is usable.

If it is still unusable, the level is not the suspect. The direct path measures within air
absorption of `1/r` and `DiffuseField` reproduces the classical critical-distance formula
algebraically, both anchored against hand arithmetic. What is left is the shape of the control:
`mix` blends dry against a wet path that carries the room's own direct sound, so the honest
control may be the listener's distance to the cabinet rather than a dry/wet ratio. No work
starts on that without the verdict.

## Room, opened by the DE phase

- **Bound the output.** Nothing between `RoomNode` and the device limits what the room can
  produce; `AsioSampleConverter::clamp_unit` is the only clamp in the path. The decision number
  used to be the corner-cube's +36 dB, but `RoomScene`'s absorption gate refuses that scene now,
  and the loudest a still-valid scene reaches has not been measured. Measure it first, then
  decide whether a guard belongs in the node, the rig or neither.
- **Weight the converter's diffuse average by solid angle**, now that the SADIE II set ships and
  the host loads it; and decide whether the near ear's 5.1 dB above 5 kHz wants a higher decoder
  order (KNOWN_ISSUES).
- **Run the tail's output cascade twice under a fade**, so a listener or microphone move stops
  stepping the filter on the network's stored energy.
- **Give `BandResponseFit` an analytic Jacobian.** It stops at its eight-step cap on about 60 %
  of fits and costs 4.33 ms for 150 fits, against the 30 Hz republish budget.
- **Settle the built room's loss below 500 Hz.** It fits Motus and BRAS CR2 from 500 Hz up; at
  125 Hz it reads 0.75 of one and 2.1 of the other (KNOWN_ISSUES). A third measured room, or a
  low-frequency term tied to lightweight elements, decides it.
- **Rebuild the room's spec section 6.2.** Its line-allocation sentence sizes the delay lines for
  an RT60 of 3 s, and `FdnDesign::kMaxLineSeconds` is the 0.525 s that follows from it, while
  `RoomScene::kMaxDecaySeconds` admits scenes up to 12 s. The lines do not lengthen with the
  gate, so a scene between the two decays at the echo density 0.525 s lines give. Whether that
  thins audibly is not measured.

## Host

- **Track playback pitch shift.** The bottom bar's Pitch field
  (`docs/design/GUI_UX_REDESIGN.md` §12) is drawn disabled. It needs a semitone shift of
  track playback that leaves speed alone; today `VariRateReader` moves pitch with speed and
  nothing in the tree stretches time. Owner decision 2026-09-14: its own plan, applied to
  track playback only.
- **Phase 2 manual acceptance.** The seven checks in Task 14 of
  `docs/archive/superpowers/plans/2026-09-14-host-gui-phase2.md` have not been run in the GUI.
- **Parallel topology in presets.** A preset with a parallel shape loads flattened to a
  series chain (`apps/host_gui/main.cpp`, `seed_rig_chain`); §19 leaves persistence open.
- **`SlotGraph::add_branch` has no caller** since the splitter picker was removed;
  `RigController` still forwards to it.
- **`SlotGraph::publish()` rebuilds every node.** One structural edit builds the node set
  three times, which is why "No bundled HRTF set" prints three times on a graph change and why a
  room edit through the graph empties the tail (KNOWN_ISSUES, "A structural rig edit empties the
  room's tail"). Carrying surviving nodes across a rebuild closes both and touches no module.
- **Render the rig in stereo on the ASIO host**, which also gives
  `write_rig_output_to_device`'s 2-into-1 downmix its first live caller and stops a stereo branch
  collapsing at a `MixerNode`. Three KNOWN_ISSUES entries, one change.
- **DS5, the OGG export.** A five-minute stereo take is 115 MB of float on disk today.

## Products

- **Calibrate the power amplifier's feedback depth, after the output transformer is the Mesa's.**
  The leg's own response is solved from S8 (`src/circuits/PowerAmpFeedback.cpp`) and the forward
  gain is measured: 38.33 dB at 80 Hz, 39.56 at 600 Hz, 61.59 at 3 kHz and 53.31 at 10 kHz, with
  the loop open. Depth cannot be set from those numbers yet. At the depth that sweeps presence the
  18 dB the shelf it replaced did, the amplifier oscillates at 11.8 kHz, which is the 8 kHz pole of
  an output transformer still borrowed from the JCM800; at the deepest depth that does not ring,
  `kTapToFeedbackVolts` 14.0, a 9 dB resonance sits at 10 kHz and presence runs backwards above
  it. What the loop does is decided by the phase of what it loops around, so this item waits on the
  transformer below. The 14.0 in the tree is a holding value, not a calibration.
- **The Mesa Mark IIC+'s remaining accuracy work.** Planned in
  [`docs/agent/2026_09_20_V0_1_0_RELEASE/PLAN.md`](../agent/2026_09_20_V0_1_0_RELEASE/PLAN.md)
  phase A. Coefficients became a 32-frame control period on 2026-09-21, and PULL BRIGHT, PULL DEEP
  and the equaliser's three-position switch landed the same day. Three switches stay out because
  the schematic does not settle them: PULL BRIGHT's V4A cathode resistor is illegible (§8 question
  9), CLASS A does not say which pair it takes out (question 12), and BASS SHIFT's sheet gives
  neither the capacitor's value nor which leg the switch lifts. The JCM800's
  citations left the module on 2026-09-21, but the output transformer's two corners, its core drive
  and both sag figures remain stand-ins, because neither sheet prints the primary impedance and the
  dossier's calibration targets carry no supply figure. Those five are listed in the module README's
  table; the leakage corner is the one that now decides whether the feedback loop is stable. A
  search outside the repository on 2026-09-21 established that the transformer's figures are
  unpublished rather than merely unread, so this waits on a bench measurement of a real chassis or
  on Mercury Magnetics characterising one. The tone stack's termination landed 2026-09-20, the
  equaliser's centres were corrected the same day, and presence became the traced feedback loop
  around a real long-tailed pair. The equaliser as its trap network is deferred past 0.1.0: S8 draws
  the active stage, but four transistors are a Newton solve of their own size.
- **Find a Diezel VH4 power-amp schematic.** Its phase inverter and power section are the
  JCM800's, borrowed whole.
- **Close the alias floor.** The maintainer's call between 16×, a band limit between the stages,
  or a higher-order antiderivative; part 4's measurements for all three are in KNOWN_ISSUES.
- **Transcribe the remaining four material rows**, and measure the SM57's curve.
- **Give `sd test` a test filter.** Without one, `SUSHIDSP_REGENERATE_GOLDEN=1 sd test -s regression`
  rewrites all twelve golden files, so re-pinning one node means overwriting the JCM800 anchor's and
  restoring it by hand afterwards (`tests/golden/README.md`).

## The Mark IIC+ questions only a real amplifier answers

Twelve of the dossier's §8 items need an LCR meter, a voltmeter or a better scan of the 1983
factory sheet, and no line of code closes one. Each is listed against the constant it holds up in
[`modules/nodes/mesa_mark2c/README.md`](../../modules/nodes/mesa_mark2c/README.md). The two that carry the
most audible weight, in order:

- **The lead output divider's shunt** (item 21). 100 K on S1, 3.3 K on S8, and 26.5 dB of V2A's
  drive between them. An ohmmeter across R30 on any IIC+ settles it.
- **Where V4A's 0.047 µF sits** (item 18). Across the 170 K or in parallel with the 68 K, worth
  10.9 dB across the guitar band. A photograph of that corner of the board settles it.

Item 15 is the one nothing in this repository can fix: no bench measurement of a Mark IIC+ appears
to have been published. The model is calibrated against arithmetic until someone puts one on an
analyser.

Items 1, 2, 11 and 16 closed on 2026-09-20 against jrb32's traced schematic, which reads as a
vector drawing rather than a scan; the readings and the corrections they make to the dossier are in
[`docs/agent/2026_09_20_V0_1_0_RELEASE/REPORT.md`](../agent/2026_09_20_V0_1_0_RELEASE/REPORT.md).
The two EQ inductors moved two band centres in the model. The equaliser's active stage is now drawn
but not modelled, and item 17's SPICE decks are unblocked by the same reading.

## Opened by the 2026-10-05 estate audit

The defects are listed in
[`docs/reference/KNOWN_ISSUES.md`](../reference/KNOWN_ISSUES.md), "Open findings of the
2026-10-05 estate audit". The work that closes them, in the order it should be taken.

Decisions the owner takes first:

- **Tag v0.1.0 or fold its heading back into Unreleased.** The changelog and commit `de235ac`
  declare the release and no tag exists.
- **Say whether SushiDSP is in hub's catalogue**, then correct `docs/design/SUSHIDSP.md` §11a
  to match `sushi-module.toml`.
- **Choose the core's place in the module tree**: one module per area under `modules/<tier>/`,
  or a recorded exemption for the root `include/` and `src/`.
- **Choose one error model**, since it is public interface. The logger landed on 2026-10-07
  (`include/SushiDSP/log/`).
- **Logger follow-ups.** No test provokes the writer-thread start or the append failure in
  `TrackRecorder`, nor the `TopologyApplier` and `SlotGraph` sites; the `list_directory` test
  skips on Windows, where a 40 000-character path raises nothing; `StderrLogSink` has no test;
  a line carries no timestamp or thread id; `sushi_dsp` uses `std::thread` without linking
  `Threads::Threads`.
- **Converge on the C++ style skill's names or record an exemption**: snake_case files,
  `K_` constants, no `I` prefix.
- **State the test count with its suite** in `README.md`, after one `sd test --suite all` run.

Mechanical work that follows:

- Rewrite `docs/archive/README.md` as an index of what the archive holds, and say there
  which of the two standards phase 1 plans is the record.
- Give the five placeless changelog entries of 2026-09-05 and 2026-09-07 their places.
- Write the READMEs for `apps/host/`, `apps/host_asio/`, `tests/measure/`, `tests/benchmark/`
  and each area under `include/SushiDSP/`.
- Rewrite the CLI guide, the design spec's opening block and the first known issue in the
  `humanizer` register.
- Rename `src/` to `source/` in every module and at the root, and `apps/` to `applications/`.
- Add a checker job to CI and route its build through `sd`; split the workflow by trigger.
- Add `.clang-format`, `.editorconfig` and `*.f32 binary`; move `Doxyfile` out of the root.
- Bring the source comments to the `source-comments` shape, then add
  `check_source_comments.py` to `sd check`.
- Move the SPICE transient CSVs out of `docs/reference/spice/` to a fixture folder, with the
  tests and `tools/spice/export.py` that read and write them.
- Correct `assets/ir/README.md`: the impulse response it names is not shipped, and its
  admission rule still names the Apache licence.
