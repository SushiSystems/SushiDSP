# Known issues

Next id: KI-46

## Amplifiers

### KI-2 The IIC+ and the 5150 leave little headroom at the smallest device buffer
- Origin: own
- Where: `tests/nodes/mesa_mark2c/benchmark_mesa_mark2c.cpp`
- Cause: the valve count solved at 8×: six triodes, a long-tailed pair and four power tubes
- Recognise: the IIC+ costs 7236 ns a sample and the 5150 6286, against the JCM800's 2587; a
  256-frame block costs the IIC+ 1.852 ms of a 1.5 ms budget, and a chain with it underruns at
  the smallest USB buffer on a slow block
- Workaround: use a larger device buffer
- Fix: RW-5

## Room

### KI-6 Four furnishing and covering rows carry no measurement of their own
- Origin: own
- Where: `modules/nodes/room/source/furniture.cpp`
- Cause: no ISO 354 or JIS A 1409 test of a household desk, a double bed with bedding, a filled
  bookcase or a kilim on a hard floor could be sourced
- Recognise: each row's citation names what it stands in for; the room-level sum sits inside the
  0.16 to 0.35 m² per m³ furnished-room studies measure
- Workaround: none
- Fix: none, until a per-object measurement is published

### KI-8 Four of the spec's fourteen material rows are not transcribed
- Origin: own
- Where: `modules/nodes/room/include/SushiDSP/nodes/room/materials.hpp`
- Cause: brick, acoustic foam, the steel garage door and carpet on underlay were not copied
- Recognise: a user picks the nearest row; the decay is off by up to 0.4 in absorption at 500 Hz
- Workaround: pick the nearest row
- Fix: RW-22

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

### KI-13 Nothing between the node and the device bounds the output
- Origin: own
- Where: `modules/nodes/room/include/SushiDSP/nodes/room/room_node.hpp`
- Cause: `float_to_asio_sample()` clips the integer formats at full scale and passes a float
  format through; `RoomScene::is_valid` refuses the +36 dB scenes, and the loudest valid one is
  not measured
- Recognise: the three reference rooms reach +9.7 dB
- Workaround: keep a limiter after the rig
- Fix: RW-6

## Host and sessions

### KI-19 Removing a track can double one block of audio
- Origin: own
- Where: `applications/host_gui/session_timeline.cpp`
- Cause: `SessionTimeline::remove_track` republishes the lanes one at a time
- Recognise: one 5.33 ms block carries a clip twice
- Workaround: remove tracks while the transport is stopped
- Fix: RW-10

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

### KI-30 The test mirror exception is a local edit of a shared checker
- Origin: own
- Where: `tools/testing/check_test_mirror.py`
- Cause: this repository changed it to skip the listed units and to add `rule_stale_untested_entry`
- Recognise: `copy_shared_tools.py --report` lists the file; a copy drops the exception and turns
  CI red
- Workaround: restore this repository's version after a copy
- Fix: RW-34

## Documentation and layout

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
- Where: `docs/design/SUSHIDSP/README.md`, `sushi-module.toml`
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
