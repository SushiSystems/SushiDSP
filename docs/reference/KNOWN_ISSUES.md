# Known issues

Next id: KI-46

## Amplifiers

### KI-1 The amplifiers alias above their bounds at 8×
- Origin: own
- Where: `modules/foundation/circuits/include/SushiDSP/foundation/circuits/stages/antiderivative_antialiaser.hpp`,
  `tests/nodes/jcm800/test_jcm800/test_output.cpp`
- Cause: the stages after the first receive an already-clipped waveform whose content reaches the
  oversampled Nyquist, which breaks the assumption first-order antiderivative antialiasing rests
  on; the transformer's tanh, the sag and the solver's iterations are measured and ruled out
- Recognise: at a −12 dBFS input `measure::alias_floor()` reads the JCM800 at −23.4 dB at preamp
  1.0 against −33.8 dB, the 5150 at −26.8 dB against −30 dB, the IIC+ at −14.8 dB, the VH4 at
  −23.0 dB; the worst line is the 20 kHz tone's 19th harmonic folding to 3995 Hz. Two tests are
  skipped with the figure printed
- Workaround: none
- Fix: RW-4

One stage alone folds at −102.4 dB at 3×. 16× without the antialiaser read JCM800 −35.0, 5150
−19.9, IIC+ −14.3, VH4 −27.3 dB; 16× with it reads JCM800 −45.3 (−32.7 at preamp 1.0), 5150
−36.2, IIC+ −36.9, VH4 −35.3 dB at twice the CPU, which puts the 5150 and the IIC+ at 99 to 102 %
of the block at `-O2`.

### KI-2 The IIC+ and the 5150 leave little headroom at the smallest device buffer
- Origin: own
- Where: `tests/nodes/mesa_mark2c/benchmark_mesa_mark2c.cpp`
- Cause: the valve count solved at 8×: six triodes, a long-tailed pair and four power tubes
- Recognise: the IIC+ costs 7236 ns a sample and the 5150 6286, against the JCM800's 2587; a
  256-frame block costs the IIC+ 1.852 ms of a 1.5 ms budget, and a chain with it underruns at
  the smallest USB buffer on a slow block
- Workaround: use a larger device buffer
- Fix: RW-5

### KI-3 The Mesa Mark IIC+ has no clean setting
- Origin: own
- Where: `tests/nodes/mesa_mark2c/test_mesa_mark2c/test_behaviour.cpp`
- Cause: the dossier gives no value for the effects send and return link, so the model has no
  loop network and V2A's plate drives V2B's grid about twenty-one times past it
- Recognise: the node limits at every input level; three tests are skipped with what they
  measure printed
- Workaround: none
- Fix: RW-44

### KI-4 The Diezel VH4's phase inverter and power section are the JCM800's
- Origin: own
- Where: `modules/nodes/jcm800/source/jcm800.cpp`
- Cause: the dossier establishes nothing about the VH4's power section
- Recognise: every figure after the phase inverter is borrowed from the JCM800
- Workaround: none
- Fix: RW-42

## Room

### KI-5 The built room's loss fits neither measured room below 500 Hz
- Origin: own
- Where: `modules/nodes/room/include/SushiDSP/nodes/room/built_room_loss.hpp`
- Cause: one absorption figure per band suits Motus and BRAS CR2 from 500 Hz up; below, they want
  different figures and 0.020 and 0.025 sit between them
- Recognise: at 125 Hz the model reads 0.745 of Motus's decay and 2.133 of CR2's; at 250 Hz 0.913
  and 1.476. A fresh room's low end is uncertain by about a factor of two
- Workaround: none
- Fix: RW-18

### KI-6 Four furnishing and covering rows carry no measurement of their own
- Origin: own
- Where: `modules/nodes/room/source/furniture.cpp`
- Cause: no ISO 354 or JIS A 1409 test of a household desk, a double bed with bedding, a filled
  bookcase or a kilim on a hard floor could be sourced
- Recognise: each row's citation names what it stands in for; the room-level sum sits inside the
  0.16 to 0.35 m² per m³ furnished-room studies measure
- Workaround: none
- Fix: none, until a per-object measurement is published

### KI-7 The measured HRTF's decoder misses the near ear by up to 5.1 dB above 5 kHz
- Origin: own
- Where: `assets/hrtf/README.md`
- Cause: the third-order magnitude-least-squares decoder of the SADIE II KU100 set
- Recognise: within 0.56 dB at the median and 2.61 dB at the 95th percentile, but up to 5.1 dB
  on the near ear at 5 to 8 kHz; the test holds the near ear to 6 dB above 5 kHz
- Workaround: none
- Fix: RW-15

