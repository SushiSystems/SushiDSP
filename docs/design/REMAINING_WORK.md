# Remaining work

The single backlog. An entry here names work that is not started or not finished; a defect that
is measured, understood and deliberately left alone lives in
[`docs/reference/KNOWN_ISSUES.md`](../reference/KNOWN_ISSUES.md) instead, and the entries below
link to it rather than repeat its figures.

Order inside a section is the order the work should be taken in, not a priority ranking.

## Waiting on the maintainer's ears

The cabinet section has not been played. A rig now carries its cabinets' own signal on the
Cabinet input and the room on the Listener, Mic and Room strips, each with a level of its own, so
the old question of how far the room's dry/wet mix could be turned up is replaced by a plainer
one: with Cabinet muted, does a microphone 30 cm in front of the cabinet, or the listener across
the room, sound like one. `docs/agent/2026_10_13_AMPLITUBE5_CHANNELS/PLAN.md`, task G6, lists
what to try on screen. No change to the levels the room renders at starts without that verdict.

## Room, opened by the DE phase

- **Bound the output.** Nothing between `RoomNode` and the device limits what the room can
  produce; `float_to_asio_sample()` is the only clamp in the path. `RoomScene`'s
  absorption gate refuses the corner-cube's +36 dB scene, and the loudest a still-valid scene reaches has not been measured. Measure it first, then
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
  an RT60 of 3 s, and `FdnDesign::K_MAX_LINE_SECONDS` is the 0.525 s that follows from it, while
  `RoomScene::K_MAX_DECAY_SECONDS` admits scenes up to 12 s. The lines do not lengthen with the
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
- **`SlotGraph::add_branch` has no caller** since the splitter picker was removed;
  `RigController` still forwards to it.
