# Remaining work

Next id: RW-78

## Decisions waiting for the owner

### RW-1 Tag v0.1.0 or fold its heading back into Unreleased
- State: decision
- Done when: the `## v0.1.0` heading of the changelog has a tag behind it, or is folded into
  `## Unreleased`

The changelog and commit `de235ac` declare the release and no tag exists.

### RW-2 Say whether SushiDSP is in hub's catalogue
- State: decision
- Done when: `docs/design/SUSHIDSP.md` and `sushi-module.toml` agree on it
- Source: `docs/design/SUSHIDSP.md`

The design calls the repository a registered SushiStack member; the manifest says it is not in
hub's catalogue.

### RW-3 Choose one error model and the host's split
- State: decision
- Done when: the owner has chosen one error model, where `[[nodiscard]]` goes, and how
  `RigController`, `SlotGraph` and `main()` divide the host
- Source: `docs/design/SUSHI_STANDARDS_MIGRATION.md`

Each changes a public interface; see `docs/agent/2026_10_12_SUSHI_STANDARDS_MIGRATION/SPEC.md`.

### RW-4 Choose how the alias floor closes
- State: decision
- Done when: the owner has chosen 16× oversampling, a band limit between the stages that lets 8×
  stay, or a higher-order antiderivative, and the amplifiers meet their bounds

Part 4's measurements for the three are in `KI-1`.

### RW-5 Choose how the IIC+ and the 5150 fit the smallest buffer
- State: decision
- Done when: the owner has chosen 4× for these models or a cheaper triode solve

### RW-7 Rule a shared rig-width rule for the two hosts
- State: decision
- Done when: the owner has ruled it (spec section 12)
- Source: `docs/agent/2026_10_13_AMPLITUBE5_CHANNELS/SPEC.md`

### RW-8 Decide what the archive's index and names become
- State: decision
- Done when: the owner has ruled whether `docs/archive/README.md` becomes an index naming which
  of the two standards phase 1 plans is the record, and whether the dated lower-case names under
  `docs/archive/` move to `UPPER_SNAKE_CASE.md`

The archive is frozen; both edit it.

### RW-9 Move the cabinet section's gate, or make the amplifier cheaper
- State: decision
- Done when: three JCM800 chains into one room with eight microphones meet the gate, or the
  owner has moved the gate from half the block

### RW-10 Choose how a track removal stays atomic
- State: decision
- Done when: a generation counter the callback reads once per block, or a refusal of the removal
  while the transport runs, is chosen and built

### RW-76 Decide whether `Wdf` stays
- State: decision
- Done when: `Wdf` has a production user, or it leaves the library

## Waiting on the maintainer's ears

### RW-11 Hear the cabinet section with Cabinet muted
- State: unmeasured
- Done when: the owner has ruled whether a microphone 30 cm in front of the cabinet, or the
  listener across the room, sounds like one
- Source: `docs/agent/2026_10_13_AMPLITUBE5_CHANNELS/PLAN.md`

Task G6 lists what to try on screen. No change to the levels the room renders at starts without
that verdict.

### RW-12 Hear and see the cabinet section with cones
- State: unmeasured
- Done when: a microphone against a cone, at its edge and between two cones has been heard
  against a real cabinet, and the scene's boxes and cones have been looked at on screen
- Source: `docs/agent/2026_10_13_CABINET_DRIVERS_AND_FURNITURE/SPEC.md`

### RW-13 Place the starter room's furniture
- State: unmeasured
- Done when: the owner has heard the starter room with its furniture placed; it changes the
  reflections the owner ear-checked on 2026-09-23

## Room

### RW-6 Measure the loudest valid scene, then decide on a guard
- State: unmeasured
- Done when: the loudest scene `RoomScene::is_valid` admits is measured, and the owner has ruled
  whether a guard belongs in the node, the rig or neither

### RW-14 Weight the HRTF converter's diffuse average by solid angle
- State: open
- Done when: `tools/dsp/convert_hrtf.py` derives each direction's solid angle from the grid it
  finds