### KI-8 Four of the spec's fourteen material rows are not transcribed
- Origin: own
- Where: `modules/nodes/room/include/SushiDSP/nodes/room/materials.hpp`
- Cause: brick, acoustic foam, the steel garage door and carpet on underlay were not copied
- Recognise: a user picks the nearest row; the decay is off by up to 0.4 in absorption at 500 Hz
- Workaround: pick the nearest row
- Fix: RW-22

### KI-9 The SM57 preset has no measured curve
- Origin: own
- Where: `modules/nodes/room/source/microphone_model.cpp`
- Cause: the one preset, `K_MICROPHONE_FLAT_CARDIOID`, has six zero bands and cites no curve
- Recognise: a microphone render lacks the capsule's presence rise and low roll-off
- Workaround: none
- Fix: RW-23

### KI-10 Modal damping at a soft boundary is about 30 % wrong
- Origin: own
- Where: `modules/nodes/room/include/SushiDSP/nodes/room/modal_solver.hpp`
- Cause: `ModalSolver::damping_constant` is linear in the absorption, right while it stays under
  about 0.1; gypsum board absorbs 0.29 at 125 Hz
- Recognise: the room's pitch is right and its low-end decay time is not
- Workaround: none
- Fix: none, the damped eigenproblem costs what FDTD does

### KI-11 The tail carries nothing below the Schroeder frequency
- Origin: own
- Where: `modules/nodes/room/include/SushiDSP/nodes/room/late_reverb.hpp`
- Cause: `CrossoverPair` hands the band under f_s to the modal bank, and the FDN's lines are
  designed for the band above
- Recognise: the tail measured alone below f_s is empty
- Workaround: none needed
- Fix: none, the design gives the low end to the eigenmodes

### KI-12 A compensated ears set's tail lands after its direct sound
- Origin: own
- Where: `modules/nodes/room/include/SushiDSP/nodes/room/room_node.hpp`
- Cause: `RoomNode::compensate_direct_delay` moves the render earlier, and the tail's read delay
  is already zero, so the tail keeps its physical timing
- Recognise: 363 frames, 7.6 ms, for the default scene; below what a listener resolves
- Workaround: none needed
- Fix: RW-26

### KI-13 Nothing between the node and the device bounds the output
- Origin: own
- Where: `modules/nodes/room/include/SushiDSP/nodes/room/room_node.hpp`
- Cause: `float_to_asio_sample()` clips the integer formats at full scale and passes a float
  format through; `RoomScene::is_valid` refuses the +36 dB scenes, and the loudest valid one is
  not measured
- Recognise: the three reference rooms reach +9.7 dB
- Workaround: keep a limiter after the rig
- Fix: RW-6

### KI-14 The raw octave step at the Schroeder frequency is not bounded
- Origin: own
- Where: `modules/nodes/room/include/SushiDSP/nodes/room/room_node.hpp`
- Cause: the reference itself changes across the seam
- Recognise: +0.16 dB for the garage, +1.29 for the bedroom, +3.75 for the block room; each layer
  holds within 2 dB of the shared reference
- Workaround: none
- Fix: RW-25

### KI-15 `BandResponseFit` stops at eight steps on about 60 % of fits
- Origin: own
- Where: `modules/nodes/room/source/band_response_fit.cpp`
- Cause: `K_MAX_STEPS` caps the refinement, which differentiates numerically at six evaluations
  a step
- Recognise: the direct path holds within 0.1 dB and a one-band 20 dB cut within 0.5 dB; a
  tighter tolerance hits the cap first
- Workaround: none
- Fix: RW-17

### KI-16 The HRTF converter counts every measured direction equally
- Origin: own
- Where: `tools/dsp/convert_hrtf.py`
- Cause: `diffuse_field_average` sums directions without weighting by solid angle
- Recognise: on a ten-degree latitude-longitude grid the polar rings carry up to 5.8 times their
  weight; nothing hits it while the fallback set is what every rig loads
- Workaround: none
- Fix: RW-14

### KI-17 A publish steps the tail's output filter on its running state
- Origin: own
- Where: `modules/nodes/room/include/SushiDSP/nodes/room/late_reverb.hpp`
- Cause: a moved microphone or listener refits the cascade, and the new coefficients load at the
  block the fade begins
- Recognise: the residual reads −45.8 dBFS for a 0.36 m move in the garage; the step ratios are
  unchanged, so nothing clicks
- Workaround: none needed
- Fix: RW-16

### KI-18 The cabinet section at its limits misses its gate
- Origin: own
- Where: `modules/nodes/room/include/SushiDSP/nodes/room/room_node.hpp`
- Cause: three JCM800s cost 2.03 ms and the room 1.28 ms of a 256-frame block
- Recognise: the benchmark row "3 x JCM800 + RoomNode" reads 3.562 ms against a 2.67 ms gate
- Workaround: none needed; the rig fits the 5.33 ms block
- Fix: RW-9