- **`SlotGraph::publish()` rebuilds every node.** One structural edit builds the node set
  three times, which is why "No bundled HRTF set" prints three times on a graph change and why a
  room edit through the graph empties the tail (KNOWN_ISSUES, "A structural rig edit empties the
  room's tail"). Carrying surviving nodes across a rebuild closes both and touches no module.
- **A stereo branch collapses at a `MixerNode`**, and **`write_rig_output_to_device`'s 2-into-1
  downmix has one live caller**, the GUI host on a mono device (KNOWN_ISSUES, both entries). The
  ASIO example renders stereo since 2026-10-13; the mixer and the downmix are the open part.
- **DS5, the OGG export.** A five-minute stereo take is 115 MB of float on disk today.
- **39 units owe a test file.** `tools/testing/untested_units.txt` lists them: 31 in
  `applications/host_gui` and its `ui/` folder, 6 in `applications/plugin`, and the ASIO and
  SDL2 audio devices. `check_test_mirror.py` skips the listed units and fails on an entry whose
  unit has gained a test, so each test written drops one line. The two devices need hardware;
  for them the work is a fake backend or a recorded reason they stay listed
  (KNOWN_ISSUES, "39 units have no test file").

## Products

- **Calibrate the power amplifier's feedback depth, after the output transformer is the Mesa's.**
  The leg's own response is solved from S8 (`modules/foundation/circuits/source/power_amp_feedback.cpp`) and the forward
  gain is measured: 38.33 dB at 80 Hz, 39.56 at 600 Hz, 61.59 at 3 kHz and 53.31 at 10 kHz, with
  the loop open. Depth cannot be set from those numbers yet. At the depth that sweeps presence the
  18 dB the shelf it replaced did, the amplifier oscillates at 11.8 kHz, which is the 8 kHz pole of
  an output transformer still borrowed from the JCM800; at the deepest depth that does not ring,
  `K_TAP_TO_FEEDBACK_VOLTS` 14.0, a 9 dB resonance sits at 10 kHz and presence runs backwards above
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

## Opened 2026-10-13: AmpliTube 5 channels and cabinet section

The copy of AmpliTube 5's signal architecture
([`docs/agent/2026_10_13_AMPLITUBE5_CHANNELS/SPEC.md`](../agent/2026_10_13_AMPLITUBE5_CHANNELS/SPEC.md))
is written through its last phase; its plan in the same folder holds what is verified and what is
not. What it leaves:

- **Start a rebuilt strip where its level is.** Every publish builds fresh nodes and then writes
  their parameters, so a smoothed parameter glides from its default to its value over 20 ms on
  each structural edit. The Parallel chain's pans and every strip's level do this.
- **A stereo post stage.** A post effect is mono, so it sums the amp bus's two channels and the
  listener's stereo image is lost behind it (`docs/reference/KNOWN_ISSUES.md`).
- **Loop FX sends and the VIRTM view** stay out; the spec's section 3 records why.
- **A shared rig-width rule** for the two hosts waits for the owner (spec section 12).

## Opened by the 2026-10-05 estate audit

The defects are listed in
[`docs/reference/KNOWN_ISSUES.md`](../reference/KNOWN_ISSUES.md), "Open findings of the
2026-10-05 estate audit". The work that closes them, in the order it should be taken.

Decisions the owner takes first:

- **Tag v0.1.0 or fold its heading back into Unreleased.** The changelog and commit `de235ac`
  declare the release and no tag exists.
- **Say whether SushiDSP is in hub's catalogue**, then correct `docs/design/SUSHIDSP.md` §11a
  to match `sushi-module.toml`.
- **Choose one error model**, since it is public interface. The logger landed on 2026-10-07
  (`modules/foundation/log/include/SushiDSP/foundation/log/`).
- **Logger follow-ups.** No test provokes the writer-thread start or the append failure in
  `TrackRecorder`, nor the `TopologyApplier` and `SlotGraph` sites; the `list_directory` test
  skips on Windows, where a 40 000-character path raises nothing; `StderrLogSink` has no test;
  a line carries no timestamp or thread id; `sushi_dsp` uses `std::thread` without linking
  `Threads::Threads`.
- **State the test count with its sizes** in `README.md`, after one `sd test` run.

Mechanical work that follows:

- Rewrite `docs/archive/README.md` as an index of what the archive holds, and say there
  which of the two standards phase 1 plans is the record.
- Give the five placeless changelog entries of 2026-09-05 and 2026-09-07 their places.
- Write the READMEs for `applications/host/`, `applications/host_asio/`, `modules/foundation/signal_measure/`, `tests/foundation/core/`
  and each area under `include/SushiDSP/`.
- Rewrite the CLI guide, the design spec's opening block and the first known issue in the
  `humanizer` register.
- Test the 39 units `check_test_mirror.py` reports, as their own piece of work (`KNOWN_ISSUES.md`):
  31 under `applications/host_gui`, 6 under `applications/plugin` and the two device backends
  `asio_audio_device` and `sdl2_audio_device`. The CLAP boundary runs behind a fake host and the
  ImGui panels run ImGui without a window, both `medium`; a device backend runs `medium` on SDL's
  dummy audio driver where it can, and `large` against a real device.
- Run the workflows once on GitHub: `ci.yml`, `nightly.yml` and `release.yml` were written on a
  machine with no runner, and the first run is what proves the `sd` lanes, the ASan and TSan
  builds and the caches.
- The forward-sink example in `modules/foundation/log/README.md` ends in `Source: not compiled`,
  because `SushiRuntime::log_forward_sink()` lives in another repository. `check_docs_truth.py
  --report` counts it as the one unverified example. Closes when that repository's sink is built
  here, or the example is cut to what this repository compiles.
- Reserve the error model, `[[nodiscard]]` and the `RigController` and `SlotGraph` split for an
  owner decision, since they change public interfaces (see `docs/agent/2026_10_12_SUSHI_STANDARDS_MIGRATION/SPEC.md`).
- Rewrite the em dash replacements of 2026-10-12 in the `humanizer` register. They replaced each
  dash with a comma, a colon or a stop by its context, and no page was read after that pass.
- Move the SPICE transient CSVs out of `docs/reference/spice/` to a fixture folder, with the
  tests and `tools/spice/export.py` that read and write them.
- Correct `assets/ir/README.md`: the impulse response it names is not shipped, and its
  admission rule still names the Apache licence.

## Opened 2026-10-13: cabinet drivers and placed furniture

[`docs/agent/2026_10_13_CABINET_DRIVERS_AND_FURNITURE/SPEC.md`](../agent/2026_10_13_CABINET_DRIVERS_AND_FURNITURE/SPEC.md)
gave a cabinet its cones and a furnishing its place. What it leaves:

- **The cabinet section has not been heard with cones.** A microphone against a cone, at its
  edge and between two cones must be listened to against a real cabinet's behaviour, and the
  scene's boxes and cones must be looked at on screen. Neither was done.
- **An electromechanical driver.** The cones are radiating geometry only. The driver's
  resonance, its impedance as the power amplifier's load, cone breakup as a response and
  compression at level are not modelled; the cabinet's voice is still `CabinetNode`'s impulse
  response.
- **Furniture that shadows and reflects.** A placed furnishing absorbs around where it stands.
  It does not block the line of sight and has no image of its own
  ([`ROOM_FURNISHINGS.md`](ROOM_FURNISHINGS.md) section 9).
- **The starter room's furniture has no place.** Placing it changes that room's reflections,
  which the owner ear-checked on 2026-09-23, so it waits for the owner's ear.
- **The listener's line of sight is one far-field tap.** At listening distance the cones differ
  by less than the pattern says, and a per-cone direct path costs an HRIR pair each.

## Opened 2026-10-13: track rigs, export and the first plugin

[`docs/agent/2026_10_13_TRACK_RIGS_AND_PLUGIN/SPEC.md`](../agent/2026_10_13_TRACK_RIGS_AND_PLUGIN/SPEC.md)
gave each track a rig and the session an export. What it leaves:

- **The re-amp leg is dead weight.** `SessionTimeline` still carries the re-amp player, its
  track and the input-source choice in `SlotGraph`; the host does not read them, since every
  track is rendered through its own rig. They and their tests can go.
- **A track's strip has no EQ.** AmpliTube 5's has three bands.
- **Clips are only moved and removed.** No split, copy, paste, crop, fade or gain trim, and no
  punch region.
- **The export writes no 24-bit WAV and no AIFF.** `WavWriter` knows 16-bit and float.
- **MP3 export is Windows only.** `cmake/SushiDSPLame.cmake` uses the configuration header LAME
  ships for Windows.
- **Removing a track can play one block of a lane through its neighbour's rig.** The lanes move
  down in the timeline before the table of rigs does.
- **Eight rigs cost eight times one.** A track is skipped only while it is neither rolling nor
  played into.
- **The host's rig sources are a list, not a library.** The tests and the plugin compile
  `applications/host_gui`'s non-GUI sources by naming them.
- **The plugin has never met a DAW.** `SushiDSP.clap` links and its adapter's tests pass; no CLAP
  host has loaded it, and nothing installs it into a CLAP folder.
- **The plugin has no editor and no VST3 shell.** Its rig comes from the file
  `SUSHIDSP_PLUGIN_RIG` names, or from the DAW session's state.
- **The plugin's rooms use the spherical head.** The HRTF set is given to a rig outside its
  preset, and the plugin gives none.
- **A plugin parameter is applied at the block's start**, not at its event's sample, and the
  adapter finds a slot's node by pairing slot order with node order, since `RigController` has
  no lookup from a slot to its node.
- **Nothing traps an allocation in `RigAdapter::process`.** The trap in `test_rt_safety.cpp` is
  private to that file.