### RW-15 Close the near ear's miss above 5 kHz
- State: open
- Done when: a higher decoder order, or a decoder that trades the far ear for the near one,
  brings the near ear within 3 dB above 5 kHz

### RW-16 Run the tail's output cascade twice under a fade
- State: open
- Done when: a listener or microphone move blends the retiring and arriving coefficients on one
  line sum, two more biquads per ear in `LateReverb`

### RW-17 Give `BandResponseFit` an analytic Jacobian
- State: open
- Done when: the fit converges inside a higher cap; it stops at eight steps on about 60 % of fits
  and costs 4.33 ms for 150 fits against the 30 Hz republish budget

### RW-18 Settle the built room's loss below 500 Hz
- State: open
- Done when: a third measured room, or a low-frequency term tied to lightweight elements, sets
  the 125 Hz and 250 Hz figures
- Source: `docs/design/ROOM_FURNISHINGS.md` §11

### RW-19 Rebuild the room's line allocation for long decays
- State: open
- Done when: spec section 6.2 sizes the delay lines for the 12 s `RoomScene::K_MAX_DECAY_SECONDS`
  admits, or a measurement shows the 0.525 s lines do not thin audibly
- Source: `docs/design/SUSHIDSP.md`

### RW-20 Let furniture shadow and reflect
- State: open
- Done when: a placed furnishing blocks the line of sight and has an image of its own
- Source: `docs/design/ROOM_FURNISHINGS.md` §9

### RW-21 Give the listener one direct path per cone
- State: open
- Done when: each cone has its own direct path to the ears, at an HRIR pair each

### RW-22 Transcribe the remaining four material rows
- State: open
- Done when: brick, acoustic foam, the steel garage door and carpet on underlay join the table
  with a citation each

### RW-23 Give the SM57 its measured curve
- State: open
- Done when: Shure's published response is a second row of `microphone_model.cpp` with its own
  `MicrophoneKind`

### RW-25 Bound the raw octave step at the seam
- State: open
- Done when: a bound derived from the reference's own step holds the raw step across f_s

### RW-26 Shorten the tail's lines by the direct-delay compensation
- State: open
- Done when: the designer is told the compensation and the tail lands with the direct sound

## Host

### RW-27 Shift the pitch of track playback
- State: open
- Done when: the bottom bar's Pitch field shifts track playback by semitones and leaves speed
  alone, in a plan of its own (owner decision of 2026-09-14)
- Source: `docs/design/GUI_UX_REDESIGN.md` §12

### RW-28 Run the phase 2 manual acceptance
- State: unmeasured
- Done when: the seven checks of task 14 of
  `docs/archive/superpowers/plans/2026-09-14-host-gui-phase2.md` have been run in the GUI

### RW-29 Remove `SlotGraph::add_branch`
- State: open
- Done when: neither `SlotGraph` nor `RigController` carries it; it has no caller since the
  splitter picker went

### RW-30 Carry surviving nodes across a rebuild
- State: open
- Done when: `SlotGraph::publish()` keeps the nodes a structural edit leaves in place, so the
  room's tail survives the edit and the node set is built once per edit

### RW-31 Keep a stereo branch stereo through a mixer
- State: open
- Done when: a `PortLayout` can say eight slots of two channels and `MixerNode` keeps its inputs'
  width

### RW-32 Export OGG, DS5
- State: open
- Done when: a take leaves the host as OGG; a five-minute stereo take is 115 MB of float today

### RW-33 Test the 39 units without a test file
- State: open
- Done when: `tools/testing/untested_units.txt` is empty: 31 units in `applications/host_gui`, 6
  in `applications/plugin`, and the ASIO and SDL2 device backends

The CLAP boundary runs behind a fake host and the ImGui panels without a window, both `medium`; a
device backend runs `medium` on SDL's dummy driver where it can and `large` against a device.

### RW-34 Take the untested-unit exception from SushiSkills
- State: blocked
- Blocked by: sushiskills RW-20
- Done when: the shared `check_test_mirror.py` matches SushiSkills', the list is read from
  `tools/settings.py`, and `tools/testing/untested_units.txt` is gone