## Host and sessions

### KI-19 Removing a track can double one block of audio
- Origin: own
- Where: `applications/host_gui/session_timeline.cpp`
- Cause: `SessionTimeline::remove_track` republishes the lanes one at a time
- Recognise: one 5.33 ms block carries a clip twice
- Workaround: remove tracks while the transport is stopped
- Fix: RW-10

### KI-20 A structural rig edit empties the room's tail
- Origin: own
- Where: `applications/host_gui/rig_controller.cpp`
- Cause: `RigController::publish()` builds every node afresh, the room's lines at zero
- Recognise: the tail stops at the edit and rebuilds over the next RT60; "No bundled HRTF set"
  prints three times
- Workaround: none
- Fix: RW-30

### KI-21 A stereo branch collapses to mono at a `MixerNode`
- Origin: own
- Where: `modules/nodes/routing/include/SushiDSP/nodes/routing/mixer_node.hpp`
- Cause: `MixerNode` declares one channel per slot, so the gather averages a stereo producer
- Recognise: a `PanNode` in a parallel branch loses its width at the mixer
- Workaround: none
- Fix: RW-31

### KI-22 A post effect sums the cabinet section to mono
- Origin: own
- Where: `applications/host_gui/slot_graph.hpp`
- Cause: a post-stage slot is mono in, so `RigGraph` sums the amp bus
- Recognise: the listener's image and every strip's pan are lost behind the post stage
- Workaround: none
- Fix: RW-38

### KI-23 The 2-into-1 downmix has no live caller
- Origin: own
- Where: `modules/foundation/audio_format/include/SushiDSP/foundation/audio_format/channel_selection.hpp`
- Cause: the host prepares the rig no wider than the device
- Recognise: only `WriteRigOutputSumsAndHalvesTwoRigChannelsIntoAMonoDevice` runs the branch
- Workaround: none needed
- Fix: none, the branch completes the adaptation table for a rig whose width is fixed

### KI-24 `Transport::seek` racing `advance` can be lost
- Origin: own
- Where: `modules/nodes/capture/source/transport.cpp`
- Cause: both store to `position_`
- Recognise: a seek during a block lands a block-length away
- Workaround: serialise the two, as the host and `seek`'s `@pre` do
- Fix: none, no caller needs a generation counter

### KI-25 A take's own sample rate is never read
- Origin: own
- Where: `modules/nodes/capture/include/SushiDSP/nodes/capture/track_player.hpp`
- Cause: a clip is read frame for frame against the rig's rate
- Recognise: a 44.1 kHz WAV on a 48 kHz rig plays 8.8 % slow; imported MP3s are converted and
  unaffected
- Workaround: convert a WAV to the rig's rate before dropping it
- Fix: RW-35

### KI-26 The imported-audio cache is never pruned
- Origin: own
- Where: `applications/host_gui/audio_import_cache.hpp`
- Cause: every dropped MP3 leaves a float WAV under `cache/imported_audio/`
- Recognise: about 79 MB for a 3.4-minute song
- Workaround: delete the folder between sessions
- Fix: RW-36

### KI-27 A take leaves the host as WAV only
- Origin: own
- Where: `modules/foundation/io/include/SushiDSP/foundation/io/wav_writer.hpp`
- Cause: DS5, the OGG export, is unwritten
- Recognise: a five-minute stereo take is 115 MB of float
- Workaround: convert outside the host
- Fix: RW-32

### KI-28 The host writes `imgui.ini` into the working directory
- Origin: own
- Where: `applications/host_gui/README.md`
- Cause: ImGui's default ini path is left in place
- Recognise: an `imgui.ini` appears where the host was started
- Workaround: start the host from its own folder
- Fix: RW-72

## Tests and CI

### KI-29 39 units have no test file
- Origin: own
- Where: `tools/testing/untested_units.txt`
- Cause: the host, the plugin and the two device backends need ImGui, a CLAP host or sound
  hardware to run
- Recognise: the list names them; the checker fails on an entry that gains a test
- Workaround: none
- Fix: RW-33

### KI-30 The test mirror exception is a local edit of a shared checker
- Origin: own
- Where: `tools/testing/check_test_mirror.py`
- Cause: this repository changed it to skip the listed units and to add `rule_stale_untested_entry`
- Recognise: `copy_shared_tools.py --report` lists the file; a copy drops the exception and turns
  CI red
- Workaround: restore this repository's version after a copy
- Fix: RW-34

### KI-31 The small tests fail under ThreadSanitizer on CI
- Origin: own
- Where: `.github/workflows/ci.yml`
- Cause: unknown
- Recognise: the job "Small tests under thread (Linux, Clang)" fails on every push since
  2026-10-18
- Workaround: none
- Fix: RW-65