### RW-35 Read a take's own sample rate
- State: open
- Done when: a clip carries its take's rate over the rig's into `VariRateReader`

### RW-36 Cap the imported-audio cache
- State: open
- Done when: `cache/imported_audio/` has a size cap and evicts by last use

### RW-37 Start a rebuilt strip where its level is
- State: open
- Done when: a structural edit leaves no 20 ms glide from a parameter's default to its value

### RW-38 Give the post stage a stereo input
- State: open
- Done when: an effect can declare a stereo input, or the post stage runs one instance per
  channel

### RW-72 Write `imgui.ini` outside the working directory
- State: open
- Done when: the host sets ImGui's ini path to a folder of its own

### RW-73 Let the host find products without naming them
- State: open
- Done when: adding a product edits no file under `applications/host_gui/`

## Products

### RW-40 Calibrate the power amplifier's feedback depth
- State: blocked
- Blocked by: RW-41
- Done when: the depth that sweeps presence leaves the loop stable; `K_TAP_TO_FEEDBACK_VOLTS`
  14.0 is a holding value

Forward gain with the loop open: 38.33 dB at 80 Hz, 39.56 at 600 Hz, 61.59 at 3 kHz, 53.31 at
10 kHz. At the depth that sweeps 18 dB the amplifier oscillates at 11.8 kHz, the 8 kHz pole of a
transformer still borrowed from the JCM800; at 14.0, a 9 dB resonance sits at 10 kHz.

### RW-41 Measure the Mesa Mark IIC+'s output transformer
- State: blocked
- Blocked by: external: a bench measurement of a real chassis, or one published
- Done when: the transformer's two corners, its core drive and both sag figures are measured
- Source: `docs/agent/2026_09_20_V0_1_0_RELEASE/PLAN.md`

A search on 2026-09-21 found the figures unpublished. Three switches also wait on the schematic:
PULL BRIGHT's V4A cathode resistor (question 9), which pair CLASS A takes out (question 12), and
BASS SHIFT's capacitor and leg. The equaliser as its trap network is deferred past 0.1.0.

### RW-42 Find a Diezel VH4 power-amp schematic
- State: blocked
- Blocked by: external: a published schematic
- Done when: the phase inverter and power section are the VH4's

### RW-43 Answer the Mark IIC+ questions only a real amplifier answers
- State: blocked
- Blocked by: external: a Mark IIC+ on a bench
- Done when: the dossier's twelve §8 bench items are measured
- Source: `modules/nodes/mesa_mark2c/README.md`

First: the lead output divider's shunt (item 21, 100 K or 3.3 K, 26.5 dB of V2A's drive), and
where V4A's 0.047 µF sits (item 18, 10.9 dB across the guitar band). Item 15, a published bench
measurement, does not exist.

### RW-44 Read the IIC+'s effects send and return network
- State: blocked
- Blocked by: external: a schematic that gives the link's values
- Done when: the network is modelled and the three skipped tests of `test_behaviour.cpp` assert
  again (dossier questions 19 and 20)

## Track rigs and the plugin

### RW-45 Remove the re-amp leg
- State: open
- Done when: `SessionTimeline` and `SlotGraph` carry no re-amp player, track or input-source
  choice, and their tests go
- Source: `docs/agent/2026_10_13_TRACK_RIGS_AND_PLUGIN/SPEC.md`

### RW-46 Give a track's strip an EQ
- State: open
- Done when: the strip has three bands, as AmpliTube 5's does

### RW-47 Edit clips
- State: open
- Done when: a clip can be split, copied, pasted, cropped, faded and gain-trimmed, and a punch
  region exists

### RW-48 Export 24-bit WAV and AIFF
- State: open
- Done when: `WavWriter` writes 24-bit and an AIFF writer exists

### RW-49 Export MP3 on Linux
- State: open
- Done when: `cmake/SushiDSPLame.cmake` configures LAME on Linux

### RW-50 Keep a removed track's lane out of its neighbour's rig
- State: open
- Done when: removing a track moves the lanes and the table of rigs in one step

### RW-51 Skip a rig that has nothing to play
- State: open
- Done when: eight idle rigs cost less than eight times one