### KI-32 24 host sources are compiled into the test binary
- Origin: own
- Where: `tests/CMakeLists.txt`
- Cause: the host's rig sources are a list, not a library
- Recognise: the tests and the plugin name the sources one by one
- Workaround: none
- Fix: RW-52

## Documentation and layout

### KI-33 The `## v0.1.0` heading has no tag behind it
- Origin: own
- Where: `docs/reference/CHANGELOG.md`
- Cause: the release was declared and not tagged
- Recognise: `git tag -l` prints nothing
- Workaround: none
- Fix: RW-1

### KI-34 Changelog entries cite places that do not exist or none
- Origin: own
- Where: `docs/reference/CHANGELOG.md`
- Cause: the entry of 2026-10-03 cites `docs/archive/changelog/`, three entries cite removed
  files, and five of 2026-09-05 and 2026-09-07 name no place
- Recognise: the cited paths resolve nowhere
- Workaround: read `git log`
- Fix: RW-62

### KI-35 The archive's README says the archive is empty
- Origin: own
- Where: `docs/archive/README.md`
- Cause: it was written before anything was archived; the archive holds 56 other files under
  dated lower-case names, and two plans of the standards phase 1 work
- Recognise: the README and the folder disagree
- Workaround: list the folder
- Fix: RW-8

### KI-36 The design and the manifest disagree on the hub catalogue
- Origin: own
- Where: `docs/design/SUSHIDSP.md`, `sushi-module.toml`
- Cause: the design calls SushiDSP a registered member; the manifest says it is not catalogued
- Recognise: the two files contradict
- Workaround: trust the manifest
- Fix: RW-2

### KI-37 Two READMEs cite an impulse response that does not ship
- Origin: own
- Where: `assets/ir/README.md`, `applications/host_gui/README.md`
- Cause: `default_4x12.wav` is not in the tree, and the admission rule names the Apache licence
- Recognise: the file is absent beside the executable
- Workaround: give the host an impulse response of your own
- Fix: RW-68

### KI-38 No document states the test count
- Origin: own
- Where: `README.md`
- Cause: the count of 1007 could not be reproduced without a build and was removed
- Recognise: the root README gives no count
- Workaround: run `sd test`
- Fix: RW-60

### KI-39 3.1 MB of transient CSVs sit inside the manual
- Origin: own
- Where: `docs/reference/spice`
- Cause: the SPICE exports were written beside the reference pages
- Recognise: `*_tran.csv` files under `docs/reference/spice/`
- Workaround: none needed
- Fix: RW-67

### KI-40 The root build file defines targets and fetches dependencies
- Origin: own
- Where: `CMakeLists.txt`
- Cause: the logic was never moved into `cmake/`
- Recognise: `add_library` and `FetchContent` in the root file
- Workaround: none needed
- Fix: RW-70

### KI-41 The version is kept by hand in two files
- Origin: own
- Where: `CMakeLists.txt`, `cli/pyproject.toml`, `sushi-module.toml`
- Cause: no generated version header; the manifest states none
- Recognise: a release edits two files by hand
- Workaround: none
- Fix: RW-69

### KI-42 The ignore file names an output nothing writes
- Origin: own
- Where: `.gitignore`
- Cause: it ignores `docs/api-site/html/`, names `sd doxygen` and carries paragraphs of comment
- Recognise: the rules and their comments
- Workaround: none needed
- Fix: RW-71

## Code shape

### KI-43 Three error models coexist
- Origin: own
- Where: `modules/foundation/io/source/wav_file.cpp`, `applications/host_gui/rig_controller.hpp`,
  `modules/nodes/room/include/SushiDSP/nodes/room/room_node.hpp`
- Cause: exceptions, `bool` with an error string and bare `bool` were each chosen locally
- Recognise: a caller handles failure three ways; `RigController` forwards `SlotGraph` one for
  one and `main()` owns setup and the frame loop
- Workaround: none
- Fix: RW-3

### KI-44 The host names products by id, and `nlohmann::json` is public
- Origin: own
- Where: `applications/host_gui/ui/panel_library.cpp`,
  `modules/session/rig_preset/include/SushiDSP/session/rig_preset/rig_preset.hpp`
- Cause: the panels, the filter and the topology applier list products; the preset header
  includes the JSON library
- Recognise: adding a product edits the GUI; a preset user compiles against nlohmann
- Workaround: none
- Fix: RW-73

### KI-45 The amplifier modules repeat helpers, and `Wdf` is unused
- Origin: own
- Where: `modules/nodes/jcm800/source/jcm800.cpp`,
  `modules/foundation/math/include/SushiDSP/foundation/math/wdf.hpp`
- Cause: the four amplifiers each carry the same helpers and constants; `Wdf` has no production
  user
- Recognise: the same helper in four files; `wdf.cpp` built and unused
- Workaround: none
- Fix: RW-75