### RW-52 Make the host's rig sources a library
- State: open
- Done when: the tests and the plugin link a library in place of naming 24
  `applications/host_gui` sources

### RW-53 Load the plugin in a DAW and install it
- State: unmeasured
- Done when: a CLAP host has loaded `SushiDSP.clap` and an install puts it in a CLAP folder

### RW-54 Give the plugin an editor and a VST3 shell
- State: open
- Done when: both exist

### RW-55 Give the plugin's rooms the HRTF set
- State: open
- Done when: a plugin rig loads the measured set, not the spherical head

### RW-56 Apply a plugin parameter at its event's sample
- State: open
- Done when: parameters apply at the event, and `RigController` maps a slot to its node

### RW-57 Trap an allocation in `RigAdapter::process`
- State: open
- Done when: the allocation trap of `test_rt_safety.cpp` is shared and covers the adapter

### RW-58 Model the cabinet's drivers
- State: open
- Done when: a driver's resonance, its impedance as the power amplifier's load, cone breakup and
  compression at level are modelled
- Source: `docs/agent/2026_10_13_CABINET_DRIVERS_AND_FURNITURE/SPEC.md`

## Logging, tests and CI

### RW-59 Finish the logger's tests and lines
- State: open
- Done when: tests provoke the writer-thread start, the append failure in `TrackRecorder` and the
  `TopologyApplier` and `SlotGraph` sites; `list_directory` fails on Windows in its test;
  `StderrLogSink` has a test; a line carries a timestamp and a thread id; `sushi_dsp` links
  `Threads::Threads`

### RW-60 State the test count with its sizes in the README
- State: unmeasured
- Done when: one `sd test` run gives the count and `README.md` states it

### RW-64 Run the release workflow once
- State: unmeasured
- Done when: `.github/workflows/release.yml` has run on GitHub; CI and Nightly have

### RW-65 Make the small tests pass under ThreadSanitizer
- State: open
- Done when: the CI job "Small tests under thread (Linux, Clang)" passes

### RW-66 Compile the forward-sink example
- State: open
- Done when: the example in `modules/foundation/log/README.md` comes from a file this tree
  compiles, or is cut to what it compiles; `SushiRuntime::log_forward_sink()` lives elsewhere

### RW-77 Test the scripts under `tools/dsp/` and `tools/spice/`
- State: open
- Done when: each script has tests

## Documentation and layout

### RW-62 Correct the changelog's places
- State: open
- Done when: the five placeless entries of 2026-09-05 and 2026-09-07 have places, and the
  entries citing `docs/archive/changelog/` and three removed files cite what exists

### RW-63 Read the pages the em dash pass changed
- State: open
- Done when: the CLI guide, the design spec's opening block and the pages the 2026-10-12 pass
  changed read in the `humanizer` register

### RW-67 Move the SPICE transient CSVs out of the manual
- State: open
- Done when: `docs/reference/spice/*/*_tran.csv` move to a fixture folder with the tests and
  `tools/spice/export.py` that read and write them

### RW-68 Correct the impulse response pages
- State: open
- Done when: `assets/ir/README.md` and `applications/host_gui/README.md` name what ships, and the
  admission rule names this repository's licence

### RW-69 Generate the version
- State: open
- Done when: a generated header carries the version, `cli/pyproject.toml` reads it, and
  `sushi-module.toml` states it

### RW-70 Move the root build file's logic into `cmake/`
- State: open
- Done when: the root `CMakeLists.txt` defines no target and fetches no dependency itself

### RW-71 Clean the ignore file
- State: open
- Done when: `.gitignore` names nothing nobody writes, says `sd docs`, and carries rules, not
  paragraphs

### RW-74 Keep `nlohmann::json` out of the preset module's interface
- State: open
- Done when: `rig_preset.hpp` includes no JSON header

### RW-75 Give the four amplifier modules their shared helpers once
- State: open
- Done when: the helpers and constants `jcm800.cpp`, `peavey5150.cpp`, `diezel_vh4.cpp` and
  `mesa_mark2c.cpp` repeat live in one module
