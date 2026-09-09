# Room acoustics: a hybrid image-source and FDN room, rendered binaurally (DE) Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Build `modules/room`, whose one node takes the cabinet's mono output and returns the two
channels a listener would hear standing at a chosen spot in a chosen room, with the room's early
reflections from geometry, its decay from its materials, its standing waves from its dimensions,
and its direction from an HRTF.

**Architecture:** Eleven leaf bricks land first, none of which names another: the six-band gain
vector, the materials table, the air law, the two-biquad fit, the cabinet's directivity, the
microphone's pattern, the three records that cross the thread boundary, the crossover, the HRTF
container, the Ambisonic encode, and the Python converter. Four audio-thread renderers and three
control-thread solvers build on those. `RoomNode` is then grown in four builds — early plus tail,
then the wave layer, then the listener choice, then the binaural path — because every one of them
edits the same two files. The host panel and the benchmark close it.

**Tech Stack:** C++17, gtest, `SushiDSP/core` (`IAudioNode`, `PortLayout`, `NodeDescriptor`,
`RtSwapSlot`, `DenormalGuard`), `SushiDSP/math` (`Biquad`, `Fft`, `PartitionedConvolution`,
`StreamingConvolution`, `SmoothedParameter`), `modules/node_registry`, Dear ImGui in
`apps/host_gui`, Python 3 with numpy and scipy for `tools/dsp/convert_hrtf.py`, `sd build` /
`sd test`.

**Spec:** `docs/superpowers/specs/2026-09-08-room-acoustics-design.md`, approved 2026-09-08.

## Scope

This plan covers **DE0 through DE6** of the spec's §14: the module, both listeners, the wave
layer, the binaural path, the host panel and the benchmark. It stops there on purpose.

- **DE7**, the AVX2 lanes in `EarlyReflectionRenderer`, opens only if the DE1 benchmark shows the
  room over 2.0 ms beside the JCM800 chain (spec §10). That measurement does not exist yet, so the
  work cannot be planned yet; Task 29 produces the number that decides it.
- **DE8**, the ray tail (`max/RayTracer`, `max/LateIrSynthesizer`, the `quality` parameter), is a
  second subsystem with its own offline solver and its own convolvers. It gets its own plan once
  DE3 and DE5 have landed, because the crossover and the decoder it is measured against are built
  here.

Nothing in DE0–DE6 depends on either, and the room is a finished product without them: the hybrid
mode with the modal layer in it is what the maintainer plays through.

## Global Constraints

Copied from the spec. Every task's requirements implicitly include this section.

- C++17 only; Allman braces; `snake_case` functions and variables, `PascalCase` types,
  `kPascalCase` constants, trailing-underscore members; spelled-out names, no abbreviations.
- Every `.hpp`/`.cpp`, tests included, opens with the 76-column license box copied verbatim from
  `include/SushiDSP/core/PortLayout.hpp`, only the filename line changed, then a `@file` block with
  one `@brief` sentence and `@author`. `@brief` on every symbol, private members included. A
  comment states an invariant, never a reason; the reason lives in the spec and is cited by path.
- The core (`include/SushiDSP`, `src/`) depends on the C++17 standard library and SIMD intrinsics
  only. `modules/room` depends only on `sushi_dsp` and adds nothing to the core. Only
  `apps/host_gui` may take a third-party dependency, and only with the maintainer's word.
- Anything reachable from `IAudioNode::process()` is allocation-free, lock-free, syscall-free and
  throw-free, and carries an `operator new` trap case in `tests/functional/unit/test_rt_safety.cpp`.
- Parameter order in a `NodeDescriptor` is a contract: appended, never reordered. `quality` is
  therefore not declared here; DE8 appends it.
- The control thread may allocate, throw, and take a millisecond. Every solver in this plan is
  control-thread code and says so in its class `@brief`.
- `kMaxEarlyTaps` = 128, `kLineCount` = 16, `kMaxModes` = 512, `kBandCount` = 6, and every buffer
  the audio thread reads is sized for those at `prepare()` and never grows.
- **The controller owns every `CMakeLists.txt` and runs every build.** Subagents run no `sd`,
  `cmake`, `ninja` or `ctest`, edit no build file, and never terminate a running
  `sushidsp_host_gui.exe`. A subagent that wants to see its own code compile writes a standalone
  probe and compiles it directly with the flags `build/compile_commands.json` records for a
  neighbouring translation unit; that is how the amplifier work has been measuring without
  building. A task's last step before its commit is `clang -fsyntax-only` through those flags,
  with the command and its output pasted.
- Commit by explicit path with a plain `-m` subject, never `git add -A`, never `--amend`; another
  session shares this checkout. Trailers on every commit.

## Physical constants, and where each one comes from

An implementer who has to invent a number will invent one. Every number this plan's tasks need is
here, with its source. Nothing else may be introduced without asking the maintainer.

| Constant | Value | Source |
| --- | --- | --- |
| Speed of sound `kSpeedOfSound` | 343.0 m/s | dry air at 20 °C: 331.3·sqrt(1 + 20/273.15) = 343.2, rounded to 343.0 for the whole module. Kinsler, Frey, Coppens and Sanders, *Fundamentals of Acoustics*, 4th ed., §5.6 |
| Band centres `kBandCentresHz` | 125, 250, 500, 1000, 2000, 4000 Hz | ISO 354 octave centres; spec §4.2 |
| Air conditions | 20 °C, 50 % RH, 101.325 kPa, fixed | spec §4.2 and §11 |
| Head radius, for the ITD | a = 8.75 cm | Duda and Martens, JASA 104(5), 1998; spec §9.2 |
| Inter-ear spacing, for the tail's coherence | d = 0.17 m | Lindevald and Benade, JASA 80(2), 1986; spec §6.4 |
| Cone radius, 12-inch driver | 0.125 m | spec §7; k·a = 1 at c/(2π·a) = 436.7 Hz |
| Driver spacing, 4x12 | 0.30 m | spec §7 |
| Baffle width, 4x12 | 0.75 m | spec §7; baffle step at 115/0.75 = 153 Hz |
| Cone breakup | f_b = 2.5 kHz, effective radius a(f) = a / sqrt(1 + (f/f_b)²) | spec §7, a documented approximation |
| Image order | N = 4, `kMaxEarlyTaps` = 128 | count(N) = (2/3)·N·(N+1)·(2N+1) + 2N + 1 gives 7, 25, 63, 129, the direct sound included |
| Handover time | t_c = (N + 1)·L_min / c | spec §4.1; 5·2.5/343 = 36.44 ms for a 2.5 m ceiling |
| Modal overlap target | Σ τ_i ≥ 1.4 · RT60 | three modes per 2.2/RT60 bandwidth; spec §6.2 |
| Schroeder frequency | f_s = 2000·sqrt(RT60_500 / V) | spec §9a.4, RT60_500 from Eyring |
| Crossfade length | 512 frames, raised cosine | spec §5.2 |

**Absorption coefficients.** Six random-incidence α per material at the six centres; the pressure
reflection factor per band is |R| = sqrt(1 − α), phase taken as zero. The rows below are the ones
the spec transcribes from Everest and Pohlmann, *Master Handbook of Acoustics*, 5th ed.,
McGraw-Hill 2009, Appendix "Absorption coefficients", plus the `OpenDoorway` pseudo-material.
**These six are the whole shipped table.** The other eight surfaces the spec's §4.2 names — brick,
plywood, wood floor, carpet on underlay, heavy curtain, acoustic foam, a steel garage door and
painted concrete — need numbers from the cited pages that this plan does not have. Task 3 ships six
rows and leaves the rest to a transcription the maintainer supplies. No implementer invents a row.

| MaterialId | 125 | 250 | 500 | 1k | 2k | 4k | s (default) |
| --- | --- | --- | --- | --- | --- | --- | --- |
| `ConcreteUnpainted` | 0.01 | 0.01 | 0.02 | 0.02 | 0.02 | 0.03 | 0.10 |
| `ConcreteBlockPainted` | 0.10 | 0.05 | 0.06 | 0.07 | 0.09 | 0.08 | 0.10 |
| `GypsumBoardOnStuds` | 0.29 | 0.10 | 0.05 | 0.04 | 0.07 | 0.09 | 0.10 |
| `HeavyCarpetOnConcrete` | 0.02 | 0.06 | 0.14 | 0.37 | 0.60 | 0.65 | 0.10 |
| `GlassWindow` | 0.35 | 0.25 | 0.18 | 0.12 | 0.07 | 0.04 | 0.10 |
| `OpenDoorway` | 1.00 | 1.00 | 1.00 | 1.00 | 1.00 | 1.00 | 0.00 |

Scattering is one of three words, not a number: `Flat` = 0.10, `Textured` = 0.25, `Cluttered` =
0.40 (Vorländer, *Auralization*, Springer 2008, ch. 11; spec §4.2).

**Air absorption**, ISO 9613-1:1993, computed rather than tabulated. T is the temperature in
kelvin, T0 = 293.15 K, T01 = 273.16 K, p_a the pressure, p_r = 101.325 kPa, h_rel the relative
humidity in percent:

    p_sat/p_r = 10^(-6.8346*(T01/T)^1.261 + 4.6151)
    h         = h_rel * (p_sat/p_r) / (p_a/p_r)
    F_rO      = (p_a/p_r) * (24 + 4.04e4*h*(0.02 + h)/(0.391 + h))
    F_rN      = (p_a/p_r) * (T/T0)^(-1/2) * (9 + 280*h*exp(-4.170*((T/T0)^(-1/3) - 1)))
    alpha(f)  = 8.686 * f^2 * ( 1.84e-11*(p_a/p_r)^(-1)*(T/T0)^(1/2)
                + (T/T0)^(-5/2) * ( 0.01275*exp(-2239.1/T)/(F_rO + f^2/F_rO)
                                  + 0.1068*exp(-3352.0/T)/(F_rN + f^2/F_rN) ) )    dB/m

At 20 °C, 50 % RH and 101.325 kPa that formula gives, and Task 4 asserts:

| f (Hz) | 125 | 250 | 500 | 1000 | 2000 | 4000 |
| --- | --- | --- | --- | --- | --- | --- |
| α, dB/m | 0.00044 | 0.00131 | 0.00273 | 0.00466 | 0.00989 | 0.02967 |
| m = α/4.343, 1/m | 1.01e-4 | 3.02e-4 | 6.28e-4 | 1.074e-3 | 2.277e-3 | 6.831e-3 |

**The three reference rooms**, declared once in `tests/common/RoomFixtures.hpp` and used by every
later acceptance. They are the rooms the six shipped materials can build, so the spec's §6.5 garage
carries an open doorway instead of a steel door and its third room is concrete block instead of
wood and plaster. Both substitutions go away when the maintainer transcribes those rows.

| Room | Dimensions (m) | V (m³) | S (m²) | Surfaces |
| --- | --- | --- | --- | --- |
| Garage | 6.0 × 4.0 × 2.5 | 60.0 | 98.0 | `ConcreteUnpainted` everywhere, with 6.0 m² of `OpenDoorway` in the 4.0 × 2.5 m wall at +x |
| Bedroom | 4.0 × 3.5 × 2.6 | 36.4 | 67.0 | floor `HeavyCarpetOnConcrete` (14.0 m²), ceiling and walls `GypsumBoardOnStuds` (51.5 m²), one `GlassWindow` patch (1.5 m²) |
| Block room | 5.0 × 4.0 × 2.7 | 54.0 | 88.6 | `ConcreteBlockPainted` everywhere |

RT60 in seconds from Eyring with the air term,
`RT60(f) = 0.161*V / (-S*ln(1 - mean_alpha(f)) + 4*m(f)*V)`:

| Room | 125 | 250 | 500 | 1k | 2k | 4k | f_s (Hz) |
| --- | --- | --- | --- | --- | --- | --- | --- |
| Garage | 1.342 | 1.333 | 1.161 | 1.146 | 1.108 | 0.893 | 278.2 |
| Bedroom | 0.326 | 0.871 | 1.154 | 0.731 | 0.428 | 0.356 | 356.1 |
| Block room | 0.929 | 1.886 | 1.548 | 1.305 | 0.983 | 0.981 | 338.6 |

The bedroom is livelier than the spec's prose ("dies in 300 ms") because six materials cannot
furnish a bedroom: nothing here models a bed, a wardrobe or curtains. The test asserts that the
renderer reproduces Eyring, not that the room is plausible.

**The garage's first eight eigenfrequencies**, f = (c/2)·sqrt((n_x/L_x)² + (n_y/L_y)² + (n_z/L_z)²)
for 6.0 × 4.0 × 2.5 m: 28.58, 42.88, 51.53, 57.17, 68.60, 71.46, 74.32 and 80.90 Hz. Modes below
f_s = 278.2 Hz: 187 by enumeration, 190 by Weyl's N(f), both well under `kMaxModes` = 512.

---

## Wave 1 (two tasks, parallel, disjoint files)

### Task 1: `BandResponse`, the six numbers everything else speaks in

**Files:**
- Create: `modules/room/include/SushiDSP/room/BandResponse.hpp` (header-only: the type is six
  floats and the arithmetic on them, and every consumer wants it inlined)
- Test: `tests/functional/unit/test_band_response.cpp`
- Controller creates `modules/room/CMakeLists.txt` on the `modules/dd3/CMakeLists.txt` pattern and
  adds the test to `tests/CMakeLists.txt`.

**Interfaces:**
- Consumes: nothing.
- Produces:

```cpp
namespace SushiDSP::room
{
    inline constexpr std::size_t kBandCount = 6;

    /// @brief The six ISO 354 octave centres every band quantity in this module is sampled at.
    inline constexpr std::array<double, kBandCount> kBandCentresHz =
        { 125.0, 250.0, 500.0, 1000.0, 2000.0, 4000.0 };

    /// @brief Dry air at 20 degrees Celsius. Spec §4.2.
    inline constexpr double kSpeedOfSound = 343.0;

    /// @brief A pressure gain per octave band, linear, never in decibels on the wire.
    struct BandResponse
    {
        std::array<float, kBandCount> gain;

        static BandResponse unity() noexcept;
        static BandResponse from_decibels(const std::array<float, kBandCount>& decibels) noexcept;

        BandResponse& operator*=(const BandResponse& other) noexcept;
        BandResponse  powered(double exponent) const noexcept;

        /// @brief The power mean over the six bands, which is what a diffuse field receives.
        float power_mean() const noexcept;

        std::array<float, kBandCount> decibels() const noexcept;
    };
}
```

- [ ] **Step 1: Write the failing tests** in `test_band_response.cpp`, with the
      `TEST(Unit_Dsp, ...)` naming every file in `tests/functional/unit` uses:

```cpp
TEST(Unit_Dsp, BandResponseUnityIsOneInEveryBand)
{
    const SushiDSP::room::BandResponse unity = SushiDSP::room::BandResponse::unity();
    for (std::size_t band = 0; band < SushiDSP::room::kBandCount; ++band)
    {
        EXPECT_FLOAT_EQ(unity.gain[band], 1.0f);
    }
}

TEST(Unit_Dsp, BandResponseMultipliesBandByBand)
{
    SushiDSP::room::BandResponse first{ { 0.5f, 0.5f, 0.25f, 1.0f, 2.0f, 4.0f } };
    const SushiDSP::room::BandResponse second{ { 2.0f, 4.0f, 4.0f, 1.0f, 0.5f, 0.25f } };
    first *= second;
    const std::array<float, 6> expected = { 1.0f, 2.0f, 1.0f, 1.0f, 1.0f, 1.0f };
    for (std::size_t band = 0; band < 6u; ++band)
    {
        EXPECT_FLOAT_EQ(first.gain[band], expected[band]);
    }
}

TEST(Unit_Dsp, BandResponsePowerOfFourIsFourMultiplications)
{
    const SushiDSP::room::BandResponse one{ { 0.9f, 0.8f, 0.7f, 0.6f, 0.5f, 0.4f } };
    const SushiDSP::room::BandResponse four = one.powered(4.0);
    for (std::size_t band = 0; band < 6u; ++band)
    {
        const float by_hand =
            one.gain[band] * one.gain[band] * one.gain[band] * one.gain[band];
        EXPECT_NEAR(four.gain[band], by_hand, 1e-6f);
    }
}

TEST(Unit_Dsp, BandResponseDecibelsRoundTrip)
{
    const std::array<float, 6> decibels = { -6.0f, -3.0f, 0.0f, 3.0f, 6.0f, -20.0f };
    const SushiDSP::room::BandResponse response =
        SushiDSP::room::BandResponse::from_decibels(decibels);
    EXPECT_NEAR(response.gain[0], 0.501187f, 1e-6f);   // 10^(-6/20)
    EXPECT_NEAR(response.gain[5], 0.1f, 1e-6f);        // 10^(-20/20)
    const std::array<float, 6> back = response.decibels();
    for (std::size_t band = 0; band < 6u; ++band)
    {
        EXPECT_NEAR(back[band], decibels[band], 1e-4f);
    }
}

TEST(Unit_Dsp, BandResponsePowerMeanIsTheRootMeanSquare)
{
    const SushiDSP::room::BandResponse flat{ { 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f } };
    EXPECT_NEAR(flat.power_mean(), 1.0f, 1e-6f);
    const SushiDSP::room::BandResponse mixed{ { 0.0f, 0.0f, 0.0f, 2.0f, 2.0f, 2.0f } };
    EXPECT_NEAR(mixed.power_mean(), std::sqrt(2.0f), 1e-6f);   // sqrt((3 * 4) / 6)
}
```

- [ ] **Step 2: Confirm they fail** by compiling the test with `clang -fsyntax-only` and the flags
      `build/compile_commands.json` records for a neighbouring test translation unit; paste the
      error naming the missing `BandResponse.hpp`.
- [ ] **Step 3: Implement.** `powered` is `std::pow` in `double` per band. `power_mean` is
      `sqrt(mean(gain²))`. `decibels` is `20*log10(gain)` with a −200 dB floor at zero gain, stated
      as an invariant comment.
- [ ] **Step 4: Syntax-check**, paste the command and its output.
- [ ] **Step 5: Commit** `git add modules/room/include/SushiDSP/room/BandResponse.hpp
      tests/functional/unit/test_band_response.cpp` then `git commit -m "DE0: BandResponse carries
      six octave-band gains and the arithmetic on them"`.

**Acceptance:** `from_decibels({-6, -3, 0, 3, 6, -20})` gives 0.501187 in band 0 and 0.1 in band 5,
and `decibels()` returns the input within 1e-4 dB.

---

### Task 2: `convert_hrtf.py` and the asset's README

**Waits on:** nothing. It touches no C++ and no CMake.

**Files:**
- Create: `tools/dsp/convert_hrtf.py`, `assets/hrtf/README.md`
- Modify: `tools/dsp/README.md` (one entry, shaped like the `design_resampler.py` entry)

**Interfaces:**
- Consumes: nothing in this repository. Developer-side `numpy`, `scipy`, and `python-sofa` or
  `netCDF4` for reading SOFA. None of them becomes a dependency of any target.
- Produces: **the byte layout of `assets/hrtf/sadie2_ku100_48k.hrtf`**, which Tasks 10 and 17 read.
  Little-endian throughout, `float32` unless stated:

```
offset 0   char[8]   magic, exactly "SDHRTF\0\0"
offset 8   uint32    version, 1
offset 12  uint32    sample_rate_hz
offset 16  uint32    direction_count
offset 20  uint32    tap_count            (128)
offset 24  uint32    decoder_order        (3)
offset 28  uint32    flags                (bit 0: the head is left-right symmetric)
offset 32  direction_count records of:
               float32 azimuth_rad        (0 front, increasing to the left)
               float32 elevation_rad      (0 horizontal, + up)
               float32 itd_frames         (+ means the right ear is later)
               float32[tap_count] left    minimum-phase HRIR
               float32[tap_count] right
then       2*(decoder_order+1)^2 records of float32[256]   the magnitude-least-squares decoder,
                                                           ACN/SN3D order, left ear then right
then       2 records of float32[128]                       the diffuse-field average per ear
```

At 1550 directions and 128 taps the direction block is 1550 × (12 + 1024) = 1 605 800 bytes and
the decoder block is 32 × 1024 = 32 768 bytes, so the file is about 1.6 MB.

- [ ] **Step 1: Write the tool's `--self-test` first.** With no database present the tool must
      still prove its writer and its reader agree: `convert_hrtf.py --self-test out.hrtf` builds
      six directions (0°, 90°, 180° and 270° azimuth at 0° elevation, plus the zenith and the
      nadir) from the Duda-Martens
      spherical-head model, writes the file, reads it back, and exits non-zero unless every field
      is bit-identical. Task 17's fixture is this file.
- [ ] **Step 2: Implement the conversion.** `--input <SOFA file or WAV directory> --output <path>`
      reads the measured pairs, takes the minimum-phase part of each by cepstral folding (real
      cepstrum, fold the anticausal half, exponentiate), measures each pair's ITD as the difference
      of the two onsets at −20 dB of each channel's peak, truncates to 128 taps, and reports the
      truncation error in decibels. The tool exits non-zero if that error exceeds −50 dB (spec
      §9.3).
- [ ] **Step 3: Implement the decoder design.** Magnitude least squares at third order
      (Schörkhuber, Zaunschirm and Höldrich, DAGA 2018): plain least squares below 2 kHz,
      magnitude-only above it, 256 taps per filter. `--decoder-from-file <path>` takes SADIE's own
      published set instead. The tool prints the worst third-octave magnitude error against the
      measured HRTF from 200 Hz to 8 kHz, which is the number §9.5's 3 dB bound is read from.
- [ ] **Step 4: Write `assets/hrtf/README.md`** on the shape of `assets/ir/README.md`: what the
      host looks for and where, the requirements on the file, the licence block, and a "Not
      present?" section. The licence block records the database name and version, the University of
      York copyright line, the Apache License 2.0 this repository already carries, the URL the set
      came from, the SHA-256 of the converted file, the converter's version and its command line,
      and the citation the SADIE project asks for: Armstrong, Thresh, Murphy and Kearney, "A
      Perceptual Evaluation of Individual and Non-Individual HRTFs: A Case Study of the SADIE II
      Database", *Applied Sciences* 8(11), 2018. `convert_hrtf.py` writes that block itself,
      copying the licence and citation text out of the downloaded database's own documentation, so
      the shipped wording is York's and not a transcription from memory. The "Not present?" section
      says what a build does without the file: everything builds, every test runs, the host prints
      one stderr line naming the path it looked for, `RoomNode` renders through the spherical-head
      model, and the room panel shows "spherical head — front and back sound alike".
- [ ] **Step 5: Do not commit the 1.6 MB asset in this task.** The maintainer downloads SADIE II,
      runs the tool and commits `assets/hrtf/sadie2_ku100_48k.hrtf` himself, because he is the one
      who accepts the licence and checks the SHA-256 into the README.
- [ ] **Step 6: Commit** `git add tools/dsp/convert_hrtf.py assets/hrtf/README.md
      tools/dsp/README.md` then `git commit -m "DE5: convert SADIE II to the flat HRTF asset the
      room loads"`.

**Acceptance:** `python tools/dsp/convert_hrtf.py --self-test build/hrtf_fixture.hrtf` exits 0 and
writes a file of exactly 32 + 6 × (12 + 1024) + 32 × 1024 + 2 × 512 = 40 040 bytes, whose four
azimuths read back as 0, π/2, π and 3π/2 radians.

---

## Wave 2 (nine tasks, parallel, disjoint files)

Every task here consumes `BandResponse` from Task 1 or the core alone, and each owns its own pair
of files and its own test file. None of them touches `test_rt_safety.cpp`; the traps for the
audio-thread bricks land in Tasks 24, 25 and 27, which own that file.

### Task 3: `Materials`

**Waits on:** Task 1.

**Files:**
- Create: `modules/room/include/SushiDSP/room/Materials.hpp`, `modules/room/src/Materials.cpp`
- Test: `tests/functional/unit/test_room_materials.cpp`

**Interfaces:**
- Consumes: `BandResponse`, `kBandCount`.
- Produces:

```cpp
namespace SushiDSP::room
{
    enum class MaterialId : std::uint8_t
    {
        ConcreteUnpainted = 0, ConcreteBlockPainted, GypsumBoardOnStuds,
        HeavyCarpetOnConcrete, GlassWindow, OpenDoorway
    };
    inline constexpr std::size_t kMaterialCount = 6;

    /// @brief How much of a reflection leaves the surface diffusely. Spec §4.2.
    enum class Scattering : std::uint8_t { Flat = 0, Textured, Cluttered };

    struct Material
    {
        const char*                   name;
        std::array<float, kBandCount> absorption;   ///< Random-incidence alpha per octave band.
        const char*                   citation;     ///< The page the row was transcribed from.
    };

    const Material& material(MaterialId id) noexcept;

    /// @brief The pressure reflection factor sqrt(1 - alpha) per band, phase taken as zero.
    BandResponse reflection_factor(MaterialId id) noexcept;

    float scattering_coefficient(Scattering scattering) noexcept;
}
```

- [ ] **Step 1: Write the failing tests.**

```cpp
TEST(Unit_Dsp, MaterialsShipSixRowsAndNoMore)
{
    EXPECT_EQ(SushiDSP::room::kMaterialCount, 6u);
    for (std::uint8_t id = 0; id < 6u; ++id)
    {
        const SushiDSP::room::Material& row =
            SushiDSP::room::material(static_cast<SushiDSP::room::MaterialId>(id));
        ASSERT_NE(row.citation, nullptr);
        EXPECT_GT(std::strlen(row.citation), 0u);
    }
}

TEST(Unit_Dsp, MaterialsCarryTheTranscribedCoefficients)
{
    using SushiDSP::room::MaterialId;
    const std::array<float, 6> concrete = { 0.01f, 0.01f, 0.02f, 0.02f, 0.02f, 0.03f };
    const std::array<float, 6> carpet   = { 0.02f, 0.06f, 0.14f, 0.37f, 0.60f, 0.65f };
    const std::array<float, 6> glass    = { 0.35f, 0.25f, 0.18f, 0.12f, 0.07f, 0.04f };
    for (std::size_t band = 0; band < 6u; ++band)
    {
        EXPECT_FLOAT_EQ(SushiDSP::room::material(MaterialId::ConcreteUnpainted).absorption[band],
                        concrete[band]);
        EXPECT_FLOAT_EQ(SushiDSP::room::material(MaterialId::HeavyCarpetOnConcrete).absorption[band],
                        carpet[band]);
        EXPECT_FLOAT_EQ(SushiDSP::room::material(MaterialId::GlassWindow).absorption[band],
                        glass[band]);
    }
}

TEST(Unit_Dsp, MaterialsReflectionFactorIsTheSquareRootOfWhatIsLeft)
{
    const SushiDSP::room::BandResponse gypsum =
        SushiDSP::room::reflection_factor(SushiDSP::room::MaterialId::GypsumBoardOnStuds);
    EXPECT_NEAR(gypsum.gain[0], 0.842615f, 1e-6f);   // sqrt(1 - 0.29)
    EXPECT_NEAR(gypsum.gain[3], 0.979796f, 1e-6f);   // sqrt(1 - 0.04)
}

TEST(Unit_Dsp, MaterialsOpenDoorwayReflectsNothing)
{
    const SushiDSP::room::BandResponse open =
        SushiDSP::room::reflection_factor(SushiDSP::room::MaterialId::OpenDoorway);
    for (std::size_t band = 0; band < 6u; ++band) { EXPECT_FLOAT_EQ(open.gain[band], 0.0f); }
}

TEST(Unit_Dsp, MaterialsScatteringIsThreeWords)
{
    EXPECT_FLOAT_EQ(SushiDSP::room::scattering_coefficient(SushiDSP::room::Scattering::Flat), 0.10f);
    EXPECT_FLOAT_EQ(SushiDSP::room::scattering_coefficient(SushiDSP::room::Scattering::Textured), 0.25f);
    EXPECT_FLOAT_EQ(SushiDSP::room::scattering_coefficient(SushiDSP::room::Scattering::Cluttered), 0.40f);
}
```

- [ ] **Step 2: Confirm they fail**; paste the `clang -fsyntax-only` error.
- [ ] **Step 3: Implement.** One `static constexpr std::array<Material, kMaterialCount>` in the
      `.cpp`, each row's `citation` naming the source and the page. Copy the six rows from this
      plan's constants table. Add no seventh row.
- [ ] **Step 4: Syntax-check**, paste. **Step 5: Commit**
      `DE0: Materials ships six transcribed absorption rows and their reflection factors`.

**Acceptance:** `reflection_factor(GypsumBoardOnStuds)` is 0.842615 at 125 Hz and 0.979796 at
1 kHz, and `kMaterialCount` is 6.

---

### Task 4: `AirAbsorption`

**Waits on:** Task 1.

**Files:**
- Create: `modules/room/include/SushiDSP/room/AirAbsorption.hpp`,
  `modules/room/src/AirAbsorption.cpp`
- Test: `tests/functional/unit/test_air_absorption.cpp`

**Interfaces:**
- Consumes: `BandResponse`, `kBandCentresHz`.
- Produces:

```cpp
namespace SushiDSP::room
{
    /// @brief ISO 9613-1:1993 attenuation at the module's fixed atmosphere. Spec §4.2 and §11.
    class AirAbsorption
    {
    public:
        static constexpr double kTemperatureKelvin       = 293.15;
        static constexpr double kRelativeHumidityPercent = 50.0;
        static constexpr double kPressureKilopascal      = 101.325;

        static double attenuation_db_per_metre(double frequency_hz) noexcept;

        /// @brief The energy attenuation coefficient m in Eyring's denominator, per metre.
        static double energy_coefficient_per_metre(std::size_t band) noexcept;

        /// @brief The pressure gain a path of this length keeps, per band.
        static BandResponse over_path(double metres) noexcept;
    };
}
```

- [ ] **Step 1: Write the failing tests**, asserting this plan's table to 5 %:

```cpp
TEST(Unit_Dsp, AirAbsorptionMatchesIso9613AtTwentyDegreesAndFiftyPercent)
{
    const std::array<double, 6> expected =
        { 0.00044, 0.00131, 0.00273, 0.00466, 0.00989, 0.02967 };
    for (std::size_t band = 0; band < 6u; ++band)
    {
        const double computed = SushiDSP::room::AirAbsorption::attenuation_db_per_metre(
            SushiDSP::room::kBandCentresHz[band]);
        EXPECT_NEAR(computed, expected[band], 0.05 * expected[band]) << "band " << band;
    }
    EXPECT_NEAR(SushiDSP::room::AirAbsorption::attenuation_db_per_metre(8000.0), 0.1053, 0.006);
}

TEST(Unit_Dsp, AirAbsorptionEnergyCoefficientIsTheDecibelFigureOverFourPointThreeFourThree)
{
    const double four_kilohertz = SushiDSP::room::AirAbsorption::attenuation_db_per_metre(4000.0);
    EXPECT_NEAR(SushiDSP::room::AirAbsorption::energy_coefficient_per_metre(5u),
                four_kilohertz / (10.0 * std::log10(std::exp(1.0))), 1e-9);
    EXPECT_NEAR(SushiDSP::room::AirAbsorption::energy_coefficient_per_metre(5u), 6.831e-3, 3.5e-4);
    EXPECT_NEAR(SushiDSP::room::AirAbsorption::energy_coefficient_per_metre(0u), 1.01e-4, 5.1e-6);
}

TEST(Unit_Dsp, AirAbsorptionOverThirtyMetresCostsUnderOneDecibel)
{
    const SushiDSP::room::BandResponse thirty = SushiDSP::room::AirAbsorption::over_path(30.0);
    EXPECT_NEAR(thirty.decibels()[5], -0.89f, 0.05f);   // 30 * 0.02967
    EXPECT_GT(thirty.decibels()[0], -0.02f);
}

TEST(Unit_Dsp, AirAbsorptionOverZeroMetresIsUnity)
{
    const SushiDSP::room::BandResponse none = SushiDSP::room::AirAbsorption::over_path(0.0);
    for (std::size_t band = 0; band < 6u; ++band) { EXPECT_FLOAT_EQ(none.gain[band], 1.0f); }
}

TEST(Unit_Dsp, AirAbsorptionGrowsWithFrequencyAndDistance)
{
    const SushiDSP::room::BandResponse ten = SushiDSP::room::AirAbsorption::over_path(10.0);
    const SushiDSP::room::BandResponse twenty = SushiDSP::room::AirAbsorption::over_path(20.0);
    for (std::size_t band = 1; band < 6u; ++band)
    {
        EXPECT_LE(ten.gain[band], ten.gain[band - 1u]);
        EXPECT_LT(twenty.gain[band], ten.gain[band]);
    }
}
```

- [ ] **Step 2: Confirm they fail**; paste the error.
- [ ] **Step 3: Implement** the closed form from this plan's constants section verbatim, in
      `double`. `energy_coefficient_per_metre` caches the six values in a function-local
      `static const std::array<double, 6>`; `over_path` is `10^(-alpha * metres / 20)` per band.
- [ ] **Step 4: Syntax-check**, paste. **Step 5: Commit**
      `DE0: AirAbsorption evaluates ISO 9613-1 at the module's fixed atmosphere`.

**Acceptance:** the six computed dB/m figures match 0.00044, 0.00131, 0.00273, 0.00466, 0.00989
and 0.02967 within 5 %, and a 30 m path loses 0.89 dB at 4 kHz.

---

### Task 5: `BandResponseFit`

**Waits on:** Task 1. It also reads `Materials` in its test, so the controller builds Tasks 3 and 5
in the same build; if Task 3 is not finished, write the five material rows as literals in the test.

**Files:**
- Create: `modules/room/include/SushiDSP/room/BandResponseFit.hpp`,
  `modules/room/src/BandResponseFit.cpp`
- Test: `tests/functional/unit/test_band_response_fit.cpp`

**Interfaces:**
- Consumes: `BandResponse`, `SushiDSP::Biquad` from `include/SushiDSP/math/Biquad.hpp`
  (`set_coefficients(double b0, double b1, double b2, double a1, double a2)`).
- Produces:

```cpp
namespace SushiDSP::room
{
    /// @brief Two biquads whose cascade matches six band gains. Control thread; a few thousand
    /// flops per fit, and the audio thread never learns what a material is. Spec §4.2.
    class BandResponseFit
    {
    public:
        struct Coefficients
        {
            std::array<double, 5> section_a;   ///< b0, b1, b2, a1, a2.
            std::array<double, 5> section_b;
        };

        struct Result
        {
            Coefficients coefficients;
            double       worst_error_db;       ///< Largest |error| over the six centres.
        };

        static Result fit(const BandResponse& target, double sample_rate);

        /// @brief The cascade's magnitude in decibels at one frequency.
        static double magnitude_db(const Coefficients& coefficients, double frequency_hz,
                                   double sample_rate);
    };
}
```

- [ ] **Step 1: Write the failing tests.**

```cpp
TEST(Unit_Dsp, BandResponseFitClearsOneDecibelForEveryMaterialToEveryOrder)
{
    for (std::uint8_t id = 0; id < SushiDSP::room::kMaterialCount; ++id)
    {
        const auto material_id = static_cast<SushiDSP::room::MaterialId>(id);
        if (material_id == SushiDSP::room::MaterialId::OpenDoorway) { continue; }
        for (const double rate : { 44100.0, 48000.0, 96000.0, 192000.0 })
        {
            for (const double order : { 1.0, 2.0, 3.0, 4.0 })
            {
                const SushiDSP::room::BandResponse target =
                    SushiDSP::room::reflection_factor(material_id).powered(order);
                const SushiDSP::room::BandResponseFit::Result result =
                    SushiDSP::room::BandResponseFit::fit(target, rate);
                EXPECT_LE(result.worst_error_db, 1.0)
                    << "material " << int(id) << " order " << order << " rate " << rate;
                for (std::size_t band = 0; band < 6u; ++band)
                {
                    const double wanted =
                        20.0 * std::log10(std::max(target.gain[band], 1e-9f));
                    const double got = SushiDSP::room::BandResponseFit::magnitude_db(
                        result.coefficients, SushiDSP::room::kBandCentresHz[band], rate);
                    EXPECT_NEAR(got, wanted, 1.0);
                }
            }
        }
    }
}
```

That loop is 5 materials × 4 orders × 4 rates = 80 fits, and every one of them must clear 1 dB.

```cpp
TEST(Unit_Dsp, BandResponseFitReproducesAFlatTargetExactly)
{
    const SushiDSP::room::BandResponseFit::Result result =
        SushiDSP::room::BandResponseFit::fit(SushiDSP::room::BandResponse::unity(), 48000.0);
    EXPECT_LT(result.worst_error_db, 0.01);
    EXPECT_NEAR(SushiDSP::room::BandResponseFit::magnitude_db(result.coefficients, 1000.0, 48000.0),
                0.0, 0.01);
}

TEST(Unit_Dsp, BandResponseFitKeepsEveryPoleInsideTheUnitCircle)
{
    // For all 80 fits above: |a2| < 1 and |a1| < 1 + a2, which is the stability triangle.
}

TEST(Unit_Dsp, BandResponseFitCostsUnderTenMillisecondsForOneHundredAndFiftyFits)
{
    // 150 fits of the carpet row at 48 kHz, timed with std::chrono::steady_clock: under 1 ms,
    // which is the per-scene budget spec §4.1 gives the solver.
}
```

Write the last two out in full, the second as the same triple loop as the first.

- [ ] **Step 2: Confirm they fail**; paste the error.
- [ ] **Step 3: Implement.** Start from a closed-form guess: a low shelf through bands 0 and 1 and
      a high shelf through bands 4 and 5, both through `Biquad::set_low_shelf` and
      `set_high_shelf` at Q = 0.7071 with corners at 250 Hz and 2 kHz. Refine with at most eight
      Gauss-Newton steps in `double` on the six log-magnitude residuals; the free parameters are
      the two shelves' gains and corners plus one peaking section's gain, centre and Q folded into
      the spare biquad. Stop early when `worst_error_db` falls under 0.25, and reject any step that
      moves a pole out of the stability triangle. `magnitude_db` evaluates each section at
      z = exp(j·2π·f/fs) and sums the two in decibels.
- [ ] **Step 4: Syntax-check**, paste. **Step 5: Commit**
      `DE0: BandResponseFit turns six band gains into two biquads within a decibel`.

**Acceptance:** all 80 fits — five materials, orders 1 to 4, four sample rates — land within 1 dB
at every one of the six band centres, with every pole inside the unit circle.

---

### Task 6: `CabinetDirectivity`

**Waits on:** Task 1.

**Files:**
- Create: `modules/room/include/SushiDSP/room/CabinetDirectivity.hpp`,
  `modules/room/src/CabinetDirectivity.cpp`
- Test: `tests/functional/unit/test_cabinet_directivity.cpp`

**Interfaces:**
- Consumes: `BandResponse`, `kBandCentresHz`, `kSpeedOfSound`.
- Produces:

```cpp
namespace SushiDSP::room
{
    struct CabinetGeometry
    {
        double cone_radius_m;
        double driver_spacing_m;      ///< Zero for a single driver.
        double baffle_width_m;
        double breakup_frequency_hz;
    };

    inline constexpr CabinetGeometry kCabinet4x12{ 0.125, 0.30, 0.75, 2500.0 };
    inline constexpr CabinetGeometry kCabinet2x12{ 0.125, 0.30, 0.75, 2500.0 };
    inline constexpr CabinetGeometry kCabinet1x12{ 0.125, 0.00, 0.40, 2500.0 };

    /// @brief Four rigid pistons in an infinite baffle in front of a closed box. Spec §7.
    class CabinetDirectivity
    {
    public:
        explicit CabinetDirectivity(const CabinetGeometry& geometry);

        /// @brief Gain per band relative to on axis, for a direction in the cabinet's own frame.
        /// @param azimuth_rad 0 straight ahead, + to the cabinet's left.
        /// @param elevation_rad 0 on the baffle's axis, + up.
        BandResponse gain_for(double azimuth_rad, double elevation_rad) const noexcept;

        /// @brief The directivity index per band, integrated over a 2562-point spherical grid.
        /// Control thread.
        std::array<float, kBandCount> directivity_index_db() const;

        /// @brief 2*J1(x)/x, the rigid-piston factor, with the x -> 0 limit of 1.
        static double piston_factor(double x) noexcept;
    };
}
```

The model, pinned so nobody re-derives it:

- Effective radius `a(f) = cone_radius_m / sqrt(1 + (f / breakup_frequency_hz)^2)`.
- Piston: `D_p(f, theta) = piston_factor(k * a(f) * sin theta)`, `k = 2 pi f / kSpeedOfSound`,
  theta the angle off the baffle's axis.
- Array, a 2 × 2 grid of spacing d:
  `AF = |cos(k*d*sin(theta)*cos(phi)/2)| * |cos(k*d*sin(theta)*sin(phi)/2)|`, phi measured in the
  baffle plane from the horizontal. It is 1 on axis; on the horizontal axis its first null is at
  `c / (2*d)` = 571.7 Hz and its first grating lobe at `c / d` = 1143 Hz, which is the frequency
  §7 names.
- Rear: for |theta| > 90° the gain is multiplied by a first-order shelf falling from 0 dB below
  the baffle-step frequency `115 / baffle_width_m` Hz to −6 dB above it.

- [ ] **Step 1: Write the failing tests**, whose numbers come from that model:

```cpp
TEST(Unit_Dsp, CabinetDirectivityIsUnityOnAxis)
{
    const SushiDSP::room::CabinetDirectivity cabinet(SushiDSP::room::kCabinet4x12);
    const SushiDSP::room::BandResponse on_axis = cabinet.gain_for(0.0, 0.0);
    for (std::size_t band = 0; band < 6u; ++band) { EXPECT_NEAR(on_axis.gain[band], 1.0f, 1e-6f); }
}

TEST(Unit_Dsp, CabinetDirectivityBeamsAtTwoKilohertzAndNotAtOneTwentyFive)
{
    const SushiDSP::room::CabinetDirectivity cabinet(SushiDSP::room::kCabinet4x12);
    const SushiDSP::room::BandResponse off = cabinet.gain_for(M_PI / 3.0, 0.0);   // 60 degrees
    // 2 kHz: a(f) = 0.125 / sqrt(1 + 0.64) = 0.097614 m, k = 36.6300 /m,
    // k*a*sin60 = 3.0958 so the piston is 0.195099 (-14.19 dB), and the array is
    // |cos(36.6300 * 0.30 * 0.866025 / 2)| = 0.046834 (-26.59 dB).
    EXPECT_NEAR(off.decibels()[4], -40.78f, 0.10f);
    // 125 Hz: piston 0.992358, array 0.956086, together -0.46 dB.
    EXPECT_NEAR(off.decibels()[0], -0.46f, 0.10f);
}

TEST(Unit_Dsp, CabinetDirectivityPistonFactorMatchesTheBesselSeries)
{
    EXPECT_NEAR(SushiDSP::room::CabinetDirectivity::piston_factor(0.0), 1.0, 1e-12);
    EXPECT_NEAR(SushiDSP::room::CabinetDirectivity::piston_factor(1.0), 0.880101, 1e-6);
    EXPECT_NEAR(SushiDSP::room::CabinetDirectivity::piston_factor(3.831706), 0.0, 1e-4);
}

TEST(Unit_Dsp, CabinetDirectivityIndexIsZeroWhereTheConeIsOmnidirectional)
{
    // 125 Hz gives k*a = 0.286, so the index is under 0.5 dB; the 2 kHz index exceeds 10 dB.
}

TEST(Unit_Dsp, CabinetDirectivityIsSymmetricLeftAndRight)
{
    // gain_for(+x, 0) equals gain_for(-x, 0) within 1e-6 for x in { 0.2, 0.7, 1.4, 2.4 } radians.
}

TEST(Unit_Dsp, CabinetDirectivityFallsSixDecibelsBehindTheBaffleAboveTheStep)
{
    // gain_for(M_PI, 0).decibels()[5] - gain_for(M_PI, 0).decibels()[0] is -6 dB within 0.5.
}
```

Write the last three out in full.

- [ ] **Step 2: Confirm they fail**; paste the error.
- [ ] **Step 3: Implement.** `piston_factor` uses the ascending series for J1 to 20 terms below
      x = 12 and the asymptotic form above it, and returns 1.0 for |x| < 1e-12.
      `directivity_index_db` integrates |D|² over a 2562-point grid — an icosahedron subdivided
      four times, built in the constructor — and reports `10*log10(|D(0)|^2 / mean)`.
- [ ] **Step 4: Syntax-check**, paste. **Step 5: Commit**
      `DE4: CabinetDirectivity evaluates a four-piston baffle per band and direction`.

**Acceptance:** the 4x12 at 60° off axis is 40.78 dB down at 2 kHz and 0.46 dB down at 125 Hz,
each within 0.10 dB of the closed form written into the test.

---

### Task 7: `MicrophoneModel`

**Waits on:** Task 1.

**Files:**
- Create: `modules/room/include/SushiDSP/room/MicrophoneModel.hpp`,
  `modules/room/src/MicrophoneModel.cpp`
- Test: `tests/functional/unit/test_microphone_model.cpp`

**Interfaces:**
- Consumes: `BandResponse`, `kBandCentresHz`, `kSpeedOfSound`.
- Produces:

```cpp
namespace SushiDSP::room
{
    enum class PolarPattern : std::uint8_t { Omni = 0, Cardioid, Supercardioid, FigureEight };

    /// @brief The pressure share a of P(theta) = a + (1 - a) cos(theta): 1, 0.5, 0.366, 0.
    double pattern_pressure_share(PolarPattern pattern) noexcept;

    struct MicrophonePreset
    {
        const char*                   name;
        PolarPattern                  pattern;
        std::array<float, kBandCount> on_axis_db;   ///< Transcribed from the maker's sheet.
        const char*                   citation;
    };

    /// @brief A first-order pattern with its proximity effect. Spec §8.
    class MicrophoneModel
    {
    public:
        MicrophoneModel(PolarPattern pattern,
                        const std::array<float, kBandCount>& on_axis_db) noexcept;

        /// @brief Pattern times proximity times the on-axis curve, per band.
        /// @param incidence_rad 0 on axis.
        /// @param distance_m Path length from the source to the capsule.
        BandResponse gain_for(double incidence_rad, double distance_m) const noexcept;

        /// @brief sqrt(1 + (c / (2 pi f r))^2), the gradient term's spherical-wave boost.
        static double proximity_boost(double frequency_hz, double distance_m) noexcept;
    };

    inline constexpr MicrophonePreset kMicrophoneFlatCardioid
        { "Cardioid, flat", PolarPattern::Cardioid, { 0, 0, 0, 0, 0, 0 }, "no maker's curve" };
    inline constexpr std::size_t kMicrophonePresetCount = 1;
}
```

**The boost multiplies the gradient half of the pattern and not the pressure half.** A capsule at
distance r and incidence theta has band gain `a + (1 - a) * cos(theta) * B(f, r)`, normalised by
its own far-field value `a + (1 - a) * cos(theta)`. That is what makes the 15.5 dB of §8.1 come
out; the pure gradient form would give 20.8 dB. State it as an invariant comment on `gain_for`.

**On the SM57 preset:** the spec asks for one named preset transcribed from Shure's published
curve. This plan does not have those six numbers. Ship `kMicrophonePresetCount = 1` with the flat
cardioid, ask the maintainer for the six values, and add the row in a follow-up commit when he
supplies them. **Do not invent a curve.**

- [ ] **Step 1: Write the failing tests.**

```cpp
TEST(Unit_Dsp, MicrophoneModelCardioidAtFiveCentimetresGainsFifteenPointFiveDecibels)
{
    const SushiDSP::room::MicrophoneModel cardioid(
        SushiDSP::room::PolarPattern::Cardioid, { 0, 0, 0, 0, 0, 0 });
    const float near_db = cardioid.gain_for(0.0, 0.05).decibels()[0];
    const float far_db  = cardioid.gain_for(0.0, 3.00).decibels()[0];
    EXPECT_NEAR(near_db - far_db, 15.47f, 0.50f);
}

TEST(Unit_Dsp, MicrophoneModelProximityBoostIsTheSphericalWaveTerm)
{
    // f_p = c / (2 pi r) is 1091.8 Hz at 5 cm and 182.0 Hz at 30 cm.
    EXPECT_NEAR(SushiDSP::room::MicrophoneModel::proximity_boost(100.0, 0.05), 10.9637, 1e-4);
    EXPECT_NEAR(SushiDSP::room::MicrophoneModel::proximity_boost(100.0, 0.30), 2.0736, 1e-4);
    EXPECT_NEAR(SushiDSP::room::MicrophoneModel::proximity_boost(1000.0, 3.0), 1.00001, 1e-5);
}

TEST(Unit_Dsp, MicrophoneModelCardioidAtThirtyCentimetresGainsThreePointSevenDecibels)
{
    // (0.5 + 0.5 * 2.0736) = 1.5368, which is 3.74 dB over the far-field value of 1.
    // Against the same capsule at 3 m (0.07 dB) the difference is 3.67 dB.
}

TEST(Unit_Dsp, MicrophoneModelFigureEightRejectsNinetyDegreesByFortyDecibels)
{
    // cos(pi/2) is exactly 0 in double, so the model must return a true zero and the -200 dB
    // floor, not a NaN; every band is at least 40 dB below the on-axis value.
}

TEST(Unit_Dsp, MicrophoneModelOmniIgnoresIncidenceAndDistanceColour)
{
    // an omni's six band gains are identical at 0, 60 and 180 degrees and at 0.05 m and 3 m.
}

TEST(Unit_Dsp, MicrophoneModelAppliesTheOnAxisCurve)
{
    // a cardioid built with { 0, 0, 0, 0, 3, 6 } reads 3 dB more at 2 kHz and 6 dB more at 4 kHz
    // than the same capsule with a flat curve, at every distance.
}
```

Write the last four out in full.

- [ ] **Step 2: Confirm they fail**; paste the error.
- [ ] **Step 3: Implement** exactly the formula above, with `pattern_pressure_share` returning
      1.0, 0.5, 0.366 and 0.0.
- [ ] **Step 4: Syntax-check**, paste. **Step 5: Commit**
      `DE4: MicrophoneModel evaluates a first-order pattern and its proximity effect`.

**Acceptance:** a cardioid on axis at 5 cm reads 15.47 dB more at 100 Hz than the same capsule at
3 m, within 0.5 dB, and 3.67 dB more at 30 cm.

---

### Task 8: the three records that cross the thread boundary

**Waits on:** nothing beyond the core.

**Files:**
- Create: `modules/room/include/SushiDSP/room/EarlyTapSet.hpp`,
  `modules/room/include/SushiDSP/room/FdnDesign.hpp`,
  `modules/room/include/SushiDSP/room/ModalDesign.hpp` (header-only, plain data)
- Test: `tests/functional/unit/test_room_records.cpp`

**Interfaces:**
- Consumes: nothing.
- Produces:

```cpp
namespace SushiDSP::room
{
    /// @brief One image source, reduced to what the audio thread reads.
    struct EarlyTap
    {
        float                 delay_frames;    ///< Fractional, from the ring's write head.
        float                 gain;            ///< 1/r times the specular share times the level.
        std::array<double, 5> section_a;       ///< b0, b1, b2, a1, a2 of the fitted filter.
        std::array<double, 5> section_b;
        std::array<float, 16> encode;          ///< ACN/SN3D order-3 gains, or {pattern, 0...}.
    };

    struct EarlyTapSet
    {
        static constexpr std::size_t kMaxEarlyTaps = 128;
        std::array<EarlyTap, kMaxEarlyTaps> taps;
        std::uint32_t count;                   ///< Taps in use; the rest are never read.
        float         crossover_time_s;        ///< t_c, where the tail takes over.
        float         scattered_energy;        ///< Energy handed to the tail's input, linear.
    };

    struct FdnDesign
    {
        static constexpr std::size_t kLineCount = 16;
        std::array<std::uint32_t, kLineCount>        line_frames;
        std::array<std::array<double, 5>, kLineCount> line_section_a;
        std::array<std::array<double, 5>, kLineCount> line_section_b;
        static constexpr std::size_t kOutputSections = 2;
        using Section = std::array<double, 5>;
        using Cascade = std::array<Section, kOutputSections>;   ///< One 4th-order filter.
        Cascade tone_correction;               ///< Both sections of the BandResponseFit.
        Cascade coherence_a;                   ///< The gamma filter of spec §6.4.
        Cascade coherence_b;                   ///< sqrt(1 - gamma^2).
        float input_gain;
        float delay_to_crossover_frames;
    };

    struct ModalDesign
    {
        static constexpr std::size_t kMaxModes = 512;
        struct Mode
        {
            std::array<double, 5> section;     ///< A resonator at the mode's frequency and damping.
            float                 source_gain; ///< psi_n at the cabinet.
            std::array<float, 2>  ear_gain;    ///< psi_n at the left and the right ear.
        };
        std::array<Mode, kMaxModes> modes;
        std::uint32_t count;
        float crossover_hz;                    ///< f_s; zero means the layer is off.
        float level_scale;                     ///< Matches the layer to Eyring at the seam.
    };
}
```

- [ ] **Step 1: Write the failing tests.**

```cpp
TEST(Unit_Dsp, RoomRecordsAreTriviallyCopyable)
{
    static_assert(std::is_trivially_copyable<SushiDSP::room::EarlyTap>::value, "the audio thread reads it");
    static_assert(std::is_trivially_copyable<SushiDSP::room::EarlyTapSet>::value, "the audio thread reads it");
    static_assert(std::is_trivially_copyable<SushiDSP::room::FdnDesign>::value, "the audio thread reads it");
    static_assert(std::is_trivially_copyable<SushiDSP::room::ModalDesign>::value, "the audio thread reads it");
    SUCCEED();
}

TEST(Unit_Dsp, RoomRecordsCarryTheSpecsCapacities)
{
    EXPECT_EQ(SushiDSP::room::EarlyTapSet::kMaxEarlyTaps, 128u);
    EXPECT_EQ(SushiDSP::room::FdnDesign::kLineCount, 16u);
    EXPECT_EQ(SushiDSP::room::ModalDesign::kMaxModes, 512u);
}

TEST(Unit_Dsp, RoomRecordsAreSmallEnoughToPublishPerGuiFrame)
{
    // One EarlyTap is 4 + 4 + 40 + 40 + 64 = 152 bytes, so a 128-tap set is 19456 plus 12 of
    // header; one Mode is 40 + 4 + 8 = 52, so 512 of them are 26624 plus 12.
    EXPECT_LT(sizeof(SushiDSP::room::EarlyTapSet), 24u * 1024u);
    EXPECT_LT(sizeof(SushiDSP::room::ModalDesign), 64u * 1024u);
}
```

- [ ] **Step 2: Confirm they fail**; paste the error.
- [ ] **Step 3: Implement** the three headers. No constructor, no method, no include beyond
      `<array>` and `<cstdint>`.
- [ ] **Step 4: Syntax-check**, paste. **Step 5: Commit**
      `DE0: the three records the room's control thread publishes to its audio thread`.

**Acceptance:** all three records are trivially copyable and carry the capacities 128, 16 and 512.

---

### Task 9: `CrossoverPair`

**Waits on:** nothing beyond the core.

**Files:**
- Create: `modules/room/include/SushiDSP/room/CrossoverPair.hpp`,
  `modules/room/src/CrossoverPair.cpp`
- Test: `tests/functional/unit/test_crossover_pair.cpp`

**Interfaces:**
- Consumes: `SushiDSP::Biquad`.
- Produces:

```cpp
namespace SushiDSP::room
{
    /// @brief A fourth-order Linkwitz-Riley split whose halves sum to a flat magnitude, so the
    /// seam between the wave layer and the geometric layers adds no dip and no hump. Spec §9a.4.
    class CrossoverPair
    {
    public:
        void set_crossover(double sample_rate, double crossover_hz) noexcept;

        /// @brief Splits one sample. Audio thread; two cascaded Butterworth pairs per half.
        void process_sample(float input, float& low_out, float& high_out) noexcept;

        void reset() noexcept;
    };
}
```

- [ ] **Step 1: Write the failing tests.**

```cpp
TEST(Unit_Dsp, CrossoverPairSumsToUnityMagnitudeEverywhere)
{
    SushiDSP::room::CrossoverPair crossover;
    crossover.set_crossover(48000.0, 278.2);
    // Drive one unit impulse, sum the two halves into an 8192-point buffer, take its FFT with
    // SushiDSP::Fft: every bin from 20 Hz to 20 kHz is within 0.1 dB of 0 dB.
}

TEST(Unit_Dsp, CrossoverPairIsMinusSixDecibelsPerHalfAtTheCrossover)
{
    // each half's magnitude at 278.2 Hz is -6.02 dB within 0.1, which is what LR4 means.
}

TEST(Unit_Dsp, CrossoverPairFallsTwentyFourDecibelsPerOctave)
{
    // the low half at 4 * 278.2 = 1112.8 Hz is 48 dB below its passband, within 1 dB.
}

TEST(Unit_Dsp, CrossoverPairResetClearsItsState)
{
    // after a full-scale impulse and reset(), 64 silent samples come out below -300 dBFS.
}

TEST(Unit_Dsp, CrossoverPairAtZeroCrossoverPassesEverythingHigh)
{
    // set_crossover(48000.0, 0.0) puts the whole signal in high_out and exact zeros in low_out,
    // which is how spec §9a.4 switches the modal layer off under 40 Hz.
}
```

Write all five out in full.

- [ ] **Step 2: Confirm they fail**; paste the error.
- [ ] **Step 3: Implement.** Two `Biquad` low-passes at Q = 0.7071 in series for the low half and
      two high-passes for the high half, both at `crossover_hz`. State the sign convention as an
      invariant comment: at fourth order the two halves are in phase and add directly, with no
      inversion.
- [ ] **Step 4: Syntax-check**, paste. **Step 5: Commit**
      `DE3: CrossoverPair splits the field at the Schroeder frequency and sums back flat`.

**Acceptance:** the two halves sum within 0.1 dB of unity from 20 Hz to 20 kHz at a 278.2 Hz
crossover, and each half is 6.02 dB down at the crossover itself.

---

### Task 10: `HrtfSet`

**Waits on:** Task 2 for the byte layout, which this plan already states in full, so the two can be
written at the same time.

**Files:**
- Create: `modules/room/include/SushiDSP/room/HrtfSet.hpp`, `modules/room/src/HrtfSet.cpp`
- Test: `tests/functional/unit/test_hrtf_set.cpp`

**Interfaces:**
- Consumes: the standard library only.
- Produces:

```cpp
namespace SushiDSP::room
{
    /// @brief Minimum-phase HRIR pairs on a direction grid, their ITDs, and the triangle table
    /// they interpolate on. Control thread; the audio thread interpolates nothing. Spec §9.3.
    class HrtfSet
    {
    public:
        static constexpr std::uint32_t kTapCount    = 128;
        static constexpr std::uint32_t kDecoderTaps = 256;
        static constexpr std::uint32_t kDiffuseTaps = 128;

        struct Direction
        {
            float azimuth_rad;
            float elevation_rad;
            float itd_frames;      ///< Positive means the right ear is later.
        };

        struct Pair
        {
            std::array<float, kTapCount> left;
            std::array<float, kTapCount> right;
            float                        itd_frames;
        };

        HrtfSet();
        HrtfSet(double sample_rate, std::vector<Direction> directions,
                std::vector<float> left_taps, std::vector<float> right_taps);

        bool          empty() const noexcept;
        double        sample_rate() const noexcept;
        std::uint32_t direction_count() const noexcept;
        const Direction& direction(std::uint32_t index) const noexcept;

        /// @brief Blends the three grid directions around one direction by barycentric weight.
        /// @throw std::runtime_error when the set is empty.
        Pair interpolate(double azimuth_rad, double elevation_rad) const;

        /// @brief The 2*(order+1)^2 decoder filters, ACN order, left ear then right.
        const std::vector<float>& decoder_filters() const noexcept;
        std::uint32_t             decoder_order() const noexcept;
        void set_decoder(std::uint32_t order, std::vector<float> filters);

        /// @brief One 128-tap diffuse-field average per ear, for the tail. Spec §6.4.
        const std::array<float, kDiffuseTaps>& diffuse_field(std::uint32_t ear) const noexcept;
        void set_diffuse_field(std::array<float, kDiffuseTaps> left,
                               std::array<float, kDiffuseTaps> right);
    };
}
```

- [ ] **Step 1: Write the failing tests** on a synthetic six-direction set — the ±x, ±y and ±z axes
      — whose left tap 0 is the direction's index, so interpolation is checkable by arithmetic:

```cpp
TEST(Unit_Dsp, HrtfSetInterpolationAtAGridPointReturnsThatPointExactly)
{
    // a direction equal to grid direction 3 returns that pair's taps bit for bit and its exact ITD.
}

TEST(Unit_Dsp, HrtfSetInterpolationBetweenThreePointsIsABarycentricBlend)
{
    // the centroid of the (+x, +y, +z) triangle returns each tap within 1e-6 of the mean of the
    // three, and its ITD within 1e-6 of their mean.
}

TEST(Unit_Dsp, HrtfSetInterpolationWeightsSumToOne)
{
    // for 500 pseudo-random directions the returned tap 0 stays between the smallest and largest
    // tap 0 of its three neighbours, which is what a partition of unity guarantees.
}

TEST(Unit_Dsp, HrtfSetIsEmptyWhenDefaultConstructed)
{
    // empty() is true, direction_count() is 0, and interpolate() throws std::runtime_error.
}

TEST(Unit_Dsp, HrtfSetKeepsItsDecoderAndDiffuseFilters)
{
    // set_decoder(3, std::vector<float>(32 * 256)): decoder_order() is 3 and
    // decoder_filters().size() is 8192.
}
```

Write all five out in full.

- [ ] **Step 2: Confirm they fail**; paste the error.
- [ ] **Step 3: Implement.** The triangle table is built once at construction by an incremental
      convex hull over the direction points on the unit sphere. `interpolate` finds the containing
      triangle by testing the three edge half-spaces, then blends by barycentric weight. A grid
      that does not cover the sphere is a programming error the constructor's `@pre` names.
- [ ] **Step 4: Syntax-check**, paste. **Step 5: Commit**
      `DE5: HrtfSet holds minimum-phase pairs, ITDs and the triangle table they blend on`.

**Acceptance:** interpolating at a grid direction returns that pair bit for bit, and at a
triangle's centroid returns the mean of its three pairs within 1e-6.

---

### Task 11: `AmbisonicEncoder`

**Waits on:** nothing beyond the core.

**Files:**
- Create: `modules/room/include/SushiDSP/room/AmbisonicEncoder.hpp`,
  `modules/room/src/AmbisonicEncoder.cpp`
- Test: `tests/functional/unit/test_ambisonic_encoder.cpp`

**Interfaces:**
- Consumes: nothing.
- Produces:

```cpp
namespace SushiDSP::room
{
    /// @brief Third-order ACN/SN3D encode gains for one direction, the convention SADIE's
    /// published decoders use. Spec §5.1 and §9.1.
    class AmbisonicEncoder
    {
    public:
        static constexpr std::uint32_t kOrder    = 3;
        static constexpr std::uint32_t kChannels = 16;   ///< (kOrder + 1)^2.

        /// @brief Fills gains with the 16 ACN-ordered SN3D coefficients for a direction.
        /// @param azimuth_rad 0 front, + to the left.
        /// @param elevation_rad 0 horizontal, + up.
        static void encode(double azimuth_rad, double elevation_rad,
                           std::array<float, kChannels>& gains) noexcept;
    };
}
```

- [ ] **Step 1: Write the failing tests.** The two anchor vectors are the SN3D harmonics evaluated
      at those two directions, and they are what makes this test worth writing:

```cpp
TEST(Unit_Dsp, AmbisonicEncoderMatchesTheAcnSn3dHarmonicsInFront)
{
    std::array<float, 16> gains{};
    SushiDSP::room::AmbisonicEncoder::encode(0.0, 0.0, gains);
    const std::array<float, 16> expected = {
        1.0f, 0.0f, 0.0f, 1.0f,
        0.0f, 0.0f, -0.5f, 0.0f, 0.866025f,
        0.0f, 0.0f, 0.0f, 0.0f, -0.612372f, 0.0f, 0.790569f };
    for (std::size_t channel = 0; channel < 16u; ++channel)
    {
        EXPECT_NEAR(gains[channel], expected[channel], 1e-5f) << "channel " << channel;
    }
}

TEST(Unit_Dsp, AmbisonicEncoderMatchesTheAcnSn3dHarmonicsToTheLeft)
{
    std::array<float, 16> gains{};
    SushiDSP::room::AmbisonicEncoder::encode(M_PI / 2.0, 0.0, gains);
    const std::array<float, 16> expected = {
        1.0f, 1.0f, 0.0f, 0.0f,
        0.0f, 0.0f, -0.5f, 0.0f, -0.866025f,
        -0.790569f, 0.0f, -0.612372f, 0.0f, 0.0f, 0.0f, 0.0f };
    for (std::size_t channel = 0; channel < 16u; ++channel)
    {
        EXPECT_NEAR(gains[channel], expected[channel], 1e-5f) << "channel " << channel;
    }
}

TEST(Unit_Dsp, AmbisonicEncoderKeepsChannelZeroAtOneEverywhere)
{
    // SN3D normalisation: gains[0] is exactly 1.0f for 200 pseudo-random directions.
}

TEST(Unit_Dsp, AmbisonicEncoderIsOrthonormalOverTheSphere)
{
    // over a 2562-point spherical grid the mean of gains[i] * gains[j] is 1/(2n+1) on the
    // diagonal within 1e-3, n being that channel's order, and 0 off it within 1e-3.
}
```

Write the last two out in full.

- [ ] **Step 2: Confirm they fail**; paste the error.
- [ ] **Step 3: Implement** the closed-form SN3D harmonics in ACN order, with
      `x = cos(el)*cos(az)`, `y = cos(el)*sin(az)`, `z = sin(el)`: ACN 0 = 1; 1 = y, 2 = z, 3 = x;
      4 = sqrt(3)*x*y, 5 = sqrt(3)*y*z, 6 = (3z² − 1)/2, 7 = sqrt(3)*x*z,
      8 = (sqrt(3)/2)(x² − y²); 9 = sqrt(5/8)*y(3x² − y²), 10 = sqrt(15)*x*y*z,
      11 = sqrt(3/8)*y(5z² − 1), 12 = z(5z² − 3)/2, 13 = sqrt(3/8)*x(5z² − 1),
      14 = (sqrt(15)/2)*z(x² − y²), 15 = sqrt(5/8)*x(x² − 3y²).
- [ ] **Step 4: Syntax-check**, paste. **Step 5: Commit**
      `DE5: AmbisonicEncoder produces third-order ACN/SN3D gains for a direction`.

**Acceptance:** the front direction encodes to
`{1, 0, 0, 1, 0, 0, −0.5, 0, 0.866025, 0, 0, 0, 0, −0.612372, 0, 0.790569}` within 1e-5.

---

## Wave 3 (seven tasks, parallel, disjoint files)

### Task 12: `RoomScene` and the reference rooms

**Waits on:** Tasks 3, 6, 7.

**Files:**
- Create: `modules/room/include/SushiDSP/room/RoomScene.hpp`, `modules/room/src/RoomScene.cpp`,
  `tests/common/RoomFixtures.hpp`
- Test: `tests/functional/unit/test_room_scene.cpp`

**Interfaces:**
- Consumes: `MaterialId`, `Scattering`, `CabinetGeometry`, `PolarPattern`.
- Produces:

```cpp
namespace SushiDSP::room
{
    struct Vec3 { double x; double y; double z; };

    /// @brief Which face of the shoebox a surface index names.
    enum class Surface : std::uint8_t { MinusX = 0, PlusX, MinusY, PlusY, Floor, Ceiling };
    inline constexpr std::size_t kSurfaceCount = 6;

    /// @brief The whole room, control-thread owned, written by the host and read by every solver.
    struct RoomScene
    {
        static constexpr double kMinDimension = 1.5;
        static constexpr double kMaxDimension = 12.0;
        static constexpr std::size_t kMaxPatches = 4;

        /// @brief A patch of a different material on one face: the open doorway, the window.
        struct Patch { Surface surface; MaterialId material; double area_m2; };

        Vec3 dimensions{ 6.0, 4.0, 2.5 };
        std::array<MaterialId, kSurfaceCount> surfaces{};
        std::array<Scattering, kSurfaceCount> scattering{};
        std::array<Patch, kMaxPatches> patches{};
        std::uint32_t patch_count = 0;

        Vec3            source_position{ 1.5, 2.0, 0.5 };
        double          source_yaw_rad = 0.0;
        CabinetGeometry cabinet = kCabinet4x12;

        Vec3   listener_position{ 4.0, 2.0, 1.2 };
        double listener_yaw_rad = 0.0;

        Vec3         microphone_position{ 1.6, 2.0, 0.5 };
        double       microphone_yaw_rad = 0.0;
        PolarPattern microphone_pattern = PolarPattern::Cardioid;

        double volume_m3() const noexcept;
        double surface_area_m2() const noexcept;

        /// @brief Area-weighted absorption per band over the six faces and their patches.
        std::array<double, kBandCount> mean_absorption() const;

        /// @brief Whether every dimension is inside the bounds and every pose inside the room.
        /// @param reason Filled with the first violation when the scene is refused.
        bool is_valid(std::string& reason) const;
    };
}
```

and, in `tests/common/RoomFixtures.hpp`, the three rooms of this plan's constants section:

```cpp
namespace SushiDSP::test
{
    SushiDSP::room::RoomScene garage_scene();      ///< 6.0 x 4.0 x 2.5, concrete, 6 m2 doorway.
    SushiDSP::room::RoomScene bedroom_scene();     ///< 4.0 x 3.5 x 2.6, carpet, gypsum, window.
    SushiDSP::room::RoomScene block_room_scene();  ///< 5.0 x 4.0 x 2.7, painted concrete block.
}
```

- [ ] **Step 1: Write `tests/common/RoomFixtures.hpp`** with those three functions, each setting
      dimensions, the six surfaces and the patches exactly as tabulated.
- [ ] **Step 2: Write the failing tests.**

```cpp
TEST(Unit_Dsp, RoomSceneComputesTheGaragesVolumeAndArea)
{
    const SushiDSP::room::RoomScene garage = SushiDSP::test::garage_scene();
    EXPECT_NEAR(garage.volume_m3(), 60.0, 1e-9);
    EXPECT_NEAR(garage.surface_area_m2(), 98.0, 1e-9);
}

TEST(Unit_Dsp, RoomSceneWeightsAPatchByItsArea)
{
    // 92 m2 of unpainted concrete and 6 m2 of open doorway over 98 m2:
    // 125 Hz (92 * 0.01 + 6 * 1.0) / 98 = 0.070612; 500 Hz (92 * 0.02 + 6 * 1.0) / 98 = 0.080000.
    const std::array<double, 6> absorption = SushiDSP::test::garage_scene().mean_absorption();
    EXPECT_NEAR(absorption[0], 0.070612, 1e-6);
    EXPECT_NEAR(absorption[2], 0.080000, 1e-6);
}

TEST(Unit_Dsp, RoomSceneComputesTheBedroomsWeightedAbsorption)
{
    // 14 m2 carpet, 51.5 m2 gypsum, 1.5 m2 glass over 67 m2:
    // 500 Hz (14 * 0.14 + 51.5 * 0.05 + 1.5 * 0.18) / 67 = 0.071716.
    EXPECT_NEAR(SushiDSP::test::bedroom_scene().mean_absorption()[2], 0.071716, 1e-6);
}

TEST(Unit_Dsp, RoomSceneRefusesADimensionOutsideItsBounds)
{
    // 1.0 m and 15.0 m are both refused, and reason names the axis and both bounds.
}

TEST(Unit_Dsp, RoomSceneRefusesAPoseOutsideTheRoom)
{
    // a source at x = -0.1 and one at x = 6.1 in the garage are refused, naming the coordinate.
}

TEST(Unit_Dsp, RoomSceneRefusesAPatchLargerThanItsFace)
{
    // a 12 m2 doorway on the 10 m2 +x wall is refused.
}
```

Write the last three out in full.

- [ ] **Step 3: Confirm they fail**; paste the error.
- [ ] **Step 4: Implement.** `mean_absorption` subtracts each patch's area from its face's area
      before weighting, so a patch replaces rather than adds. State that as an invariant comment.
- [ ] **Step 5: Syntax-check**, paste. **Step 6: Commit**
      `DE0: RoomScene is the shoebox, its materials, its patches and the three poses`.

**Acceptance:** the garage's area-weighted absorption is 0.070612 at 125 Hz and 0.080000 at 500 Hz,
and its volume and surface area are 60.0 m³ and 98.0 m².

---

### Task 13: `EarlyReflectionRenderer`

**Waits on:** Task 8.

**Files:**
- Create: `modules/room/include/SushiDSP/room/EarlyReflectionRenderer.hpp`,
  `modules/room/src/EarlyReflectionRenderer.cpp`
- Test: `tests/functional/unit/test_early_reflection_renderer.cpp`

**Interfaces:**
- Consumes: `EarlyTapSet`, `EarlyTap`, `SushiDSP::Biquad`, `SushiDSP::DenormalGuard`.
- Produces:

```cpp
namespace SushiDSP::room
{
    /// @brief One input ring, 128 fractional reads, two biquads and an encode per tap. Audio
    /// thread: allocation-free after prepare(). Spec §5.1.
    class EarlyReflectionRenderer
    {
    public:
        static constexpr std::uint32_t kOutputChannels = 16;

        /// @param max_delay_frames Sized for the longest t_c the scene bounds allow:
        /// 5 * 12.0 / 343.0 s, which is 8397 frames at 48 kHz.
        void prepare(std::uint32_t max_delay_frames, std::uint32_t max_block_frames);
        void reset() noexcept;

        /// @brief Writes one block into the ring. Call once per block, before render().
        void write(const float* input, std::uint32_t frames) noexcept;

        /// @brief Adds this set's contribution into 16 accumulation buffers.
        /// @param bank 0 or 1: which biquad state bank to run, so a crossfade renders both.
        void render(const EarlyTapSet& set, std::uint32_t bank,
                    float* const* outputs, std::uint32_t frames) noexcept;

        /// @brief Zeroes one state bank, for the set arriving into a crossfade. Audio thread.
        void clear_bank(std::uint32_t bank) noexcept;
    };
}
```

- [ ] **Step 1: Write the failing tests**, driving hand-written tap sets so the renderer is tested
      with no solver present:

```cpp
TEST(Unit_Dsp, EarlyReflectionRendererPutsAnImpulseAtTheTapsDelay)
{
    SushiDSP::room::EarlyTapSet set{};
    set.count = 1u;
    set.taps[0].delay_frames = 100.0f;
    set.taps[0].gain = 1.0f;
    set.taps[0].section_a = { 1.0, 0.0, 0.0, 0.0, 0.0 };   // a pass-through biquad
    set.taps[0].section_b = { 1.0, 0.0, 0.0, 0.0, 0.0 };
    set.taps[0].encode.fill(0.0f);
    set.taps[0].encode[0] = 1.0f;
    // A unit impulse in, one 256-frame block out: channel 0 holds exactly 1.0f at frame 100 and
    // stays under 1e-6 everywhere else.
}

TEST(Unit_Dsp, EarlyReflectionRendererInterpolatesAFractionalDelay)
{
    // delay_frames 100.5 through the 4-tap Lagrange kernel: frames 100 and 101 are both above
    // 0.4, and a 1 kHz sine read at 100.5 lags the same sine read at 100.0 by half a sample,
    // measured by cross-correlation to 0.01 frames.
}

TEST(Unit_Dsp, EarlyReflectionRendererSumsOneHundredAndTwentyEightTaps)
{
    // 128 taps of gain 1/128 at delays 100 to 227: the output is a plateau of height 1/128 that
    // is 128 frames long, and its sum is exactly 1.0 within 1e-5.
}

TEST(Unit_Dsp, EarlyReflectionRendererEncodesIntoSixteenChannels)
{
    // one tap with encode = { 1, 0, 0, 1, 0... }: channels 0 and 3 carry the impulse and the
    // other fourteen stay below 1e-9.
}

TEST(Unit_Dsp, EarlyReflectionRendererKeepsTwoIndependentStateBanks)
{
    // render bank 0 with a resonant filter for ten blocks, clear_bank(1), then render bank 1 with
    // the same set: bank 1's first block equals bank 0's first block bit for bit.
}

TEST(Unit_Dsp, EarlyReflectionRendererCarriesATailAcrossBlockBoundaries)
{
    // a tap at delay 300 with 256-frame blocks appears at frame 44 of the second block, not in
    // the first.
}
```

Write the last five out in full.

- [ ] **Step 2: Confirm they fail**; paste the error.
- [ ] **Step 3: Implement.** One `std::vector<float>` ring sized in `prepare()`, a write head, and
      a tap-outer sample-inner loop: read the four Lagrange neighbours, apply the gain, run the two
      biquads in `double` from the chosen bank's state, then sixteen multiply-adds into the
      accumulation buffers. Two banks of `2 * kMaxEarlyTaps` biquad states, both allocated in
      `prepare()`. Nothing in `write`, `render` or `clear_bank` allocates. Lay the coefficients out
      structure-of-arrays so the auto-vectoriser has a chance at the Lagrange and encode steps; the
      SIMD lanes themselves are DE7 and are not written here.
- [ ] **Step 4: Syntax-check**, paste. **Step 5: Commit**
      `DE1: EarlyReflectionRenderer reads 128 fractional taps into 16 Ambisonic channels`.

**Acceptance:** a single tap at `delay_frames = 100.0` with a pass-through filter puts exactly
1.0f at output frame 100 and stays under 1e-6 everywhere else in the block.

---

### Task 14: `LateReverb`

**Waits on:** Task 8.

**Files:**
- Create: `modules/room/include/SushiDSP/room/LateReverb.hpp`, `modules/room/src/LateReverb.cpp`
- Test: `tests/functional/unit/test_late_reverb.cpp`

**Interfaces:**
- Consumes: `FdnDesign`, `SushiDSP::Biquad`, `SushiDSP::DenormalGuard`.
- Produces:

```cpp
namespace SushiDSP::room
{
    /// @brief A 16-line feedback delay network with a Hadamard matrix, one fitted absorptive
    /// filter per line and Jot's tone correction. Audio thread. Spec §6.
    class LateReverb
    {
    public:
        /// @param max_line_frames Sized for the longest room the bounds allow: an RT60 of 3 s
        /// gives a mean line of 262.5 ms and a longest of 525 ms, 25200 frames at 48 kHz.
        void prepare(std::uint32_t max_line_frames);
        void reset() noexcept;

        /// @brief Renders one block. For the microphone listener the caller passes one buffer
        /// twice and reads it once.
        void process(const FdnDesign& design, const float* input,
                     float* left, float* right, std::uint32_t frames) noexcept;
    };
}
```

- [ ] **Step 1: Write the failing tests**, all driven by hand-written designs:

```cpp
TEST(Unit_Dsp, LateReverbHadamardPreservesEnergy)
{
    // 16 lines of length 1, unit line filters, no absorption: the sum of squares over the 16
    // lines stays within 1e-5 of its starting value over 1000 samples, because a normalised
    // Hadamard matrix is orthogonal.
}

TEST(Unit_Dsp, LateReverbDecaysAtTheRateItsFiltersAsk)
{
    // every line filter a plain gain g = 10^(-3 * tau_i / 1.0) with tau_i = line_frames[i]/48000:
    // the Schroeder backward integral of the impulse response crosses -60 dB at 1.00 s +/- 5 %.
}

TEST(Unit_Dsp, LateReverbIsSilentBeforeItsInputDelay)
{
    // delay_to_crossover_frames = 1749, the garage's t_c at 48 kHz: every output frame before
    // 1749 is exactly 0.0f.
}

TEST(Unit_Dsp, LateReverbEarsDifferButShareTheirEnergy)
{
    // with the two orthogonal output vectors, the left and right blocks are not equal and their
    // energies over 48000 frames are within 0.5 dB.
}

TEST(Unit_Dsp, LateReverbResetSilencesTheLines)
{
    // after a full-scale impulse, reset(), and 4096 silent input frames, every output is 0.0f.
}

TEST(Unit_Dsp, LateReverbSurvivesALineLengthChangeWithoutAClick)
{
    // 1024 frames with one design, then 1024 with a design whose line_frames are all shorter: no
    // sample-to-sample step exceeds 0.05 of the block's peak.
}
```

Write all six out in full.

- [ ] **Step 2: Confirm they fail**; paste the error.
- [ ] **Step 3: Implement.** Sixteen `std::vector<float>` lines allocated in `prepare()` at
      `max_line_frames`, each read at its design offset so a shorter room is a shorter read and
      never a reallocation. The feedback matrix is a fast Walsh-Hadamard transform, 64 additions
      per sample, scaled by 1/4 to be orthonormal. Two `Biquad` per line, the two-section
      tone correction on each ear's sum, and the two two-section coherence filters on the right
      ear. A changed line length crossfades between the old and new read heads over 512 frames,
      the same length the node's set crossfade uses.
- [ ] **Step 4: Syntax-check**, paste. **Step 5: Commit**
      `DE2: LateReverb runs the 16-line Hadamard FDN and its two ear outputs`.

**Acceptance:** with every line filter set to the gain a 1.0 s RT60 asks for, the Schroeder
backward integral of the impulse response crosses −60 dB at 1.00 s within 5 %.

---

### Task 15: `ModalBank`

**Waits on:** Task 8.

**Files:**
- Create: `modules/room/include/SushiDSP/room/ModalBank.hpp`, `modules/room/src/ModalBank.cpp`
- Test: `tests/functional/unit/test_modal_bank.cpp`

**Interfaces:**
- Consumes: `ModalDesign`, `SushiDSP::DenormalGuard`.
- Produces:

```cpp
namespace SushiDSP::room
{
    /// @brief Up to kMaxModes second-order resonators, each fed by the source's mode shape and
    /// read at the two ears' mode shapes. Audio thread; 13 flops per mode per sample. Spec §9a.2.
    class ModalBank
    {
    public:
        void prepare(std::uint32_t max_block_frames);
        void reset() noexcept;

        /// @brief Adds the modal field into left and right. A design whose count is zero adds
        /// nothing and touches no state.
        void process(const ModalDesign& design, const float* input,
                     float* left, float* right, std::uint32_t frames) noexcept;
    };
}
```

- [ ] **Step 1: Write the failing tests.**

```cpp
TEST(Unit_Dsp, ModalBankRingsAtEachModesFrequency)
{
    SushiDSP::room::ModalDesign design{};
    design.count = 1u;
    // A resonator at 68.6 Hz, the garage's 2.5 m axial mode, with a 1.0 s 60 dB decay at 48 kHz:
    // r = 10^(-3 / (1.0 * 48000)) = 0.99985610 and theta = 2 * pi * 68.6 / 48000 = 0.00897972,
    // so a1 = -2 * r * cos(theta) = -1.99963157 and a2 = r * r = 0.99971222. Derive the section
    // from the formula in the test rather than pasting the digits: 0.5 % of 68.6 Hz is 0.34 Hz,
    // finer than the seventh digit of a1 moves the pole.
    design.modes[0].section = { 1.0, 0.0, -1.0, -1.99963157, 0.99971222 };
    design.modes[0].source_gain = 1.0f;
    design.modes[0].ear_gain = { 1.0f, 1.0f };
    // A unit impulse in, 65536 frames out, a 65536-point FFT: the peak bin is within 0.5 % of
    // 68.6 Hz.
}

TEST(Unit_Dsp, ModalBankDecaysAtTheDampingItWasGiven)
{
    // the same resonator's envelope falls 60 dB in 1.00 s within 5 %.
}

TEST(Unit_Dsp, ModalBankScalesEachModeByItsSourceAndEarGains)
{
    // source_gain 0.5 and ear_gain { 1.0f, -1.0f }: right is exactly the negative of left, and
    // both are half the amplitude of the unit-gain run.
}

TEST(Unit_Dsp, ModalBankRunsFiveHundredAndTwelveModesWithoutDenormals)
{
    // 512 modes from 20 Hz to 500 Hz, 96000 frames: no output sample is denormal.
}

TEST(Unit_Dsp, ModalBankWithNoModesAddsNothing)
{
    // count = 0: the output buffers keep whatever they held, to the bit.
}

TEST(Unit_Dsp, ModalBankResetSilencesEveryResonator)
{
    // after a full-scale impulse and reset(), 4096 silent frames come out exactly 0.0f.
}
```

Write the last five out in full.

- [ ] **Step 2: Confirm they fail**; paste the error.
- [ ] **Step 3: Implement.** One `double` state pair per mode, allocated in `prepare()` at
      `kMaxModes`; the inner loop is mode-outer, sample-inner; `process` opens with a
      `DenormalGuard`, as every other audio-thread class in the tree does.
- [ ] **Step 4: Syntax-check**, paste. **Step 5: Commit**
      `DE3: ModalBank runs the room's standing waves as second-order resonators`.

**Acceptance:** a single resonator at 68.6 Hz with a 1.0 s decay peaks within 0.5 % of 68.6 Hz,
read off the parabola through the three bins around the maximum because 0.34 Hz is finer than a
0.732 Hz bin, in
a 65536-point FFT and falls 60 dB in 1.00 s within 5 %.

---

### Task 16: `SphericalHeadHrtf`

**Waits on:** Task 10.

**Files:**
- Create: `modules/room/include/SushiDSP/room/SphericalHeadHrtf.hpp`,
  `modules/room/src/SphericalHeadHrtf.cpp`
- Test: `tests/functional/unit/test_spherical_head_hrtf.cpp`

**Interfaces:**
- Consumes: `HrtfSet`, `SushiDSP::Fft`.
- Produces:

```cpp
namespace SushiDSP::room
{
    /// @brief Builds an HrtfSet from the Duda-Martens rigid-sphere model with Woodworth's ITD.
    /// Correct ITD and ILD, no pinna cues: front and back sound alike. Spec §9.2.
    class SphericalHeadHrtf
    {
    public:
        static constexpr double kHeadRadiusMetres = 0.0875;

        /// @brief A set on a 10-degree azimuth by 10-degree elevation grid, 614 directions.
        static HrtfSet build(double sample_rate);

        /// @brief Woodworth's (a / c) * (theta + sin theta), in seconds.
        static double interaural_time_difference(double azimuth_rad) noexcept;
    };
}
```

- [ ] **Step 1: Write the failing tests.**

```cpp
TEST(Unit_Dsp, SphericalHeadHrtfMatchesWoodworthsInterauralDelay)
{
    // (a / c) * (theta + sin theta) with a = 0.0875 m and c = 343 m/s.
    EXPECT_NEAR(SushiDSP::room::SphericalHeadHrtf::interaural_time_difference(M_PI / 6.0),
                0.0002611, 2e-6);
    EXPECT_NEAR(SushiDSP::room::SphericalHeadHrtf::interaural_time_difference(M_PI / 3.0),
                0.0004881, 2e-6);
    EXPECT_NEAR(SushiDSP::room::SphericalHeadHrtf::interaural_time_difference(M_PI / 2.0),
                0.0006558, 2e-6);
}

TEST(Unit_Dsp, SphericalHeadHrtfPutsZeroDelayStraightAhead)
{
    // interaural_time_difference(0.0) is 0, and the built set's front direction has
    // itd_frames == 0.0f.
}

TEST(Unit_Dsp, SphericalHeadHrtfIsSymmetricAboutTheMedianPlane)
{
    // the pair at +60 degrees is the mirror of the pair at -60 degrees, ear for ear, within 1e-6.
}

TEST(Unit_Dsp, SphericalHeadHrtfCannotTellFrontFromBack)
{
    // the pairs at 0 and at 180 degrees are equal within 1e-6 in both ears; this is the model's
    // documented limit and the reason the asset exists.
}

TEST(Unit_Dsp, SphericalHeadHrtfShadowsTheFarEarAboveOneKilohertz)
{
    // at 90 degrees the far ear is at least 4 dB below the near ear in the 4 kHz octave and
    // within 2 dB of it in the 125 Hz octave, and the difference rises with every octave in
    // between. With the ears on the interaural axis the far ear sits at the source's antipode,
    // the sphere's bright spot, so the measured 4.73 dB is what the model gives and the deeper
    // shadow of Duda and Martens belongs to their ears at plus and minus 100 degrees.
}

TEST(Unit_Dsp, SphericalHeadHrtfBuildsAGridThatCoversTheSphere)
{
    // direction_count() is 614, and interpolate() succeeds for 500 pseudo-random directions.
}
```

Write the last five out in full.

- [ ] **Step 2: Confirm they fail**; paste the error.
- [ ] **Step 3: Implement.** Evaluate the rigid-sphere transfer function per direction on a
      512-bin grid to 24 kHz by the Duda-Martens series, take its minimum-phase part by cepstral
      folding, truncate to 128 taps, and store the ITD separately as Woodworth's figure in frames.
- [ ] **Step 4: Syntax-check**, paste. **Step 5: Commit**
      `DE5: SphericalHeadHrtf gives the room an HRTF with no asset present`.

**Acceptance:** the ITD at 30°, 60° and 90° is 0.2611, 0.4881 and 0.6558 ms, each within 2 µs.

---

### Task 17: `HrtfSetLoader`

**Waits on:** Task 10, and Task 2 for the fixture.

**Files:**
- Create: `modules/room/include/SushiDSP/room/HrtfSetLoader.hpp`,
  `modules/room/src/HrtfSetLoader.cpp`
- Create: `tests/fixtures/hrtf_self_test.hrtf` (the 40 040-byte file Task 2's `--self-test` writes)
- Test: `tests/functional/unit/test_hrtf_set_loader.cpp`

**Interfaces:**
- Consumes: `HrtfSet`, `std::ifstream`. No HDF5, no SOFA, no third-party library.
- Produces:

```cpp
namespace SushiDSP::room
{
    /// @brief Reads the flat asset convert_hrtf.py writes. Control thread; the only file this
    /// module opens. Spec §9.2 and §9.3.
    class HrtfSetLoader
    {
    public:
        /// @brief Reads path into set.
        /// @param error_out Filled with the reason on failure: missing, bad magic, wrong version,
        /// a rate mismatch, or a truncated file, each naming the path.
        /// @return False and leaves set untouched on any failure.
        static bool load(const std::string& path, double expected_sample_rate,
                         HrtfSet& set, std::string& error_out);
    };
}
```

- [ ] **Step 1: Write the failing tests** against the six-direction fixture:

```cpp
TEST(Unit_Dsp, HrtfSetLoaderReadsEveryFieldOfTheFixture)
{
    // direction_count() is 6; the ring's azimuths read back as 0.0, pi/2, pi and 3*pi/2 within
    // 1e-6 and the zenith and the nadir carry elevation +pi/2 and -pi/2; decoder_order() is 3
    // and decoder_filters().size() is 32 * 256.
}

TEST(Unit_Dsp, HrtfSetLoaderRefusesAMissingFileAndNamesThePath)
{
    // load("assets/hrtf/nothing.hrtf", 48000.0, set, error) is false and error contains the path.
}

TEST(Unit_Dsp, HrtfSetLoaderRefusesTheWrongMagic)
{
    // a copy of the fixture with byte 0 changed is refused, and set is untouched.
}

TEST(Unit_Dsp, HrtfSetLoaderRefusesARateMismatch)
{
    // the 48000 Hz fixture asked for at 96000 is refused, and error names both rates.
}

TEST(Unit_Dsp, HrtfSetLoaderRefusesATruncatedFile)
{
    // the fixture cut to half its length is refused, and set is untouched.
}
```

Write all five out in full.

- [ ] **Step 2: Confirm they fail**; paste the error.
- [ ] **Step 3: Implement.** Read the header, check the magic and the version, size the vectors
      once from the counts, read the payload, and check the stream reached exactly the file's end.
      Every failure path leaves `set` untouched: build a local `HrtfSet` and move it out only on
      success, stated as an invariant comment.
- [ ] **Step 4: Syntax-check**, paste. **Step 5: Commit**
      `DE5: HrtfSetLoader reads the flat asset with the standard library alone`.

**Acceptance:** the six-direction fixture round-trips every field, and each of the four failure
modes returns false with the path named and `set` unchanged.

---

### Task 18: `BinauralDecoder`

**Waits on:** Task 10.

**Files:**
- Create: `modules/room/include/SushiDSP/room/BinauralDecoder.hpp`,
  `modules/room/src/BinauralDecoder.cpp`
- Test: `tests/functional/unit/test_binaural_decoder.cpp`

**Interfaces:**
- Consumes: `HrtfSet`, `AmbisonicEncoder::kChannels`, `SushiDSP::PartitionedConvolution`,
  `SushiDSP::StreamingConvolution::partition_size_for`.
- Produces:

```cpp
namespace SushiDSP::room
{
    /// @brief 32 partitioned convolutions turning 16 Ambisonic channels into two ears. Its cost
    /// is fixed by the order and does not depend on the scene. Audio thread. Spec §9.4.
    class BinauralDecoder
    {
    public:
        /// @brief Builds the 32 convolvers from the set's decoder filters. Control thread.
        /// @throw std::runtime_error when the set carries no decoder of the expected order.
        void prepare(const HrtfSet& set, double sample_rate, std::uint32_t max_block_frames);

        bool          ready() const noexcept;
        std::uint32_t latency_frames() const noexcept;   ///< One partition.

        /// @brief Decodes one block; inputs is 16 channel pointers.
        void process(const float* const* inputs, float* left, float* right,
                     std::uint32_t frames) noexcept;

        void reset() noexcept;
    };
}
```

- [ ] **Step 1: Write the failing tests**, on a synthetic set whose decoder filters are deltas:

```cpp
TEST(Unit_Dsp, BinauralDecoderWithDeltaFiltersPassesChannelZeroThrough)
{
    // decoder filter (channel 0, left ear) is a delta at tap 0 and everything else is zero: an
    // impulse on Ambisonic channel 0 leaves the left ear at frame latency_frames() as exactly
    // 1.0f.
}

TEST(Unit_Dsp, BinauralDecoderReportsOnePartitionOfLatency)
{
    EXPECT_EQ(decoder.latency_frames(),
              SushiDSP::StreamingConvolution::partition_size_for(48000.0));   // 128 at 48 kHz
}

TEST(Unit_Dsp, BinauralDecoderSumsSixteenChannelsIntoEachEar)
{
    // deltas on all sixteen channels into the left ear: each channel alone gives 1.0f and all
    // sixteen at once give exactly 16.0f.
}

TEST(Unit_Dsp, BinauralDecoderRefusesASetWithNoDecoder)
{
    // EXPECT_THROW(decoder.prepare(SushiDSP::room::HrtfSet{}, 48000.0, 256u), std::runtime_error)
    // and ready() stays false.
}

TEST(Unit_Dsp, BinauralDecoderResetClearsTheHistory)
{
    // after an impulse and reset(), 2048 silent frames come out exactly 0.0f.
}
```

Write the other four out in full.

- [ ] **Step 2: Confirm they fail**; paste the error.
- [ ] **Step 3: Implement.** 32 `PartitionedConvolution` instances built in `prepare()` at
      `partition_size_for(sample_rate)`; `process` runs each and sums into the ear buffers. Each
      instance transforms its own input, so the sixteen channels are each transformed twice: that
      is the unshared figure spec §10 budgets, and sharing the spectra later changes no interface.
      Say so in an invariant comment.
- [ ] **Step 4: Syntax-check**, paste. **Step 5: Commit**
      `DE5: BinauralDecoder turns 16 Ambisonic channels into two ears`.

**Acceptance:** with a delta on channel 0's left decoder filter, an impulse on channel 0 emerges
from the left ear at frame 128 as exactly 1.0f.

---

## Wave 4 (two tasks, parallel, disjoint files)

### Task 19: `EyringDecay`

**Waits on:** Tasks 3, 4, 12.

**Files:**
- Create: `modules/room/include/SushiDSP/room/EyringDecay.hpp`,
  `modules/room/src/EyringDecay.cpp`
- Test: `tests/functional/unit/test_eyring_decay.cpp`

**Interfaces:**
- Consumes: `RoomScene::mean_absorption`, `RoomScene::volume_m3`, `RoomScene::surface_area_m2`,
  `AirAbsorption::energy_coefficient_per_metre`.
- Produces:

```cpp
namespace SushiDSP::room
{
    /// @brief The room's decay time per band, and the Schroeder frequency that follows from it.
    /// Control thread. Eyring rather than Sabine: spec §6.3.
    class EyringDecay
    {
    public:
        /// @brief 0.161 * V / (-S * ln(1 - mean_alpha(f)) + 4 * m(f) * V), in seconds per band.
        static std::array<double, kBandCount> rt60_seconds(const RoomScene& scene);

        /// @brief 2000 * sqrt(RT60(500 Hz) / V), the frequency below which the room is modal.
        static double schroeder_frequency_hz(const RoomScene& scene);
    };
}
```

- [ ] **Step 1: Write the failing tests** against this plan's reference table:

```cpp
TEST(Unit_Dsp, EyringDecayReproducesTheGaragesSixBandDecay)
{
    const std::array<double, 6> expected = { 1.342, 1.333, 1.161, 1.146, 1.108, 0.893 };
    const std::array<double, 6> computed =
        SushiDSP::room::EyringDecay::rt60_seconds(SushiDSP::test::garage_scene());
    for (std::size_t band = 0; band < 6u; ++band)
    {
        EXPECT_NEAR(computed[band], expected[band], 0.002) << "band " << band;
    }
}

TEST(Unit_Dsp, EyringDecayReproducesTheBedroomsSixBandDecay)
{
    // { 0.326, 0.871, 1.154, 0.731, 0.428, 0.356 } within 0.002 s.
}

TEST(Unit_Dsp, EyringDecayReproducesTheBlockRoomsSixBandDecay)
{
    // { 0.929, 1.886, 1.548, 1.305, 0.983, 0.981 } within 0.002 s.
}

TEST(Unit_Dsp, EyringDecayComputesTheSchroederFrequency)
{
    // garage 278.2 Hz, bedroom 356.1 Hz, block room 338.6 Hz, each within 0.5 Hz.
}

TEST(Unit_Dsp, EyringDecayDivergesFromSabineOnlyInAnAbsorbentRoom)
{
    // the ratio -ln(1 - a) / a is 1.010 at a = 0.02, 1.042 at 0.0796 and 1.386 at 0.5; the test
    // computes both forms and asserts those three ratios within 0.002, which is why a concrete
    // garage comes out the same either way and a carpeted room does not.
}

TEST(Unit_Dsp, EyringDecayHandlesAFullyAbsorbentRoom)
{
    // every surface OpenDoorway: mean alpha is 1, -ln(0) is infinite, and the function returns
    // 0 s in every band instead of a NaN.
}
```

Write the last five out in full.

- [ ] **Step 2: Confirm they fail**; paste the error.
- [ ] **Step 3: Implement** that formula in `double`, clamping mean absorption to 1 − 1e-9.
- [ ] **Step 4: Syntax-check**, paste. **Step 5: Commit**
      `DE2: EyringDecay gives the room its decay time per band and its Schroeder frequency`.

**Acceptance:** the garage's six RT60 figures are 1.342, 1.333, 1.161, 1.146, 1.108 and 0.893 s
within 2 ms, and its Schroeder frequency is 278.2 Hz within 0.5 Hz.

---

### Task 20: `ImageSourceSolver`

**Waits on:** Tasks 3, 4, 5, 6, 7, 8, 11, 12.

**Files:**
- Create: `modules/room/include/SushiDSP/room/ImageSourceSolver.hpp`,
  `modules/room/src/ImageSourceSolver.cpp`
- Test: `tests/functional/unit/test_image_source_solver.cpp`

**Interfaces:**
- Consumes: `RoomScene`, `Materials`, `AirAbsorption`, `BandResponseFit`, `CabinetDirectivity`,
  `MicrophoneModel`, `AmbisonicEncoder`, `EarlyTapSet`.
- Produces:

```cpp
namespace SushiDSP::room
{
    enum class ListenerKind : std::uint8_t { Ears = 0, Microphone };

    /// @brief Mirrors the source across the six walls to order 4 and reduces every surviving
    /// image to one tap. Control thread; under ten milliseconds for a garage solve. Spec §4.1.
    class ImageSourceSolver
    {
    public:
        struct Options
        {
            std::uint32_t max_order   = 4;
            ListenerKind  listener    = ListenerKind::Ears;
            double        sample_rate = 48000.0;
        };

        /// @brief (2/3) * N * (N + 1) * (2N + 1) + 2N + 1, the direct sound included.
        static std::uint32_t image_count(std::uint32_t order) noexcept;

        /// @brief (order + 1) * min(L_x, L_y, L_z) / c, after which the image set is incomplete.
        static double crossover_time_s(const RoomScene& scene, std::uint32_t order) noexcept;

        /// @brief The whole tap set, sorted by arrival, with nothing past crossover_time_s.
        static EarlyTapSet solve(const RoomScene& scene, const Options& options);
    };
}
```

- [ ] **Step 1: Write the failing tests.** The seven arrival times below are computed by hand for
      the spec's §4.3 example — 6 × 4 × 2.5 m, source at (1.5, 2.0, 0.5), listener at (4.0, 2.0,
      1.2) — as |p_i − r| / c × 48000 with c = 343 m/s:

```cpp
TEST(Unit_Dsp, ImageSourceSolverCountsTheLatticePoints)
{
    EXPECT_EQ(SushiDSP::room::ImageSourceSolver::image_count(1u), 7u);
    EXPECT_EQ(SushiDSP::room::ImageSourceSolver::image_count(2u), 25u);
    EXPECT_EQ(SushiDSP::room::ImageSourceSolver::image_count(3u), 63u);
    EXPECT_EQ(SushiDSP::room::ImageSourceSolver::image_count(4u), 129u);
}

TEST(Unit_Dsp, ImageSourceSolverPlacesTheFirstOrderArrivalsExactly)
{
    SushiDSP::room::RoomScene scene = SushiDSP::test::garage_scene();
    scene.source_position   = { 1.5, 2.0, 0.5 };
    scene.listener_position = { 4.0, 2.0, 1.2 };
    SushiDSP::room::ImageSourceSolver::Options options;
    // Order 1 alone cuts at t_c = 2 * 2.5 / 343 = 699.708 frames, which drops the -x and +x
    // images, so the seven arrivals are read from an order 4 solve.
    options.max_order = 4u;
    const SushiDSP::room::EarlyTapSet set =
        SushiDSP::room::ImageSourceSolver::solve(scene, options);
    ASSERT_GE(set.count, 7u);
    // direct 2.596151 m, floor 3.023243 m, ceiling 4.140048 m, -y and +y 4.768648 m,
    // -x 5.544367 m, +x 6.537584 m; divided by 343 and multiplied by 48000:
    const std::array<double, 7> frames =
        { 363.310, 423.078, 579.365, 667.333, 667.333, 775.888, 914.881 };
    for (std::size_t tap = 0; tap < 7u; ++tap)
    {
        EXPECT_NEAR(set.taps[tap].delay_frames, frames[tap], 1e-3) << "tap " << tap;
    }
}

TEST(Unit_Dsp, ImageSourceSolverSortsItsTapsByArrival)
{
    // delay_frames is non-decreasing across the whole set at order 4, for all three reference
    // rooms.
}

TEST(Unit_Dsp, ImageSourceSolverDropsEveryTapPastTheHandoverTime)
{
    // crossover_time_s(garage, 4) is 5 * 2.5 / 343 = 0.0364431 s, so no tap exceeds 1749.3 frames
    // at 48 kHz and set.crossover_time_s carries that figure within 1e-6.
}

TEST(Unit_Dsp, ImageSourceSolverFallsOffAsOneOverDistance)
{
    // with every surface OpenDoorway no reflection survives, so the single direct tap's gain is
    // 1 / 2.596151 = 0.385186 within 1e-5.
}

TEST(Unit_Dsp, ImageSourceSolverEncodesEarsAndMicrophoneDifferently)
{
    // ListenerKind::Ears fills all sixteen encode gains and leaves channel 0 at exactly 1 for
    // every tap; ListenerKind::Microphone leaves channels 1 to 15 at zero.
}

TEST(Unit_Dsp, ImageSourceSolverHandsTheScatteredEnergyToTheTail)
{
    // Flat is s = 0.10, not zero, so the ratio is against that: with every surface Cluttered
    // (s = 0.40) a first-order tap's gain is sqrt(0.60 / 0.90) = 0.816497 of the same tap over
    // Flat surfaces, and set.scattered_energy is above zero.
}

TEST(Unit_Dsp, ImageSourceSolverSolvesTheGarageUnderTenMilliseconds)
{
    // order 4 with its fits, for the garage, timed with std::chrono::steady_clock: under 10 ms,
    // the bound spec §4.2 derives from the thirty solves a second §5.2 needs while a slider
    // drags. The fits dominate; the lattice arithmetic is under a percent of the total.
}
```

Write the last six out in full.

- [ ] **Step 2: Confirm they fail**; paste the error.
- [ ] **Step 3: Implement.** Enumerate the images on the mirror lattice: for reflection counts
      (n_x, n_y, n_z) with |n_x| + |n_y| + |n_z| ≤ order, the image coordinate on each axis is
      `n * L + (n even ? s : L - s)`, and the walls crossed are |n| bounces off each of that axis's
      two faces. Multiply the wall reflection factors, the directivity for the outgoing direction,
      the air absorption for the path length and — for the microphone listener — the pattern; hand
      the six numbers to one `BandResponseFit`. Sort by delay, drop anything past t_c, and refuse
      to write past `kMaxEarlyTaps`.
- [ ] **Step 4: Syntax-check**, paste. **Step 5: Commit**
      `DE0: ImageSourceSolver turns a scene into 128 early taps with one filter each`.

**Acceptance:** for the spec's example geometry the seven first-order taps land at 363.310,
423.078, 579.365, 667.333, 667.333, 775.888 and 914.881 frames at 48 kHz, each within 0.001 frames.

---

## Wave 5 (two tasks, parallel, disjoint files)

### Task 21: `LateReverbDesign`

**Waits on:** Tasks 5, 8, 19, 20.

**Files:**
- Create: `modules/room/include/SushiDSP/room/LateReverbDesign.hpp`,
  `modules/room/src/LateReverbDesign.cpp`
- Test: `tests/functional/unit/test_late_reverb_design.cpp`

**Interfaces:**
- Consumes: `EyringDecay::rt60_seconds`, `BandResponseFit::fit`, `EarlyTapSet`, `FdnDesign`,
  `ListenerKind`.
- Produces:

```cpp
namespace SushiDSP::room
{
    /// @brief Turns a room into 16 line lengths, 16 absorptive filters, an input gain that
    /// continues the early set's energy at t_c, and the two coherence filters. Control thread.
    /// Spec §6.2, §6.3 and §6.4.
    class LateReverbDesign
    {
    public:
        static FdnDesign design(const RoomScene& scene, const EarlyTapSet& early,
                                double sample_rate, ListenerKind listener);

        /// @brief Sixteen distinct primes spread logarithmically over
        /// [mean / sqrt(2), mean * sqrt(2)].
        static std::array<std::uint32_t, FdnDesign::kLineCount>
            line_frames(double mean_seconds, double sample_rate);
    };
}
```

- [ ] **Step 1: Write the failing tests.**

```cpp
TEST(Unit_Dsp, LateReverbDesignMeetsTheModalOverlapRule)
{
    // Garage: RT60(500 Hz) = 1.161 s, so the sixteen lines must total at least
    // 1.4 * 1.161 = 1.6254 s, which is 78019 frames at 48 kHz.
    const SushiDSP::room::FdnDesign design = SushiDSP::room::LateReverbDesign::design(
        SushiDSP::test::garage_scene(), SushiDSP::test::garage_early_taps(), 48000.0,
        SushiDSP::room::ListenerKind::Ears);
    std::uint64_t total = 0u;
    for (const std::uint32_t frames : design.line_frames) { total += frames; }
    EXPECT_GE(total, 78019u);
    EXPECT_LT(total, 90000u);
}

TEST(Unit_Dsp, LateReverbDesignUsesSixteenDistinctPrimes)
{
    // The garage's mean line is 1.4 * 1.161 / 16 = 0.101588 s, 4876 frames, so the sixteen land
    // on 3449, 3613, 3793, 3967, 4153, 4349, 4549, 4783, 4993, 5227, 5477, 5737, 6007, 6287,
    // 6599 and 6899, totalling 79882. Every entry is prime, none repeats, and max/min is 2.00.
}

TEST(Unit_Dsp, LateReverbDesignFitsTheDecayEachBandAsks)
{
    // line 0's fitted filter at each of the six centres is within 1 dB of
    // 10^(-3 * tau_0 / RT60(f)) with tau_0 = line_frames[0] / 48000.
}

TEST(Unit_Dsp, LateReverbDesignDelaysTheTailToTheHandoverTime)
{
    // delay_to_crossover_frames is round(early.crossover_time_s * 48000) exactly: 1749.
}

TEST(Unit_Dsp, LateReverbDesignScalesTheInputToContinueTheEarlyEnergy)
{
    // halving every early tap's gain halves input_gain within 1e-6: the match is linear in
    // pressure.
}

TEST(Unit_Dsp, LateReverbDesignMakesTheTailCoherentLowAndIncoherentHigh)
{
    // sin(k d)/(k d) at d = 0.17 m is 0.975 at 125 Hz, 0.902 at 250 Hz and -0.0088 at 4 kHz,
    // so the coherence
    // filter's magnitude is above 0.85 at 125 Hz and below 0.20 at 4 kHz.
}

TEST(Unit_Dsp, LateReverbDesignGivesTheMicrophoneOneOutput)
{
    // ListenerKind::Microphone leaves both coherence filters as pass-throughs.
}
```

`garage_early_taps()` is a helper in `tests/common/RoomFixtures.hpp` that calls
`ImageSourceSolver::solve(garage_scene(), {})`; add it in this task.

Write the last six out in full.

- [ ] **Step 2: Confirm they fail**; paste the error.
- [ ] **Step 3: Implement.** Mean line = `1.4 * RT60(500 Hz) / 16`. Spread sixteen targets
      logarithmically over `[mean / sqrt(2), mean * sqrt(2)]`, round each to frames, and step up to
      the next prime. Each line's band gains are `10^(-3 * tau_i / RT60(f))` through
      `BandResponseFit`. The input gain sums the energy of the taps in the last 5 ms before t_c,
      scales it by the statistical decay over that interval, and adds the set's
      `scattered_energy`.
- [ ] **Step 4: Syntax-check**, paste. **Step 5: Commit**
      `DE2: LateReverbDesign sets the FDN's lengths, filters and input from the room`.

**Acceptance:** the garage's sixteen prime line lengths total 79 882 frames at 48 kHz, above the
78 019 the rule Σ τ_i ≥ 1.4·RT60 demands, and no two lengths are equal.

---

### Task 22: `ModalSolver`

**Waits on:** Tasks 3, 8, 12, 19.

**Files:**
- Create: `modules/room/include/SushiDSP/room/ModalSolver.hpp`,
  `modules/room/src/ModalSolver.cpp`
- Test: `tests/functional/unit/test_modal_solver.cpp`

**Interfaces:**
- Consumes: `RoomScene`, `Materials`, `EyringDecay::schroeder_frequency_hz`, `ModalDesign`.
- Produces:

```cpp
namespace SushiDSP::room
{
    /// @brief The shoebox's eigenmodes below the Schroeder frequency, their perturbation damping,
    /// and their mode-shape gains at the cabinet and the two ears. Control thread. Spec §9a.2.
    class ModalSolver
    {
    public:
        static constexpr double kMinCrossoverHz = 40.0;
        static constexpr double kEarHalfSpacingMetres = 0.085;

        struct Mode
        {
            std::uint32_t nx; std::uint32_t ny; std::uint32_t nz;
            double frequency_hz;
            double decay_seconds;   ///< 60 dB decay from the first-order perturbation damping.
        };

        /// @brief Every mode under max_frequency_hz, sorted by frequency.
        static std::vector<Mode> enumerate(const RoomScene& scene, double max_frequency_hz);

        /// @brief Weyl's count, (4pi/3) V (f/c)^3 + (pi/4) S (f/c)^2 + (L/8)(f/c) with
        /// L = 4 (L_x + L_y + L_z).
        static double estimated_count(const RoomScene& scene, double frequency_hz);

        /// @brief The whole bank. A crossover under kMinCrossoverHz returns count = 0, which is
        /// how the layer switches itself off in a large or dead room. Spec §9a.4.
        static ModalDesign design(const RoomScene& scene, double sample_rate, double crossover_hz);
    };
}
```

- [ ] **Step 1: Write the failing tests.**

```cpp
TEST(Unit_Dsp, ModalSolverFindsTheGaragesEigenfrequencies)
{
    const std::vector<SushiDSP::room::ModalSolver::Mode> modes =
        SushiDSP::room::ModalSolver::enumerate(SushiDSP::test::garage_scene(), 100.0);
    // f = (c/2) * sqrt((nx/6)^2 + (ny/4)^2 + (nz/2.5)^2) with c = 343:
    const std::array<double, 8> expected =
        { 28.58, 42.88, 51.53, 57.17, 68.60, 71.46, 74.32, 80.90 };
    ASSERT_GE(modes.size(), 8u);
    for (std::size_t index = 0; index < 8u; ++index)
    {
        EXPECT_NEAR(modes[index].frequency_hz, expected[index], 0.01) << "mode " << index;
    }
}

TEST(Unit_Dsp, ModalSolverCountsOneHundredAndEightySevenModesBelowTheGaragesSchroederFrequency)
{
    EXPECT_EQ(SushiDSP::room::ModalSolver::enumerate(SushiDSP::test::garage_scene(), 278.2).size(),
              187u);
    EXPECT_NEAR(SushiDSP::room::ModalSolver::estimated_count(SushiDSP::test::garage_scene(), 278.2),
                190.0, 1.0);   // Weyl and the exact count agree within 2 %
}

TEST(Unit_Dsp, ModalSolverStaysUnderTheModeCap)
{
    // for all three reference rooms, the count at that room's own f_s is under
    // ModalDesign::kMaxModes.
}

TEST(Unit_Dsp, ModalSolverLowersTheCrossoverWhenTheCountWouldOverflow)
{
    // a 3.0 x 2.5 x 2.4 m room of ConcreteUnpainted asked for a 900 Hz crossover: design()
    // returns count == kMaxModes and a crossover_hz under 900, and enumerate() at that returned
    // figure gives exactly kMaxModes modes.
}

TEST(Unit_Dsp, ModalSolverSwitchesOffUnderFortyHertz)
{
    // design(scene, 48000.0, 39.0) returns count == 0 and crossover_hz == 0.0f.
}

TEST(Unit_Dsp, ModalSolverPutsThePressureInTheCorners)
{
    // for the (1, 0, 0) mode, psi at x = 0.05 m is cos(pi * 0.05 / 6) = 0.999657 and at the
    // centre x = 3.0 m is cos(pi / 2) = 0, within 1e-9. That cosine is what the node's 12 dB
    // corner-to-centre acceptance measures.
}

TEST(Unit_Dsp, ModalSolverDampsAnAxialModeByTheTwoWallsItBouncesBetween)
{
    // a garage whose +x and -x faces are GypsumBoardOnStuds (alpha 0.29 at 125 Hz) and whose
    // other four are ConcreteUnpainted: the (1, 0, 0) mode's decay_seconds is at least 1.5 times
    // shorter than the (0, 0, 1) mode's, because the soft pair bounds the first and grazes the
    // second. A grazing wall still absorbs: the first-order perturbation of spec 7.2 weights it
    // by the mean of psi squared, which is a half rather than zero, so the measured ratio is
    // 1.846 and not the factor of four a fully reflective grazing wall would give.
}

TEST(Unit_Dsp, ModalSolverGivesTheTwoEarsDifferentGains)
{
    // two ears 0.17 m apart across the (1, 0, 0) mode's gradient differ by more than 1 % in
    // ear_gain anywhere but the pressure maximum.
}
```

Write the last six out in full.

- [ ] **Step 2: Confirm they fail**; paste the error.
- [ ] **Step 3: Implement.** Enumerate (n_x, n_y, n_z) up to the index each axis allows below the
      cap, sort by frequency, and stop at `kMaxModes`; when the cap binds, lower the crossover to
      the last mode's frequency and report it in `ModalDesign::crossover_hz`. Damping is the
      first-order perturbation: a mode's decay constant is the sum over the six faces of that
      face's absorption weighted by the mode's mean-square pressure on it, with the 125 Hz and
      250 Hz material rows interpolated in log frequency to the mode. The mode shape is
      `cos(nx * pi * x / Lx) * cos(ny * pi * y / Ly) * cos(nz * pi * z / Lz)`, evaluated at the
      cabinet and at each ear, the ears sitting `kEarHalfSpacingMetres` either side of the listener
      along its yaw. `level_scale` matches the layer's steady-state energy in the octave below f_s
      to what Eyring predicts for the same octave.
- [ ] **Step 4: Syntax-check**, paste. **Step 5: Commit**
      `DE3: ModalSolver gives the room its standing waves, their damping and their gains`.

**Acceptance:** the garage's first eight eigenfrequencies are 28.58, 42.88, 51.53, 57.17, 68.60,
71.46, 74.32 and 80.90 Hz within 0.01 Hz, and exactly 187 modes lie below its 278.2 Hz Schroeder
frequency.

---

## Wave 6 (one task, serial)

### Task 23: `RoomRenderSet` and `IRoomHost`

**Waits on:** Tasks 16, 17, 20, 21, 22.

**Files:**
- Create: `modules/room/include/SushiDSP/room/RoomRenderSet.hpp`,
  `modules/room/src/RoomRenderSet.cpp`, `modules/room/include/SushiDSP/room/IRoomHost.hpp`
- Test: `tests/functional/unit/test_room_render_set.cpp`

**Interfaces:**
- Consumes: every solver of waves 4 and 5, plus `HrtfSet`.
- Produces:

```cpp
namespace SushiDSP::room
{
    /// @brief Everything the audio thread reads for one scene. Built whole on the control thread
    /// and published through RtSwapSlot; coefficients only, never state. Spec §5.2.
    struct RoomRenderSet
    {
        EarlyTapSet early;
        FdnDesign   late;
        ModalDesign modal;

        std::array<float, HrtfSet::kTapCount> direct_left;
        std::array<float, HrtfSet::kTapCount> direct_right;
        float direct_itd_frames;
        float direct_delay_frames;    ///< round(d / c * fs), the propagation delay.
        float direct_gain;            ///< 1/r for the direct path.

        std::array<float, HrtfSet::kDiffuseTaps> diffuse_left;
        std::array<float, HrtfSet::kDiffuseTaps> diffuse_right;

        float        crossover_hz;
        ListenerKind listener;
    };

    /// @brief Builds one set. Control thread; under 2 ms for a 129-image scene.
    /// @throw std::runtime_error when the scene is invalid, naming the violation.
    std::unique_ptr<RoomRenderSet> build_render_set(const RoomScene& scene, const HrtfSet& hrtf,
                                                    double sample_rate, ListenerKind listener);

    /// @brief Where the room's HRTF came from, so the host can say so without reading a log.
    enum class HrtfSource : std::uint8_t { SphericalHead = 0, Measured };

    /// @brief What a host calls to install a scene and an HRTF. A host discovers it with
    /// dynamic_cast<IRoomHost*> on an IAudioNode, as it does with ICabinetHost.
    struct IRoomHost
    {
        virtual ~IRoomHost() = default;

        /// @brief Solves the scene and publishes a new render set.
        /// @return False and fills reason_out when the scene is refused; nothing is published
        /// and the previous scene stays in place.
        virtual bool set_scene(const RoomScene& scene, std::string& reason_out) = 0;
        virtual RoomScene scene() const = 0;

        /// @brief Installs a measured set and republishes. An empty set restores the fallback.
        virtual bool set_hrtf(HrtfSet set) = 0;
        virtual HrtfSource hrtf_source() const = 0;

        /// @brief The computed Schroeder frequency, for the panel to show beside the room.
        virtual double schroeder_frequency_hz() const = 0;

        /// @brief Frees every render set the audio thread has provably moved past.
        /// @pre Control thread, never concurrently with process(); once per GUI frame.
        virtual void reclaim_rooms() = 0;
    };
}
```

- [ ] **Step 1: Write the failing tests.**

```cpp
TEST(Unit_Dsp, BuildRenderSetFillsEveryLayerForTheGarage)
{
    // early.count == 128, late.line_frames[0] > 0, modal.count == 187, and crossover_hz is
    // 278.2 within 0.5.
}

TEST(Unit_Dsp, BuildRenderSetPutsTheDirectPathAtItsPropagationDelay)
{
    // source (1.5, 2.0, 0.5) and listener (4.0, 2.0, 1.2) are 2.596151 m apart, so
    // direct_delay_frames is round(2.596151 / 343 * 48000) = 363 and direct_gain is
    // 1 / 2.596151 = 0.385186 within 1e-5.
}

TEST(Unit_Dsp, BuildRenderSetRefusesAnInvalidSceneAndSaysWhy)
{
    // a 15 m dimension throws std::runtime_error whose what() names the axis and the bound.
}

TEST(Unit_Dsp, BuildRenderSetTakesTheDirectPairFromTheHrtfSet)
{
    // on the spherical-head fallback, a source 90 degrees to the left gives
    // direct_itd_frames = 0.0006558 * 48000 = 31.5 within 0.1.
}

TEST(Unit_Dsp, BuildRenderSetCostsUnderTwoMilliseconds)
{
    // the garage at order 4 with all three layers, timed with std::chrono::steady_clock.
}

TEST(Unit_Dsp, RoomRenderSetIsTriviallyCopyable)
{
    static_assert(std::is_trivially_copyable<SushiDSP::room::RoomRenderSet>::value,
                  "it crosses to the audio thread");
    SUCCEED();
}
```

Write the first five out in full.

- [ ] **Step 2: Confirm they fail**; paste the error.
- [ ] **Step 3: Implement.** `build_render_set` validates, then calls `ImageSourceSolver::solve`,
      `LateReverbDesign::design`, `EyringDecay::schroeder_frequency_hz` and `ModalSolver::design`
      in that order, and takes the direct pair from `HrtfSet::interpolate`. `IRoomHost.hpp`
      declares only the interface; `RoomNode` implements it in Task 24.
- [ ] **Step 4: Syntax-check**, paste. **Step 5: Commit**
      `DE1: RoomRenderSet is what one scene hands the audio thread, and IRoomHost installs it`.

**Acceptance:** the garage's render set carries 128 early taps, 187 modes, a 278.2 Hz crossover
and a direct path at 363 frames with a gain of 0.385186.

---

## Wave 7 (one task, serial)

### Task 24: `RoomNode`, the early set and the tail

**Waits on:** Tasks 13, 14, 23.

**Files:**
- Create: `modules/room/include/SushiDSP/room/RoomNode.hpp`, `modules/room/src/RoomNode.cpp`,
  `modules/room/README.md`
- Test: `tests/functional/unit/test_room_node.cpp`
- Modify: `tests/functional/unit/test_rt_safety.cpp` (append the room's traps at the end of the
  file). **Tasks 24, 25 and 27 are the only tasks in this plan that touch that file, and they run
  in three different waves, so no two agents ever hold it.**
- Modify: `modules/node_registry/src/NodeRegistry.cpp` (one entry under `NodeCategory{ "Room" }`)

**Interfaces:**
- Consumes: `EarlyReflectionRenderer`, `LateReverb`, `RoomRenderSet`, `build_render_set`,
  `IRoomHost`, `SphericalHeadHrtf`, `SushiDSP::RtSwapSlot`, `SushiDSP::IAudioNode`,
  `SushiDSP::NodeDescriptor`, `SushiDSP::SmoothedParameter`.
- Produces:

```cpp
namespace SushiDSP::room
{
    /// @brief One cabinet in one room, heard from one spot. PortLayout{1, 2}. Spec §1.
    class RoomNode final : public IAudioNode, public IRoomHost
    {
    public:
        static constexpr std::uint32_t kCrossfadeFrames = 512;

        RoomNode();

        void          prepare(const PrepareInfo& info) override;
        void          process(ProcessContext& context) override;
        PortLayout    ports() const override;
        float         get_parameter(const char* id) const override;
        void          set_parameter(const char* id, float value) override;
        void          set_parameter(std::uint16_t index, float value) override;
        void          reset() override;
        std::uint32_t latency_samples() const override;
        const char*   type_id() const override;

        bool       set_scene(const RoomScene& scene, std::string& reason_out) override;
        RoomScene  scene() const override;
        bool       set_hrtf(HrtfSet set) override;
        HrtfSource hrtf_source() const override;
        double     schroeder_frequency_hz() const override;
        void       reclaim_rooms() override;
    };

    const NodeDescriptor& room_descriptor();
}
```

The parameter table, in this order, appended to and never reordered:

| index | id | label | min | max | default | kind | unit |
| --- | --- | --- | --- | --- | --- | --- | --- |
| 0 | `mix` | Mix | 0 | 1 | 1 | Linear | — |
| 1 | `level` | Level | −24 | 24 | 0 | Decibel | dB |
| 2 | `listener` | Listener | 0 | 1 | 0 | Choice | — |
| 3 | `direct_delay` | Direct delay | 0 | 1 | **0** | Choice | — |

`listener` 0 is the ears and 1 the microphone. `direct_delay` **0 is `compensated` and 1 is
`physical`**, and 0 is the default because the maintainer plays live through this node: `physical`
stacks the room's 5.8 ms at 2 m on top of the device's own buffer, and a player in a real room
also hears his strings acoustically at zero delay, which headphones take away (spec §9.4).
`listener` is honoured in Task 26 and `direct_delay` in Task 27; both are declared here so the
table never has to be reordered.

- [ ] **Step 1: Write the failing tests** in `test_room_node.cpp`:

```cpp
TEST(Unit_Dsp, RoomNodeDeclaresOneInputAndTwoOutputs)
{
    SushiDSP::room::RoomNode node;
    EXPECT_EQ(node.ports().input_channels, 1u);
    EXPECT_EQ(node.ports().output_channels, 2u);
}

TEST(Unit_Dsp, RoomNodePlacesTheFirstOrderPeaksWhereTheSolverSaid)
{
    // microphone listener, omni pattern, the tail muted by an input_gain of zero, a unit impulse
    // in: the six first-order peaks land within half a sample of 423.078, 579.365, 667.333,
    // 667.333, 775.888 and 914.881 frames, each measured as the local maximum nearest its figure.
}

TEST(Unit_Dsp, RoomNodeCrossfadesAPublishWithoutAStep)
{
    // a 1 kHz full-scale tone running, a second scene published between two blocks: the output
    // minus a reference raised-cosine crossfade of the two steady renders stays below -60 dBFS.
}

TEST(Unit_Dsp, RoomNodeKeepsRenderingTheOldSetWhileASetIsBuilt)
{
    // no block is silent across 100 publishes: every block's RMS stays above -80 dBFS.
}

TEST(Unit_Dsp, RoomNodeReclaimsARetiredSetOnlyAfterTheAudioThreadMovedOn)
{
    // publish, publish again, reclaim_rooms() with no process() in between: the first set is
    // still alive; one process() then reclaim_rooms() frees it. The idiom is
    // tests/functional/unit/test_rt_swap_slot.cpp's.
}

TEST(Unit_Dsp, RoomNodeRefusesAnInvalidSceneAndKeepsTheOldOne)
{
    // set_scene with a 15 m dimension returns false, fills reason_out, and scene() is unchanged.
}

TEST(Unit_Dsp, RoomNodeMixesDryAndWet)
{
    // mix = 0 puts the input on both channels bit for bit; mix = 1 leaves no dry path at all.
}

TEST(Unit_Dsp, RoomNodeIsRegisteredUnderRoom)
{
    // node_registry() holds "sushidsp.room" with category name "Room", and
    // make_node("sushidsp.room", info) builds it.
}
```

Write the last seven out in full.

- [ ] **Step 2: Confirm they fail**; paste the error.
- [ ] **Step 3: Implement.** `prepare` sizes the input ring for the longest t_c the bounds allow
      (8397 frames at 48 kHz), sizes the FDN lines for a 3 s RT60 (25 200 frames), builds the
      `SphericalHeadHrtf` fallback, and publishes a first set for the default scene. `process` does
      one `acquire_for_audio_thread()`, renders the early set into 16 accumulation buffers, runs
      the tail, sums, applies `mix` and `level` through `SmoothedParameter`, and writes two
      channels. A newly acquired set starts a 512-frame raised-cosine crossfade rendered from both
      banks, the arriving bank cleared first; the retiring bank becomes the next spare. The node
      owns exactly two banks and allocates nothing after `prepare`.
- [ ] **Step 4: RT safety.** Append to `test_rt_safety.cpp`, following the file's trap idiom with
      construction outside the trap:

```cpp
TEST(Unit_Dsp, RoomNodeProcessPerformsNoHeapAllocation)
TEST(Unit_Dsp, RoomNodeProcessPerformsNoHeapAllocationAcrossAPublish)
TEST(Unit_Dsp, EarlyReflectionRendererRenderPerformsNoHeapAllocation)
TEST(Unit_Dsp, LateReverbProcessPerformsNoHeapAllocation)
```

- [ ] **Step 5: Write `modules/room/README.md`** on the shape of `modules/dd3/README.md`: which
      class owns which block, which of them the audio thread calls, the two-bank crossfade rule,
      and the approximations named as such — the piston model's breakup radius, the six shipped
      materials, and the modal layer's damping error at a gypsum boundary.
- [ ] **Step 6: Syntax-check**, paste. **Step 7: Commit**
      `DE1: RoomNode renders the early set and the tail, and swaps sets without a click`.

**Acceptance:** with the omni microphone listener and the tail muted, the six first-order peaks in
the node's rendered impulse response land within half a sample of 423.078, 579.365, 667.333,
667.333, 775.888 and 914.881 frames at 48 kHz.

---

## Wave 8 (one task, serial)

### Task 25: `RoomNode` gains the wave layer

**Waits on:** Tasks 9, 15, 24.

**Files:**
- Modify: `modules/room/include/SushiDSP/room/RoomNode.hpp`, `modules/room/src/RoomNode.cpp`,
  `tests/functional/unit/test_room_node.cpp`, `tests/functional/unit/test_rt_safety.cpp`,
  `modules/room/README.md`

**Interfaces:**
- Consumes: `ModalBank`, `CrossoverPair`, `ModalDesign`, `RoomRenderSet::modal`,
  `RoomRenderSet::crossover_hz`.
- Produces: no new public signature. `RoomNode` gains one `ModalBank` and three `CrossoverPair` —
  one on the modal input and one per ear on the geometric sum — and honours
  `RoomRenderSet::crossover_hz`.

- [ ] **Step 1: Write the failing tests.**

```cpp
TEST(Unit_Dsp, RoomNodeRingsAtTheGaragesEigenfrequencies)
{
    // a unit impulse through the garage scene, 262144 frames at 48 kHz, a 262144-point FFT: the
    // first six peaks below f_s = 278.2 Hz sit at 28.58, 42.88, 51.53, 57.17, 68.60 and 71.46 Hz,
    // each within 0.5 % of its analytic value.
}

TEST(Unit_Dsp, RoomNodePressesTheBassIntoTheCorner)
{
    // the same scene with the listener at (0.15, 0.15, 0.15) and again at the room's centre
    // (3.0, 2.0, 1.25): the band energy from 20 Hz to 100 Hz is at least 12 dB higher in the
    // corner.
}

TEST(Unit_Dsp, RoomNodeIsContinuousAcrossTheSchroederFrequency)
{
    // third-octave band energies from 0.5 * f_s to 2 * f_s: no step between neighbouring bands
    // exceeds 2 dB.
}

TEST(Unit_Dsp, RoomNodeModeWidthsMatchTheirDamping)
{
    // each of the first six peaks' -3 dB width is within 20 % of 2.2 / decay_seconds, the figure
    // ModalSolver computed for that mode.
}

TEST(Unit_Dsp, RoomNodeSwitchesTheWaveLayerOffInADeadRoom)
{
    // a scene whose f_s falls under 40 Hz renders bit for bit the same with and without the bank,
    // because ModalDesign::count is zero.
}

TEST(Unit_Dsp, RoomNodePublishesNewModalGainsWithoutRebuildingAResonator)
{
    // moving only the cabinet leaves every mode's section coefficients bit-identical and changes
    // only source_gain and ear_gain. Spec §9a.2.
}
```

Write all six out in full.

- [ ] **Step 2: Confirm they fail**; paste the error.
- [ ] **Step 3: Implement.** The input splits through a `CrossoverPair` at
      `RoomRenderSet::crossover_hz`: the low half feeds `ModalBank` and the high half feeds the
      early renderer and the tail. Each ear's geometric sum passes its own high half and the modal
      output is added. A `crossover_hz` of zero routes everything to the geometric path and skips
      the bank.
- [ ] **Step 4: RT safety.** Append `TEST(Unit_Dsp, ModalBankProcessPerformsNoHeapAllocation)` and
      `TEST(Unit_Dsp, CrossoverPairProcessPerformsNoHeapAllocation)` to `test_rt_safety.cpp`.
- [ ] **Step 5: Update `modules/room/README.md`** with the wave layer and the ruling of 2026-09-08
      that put it in both quality modes.
- [ ] **Step 6: Syntax-check**, paste. **Step 7: Commit**
      `DE3: RoomNode runs the modal layer below the Schroeder frequency in every quality mode`.

**Acceptance:** the garage's rendered impulse response peaks at 28.58, 42.88, 51.53, 57.17, 68.60
and 71.46 Hz, each within 0.5 %, and a listener in the corner hears the 20 to 100 Hz band at least
12 dB louder than one at the room's centre.

---

## Wave 9 (one task, serial)

### Task 26: `RoomNode` gains the listener choice

**Waits on:** Tasks 6, 7, 20, 25.

**Files:**
- Modify: `modules/room/include/SushiDSP/room/RoomNode.hpp`, `modules/room/src/RoomNode.cpp`,
  `tests/functional/unit/test_room_node.cpp`, `modules/room/README.md`

**Interfaces:**
- Consumes: `ListenerKind`, `RoomRenderSet::listener`, and the directivity and microphone gains
  `ImageSourceSolver` has already folded into each tap's filter.
- Produces: no new signature. `set_parameter("listener", …)` rebuilds the render set on the control
  thread; say in the `@brief` that the audio thread applies the choice only through the next
  published set.

- [ ] **Step 1: Write the failing tests.**

```cpp
TEST(Unit_Dsp, RoomNodeMicrophoneListenerFillsBothChannelsIdentically)
{
    // listener = 1: the two output channels are equal bit for bit over 4096 frames.
}

TEST(Unit_Dsp, RoomNodeMicrophoneAtFiveCentimetresLiftsTheBottomEnd)
{
    // the microphone 5 cm from the cabinet against the same microphone at 3 m: the 100 Hz
    // third-octave band is 15.5 dB louder within 1.0 dB, which is Task 7's figure surviving the
    // whole chain.
}

TEST(Unit_Dsp, RoomNodeMicrophoneHearsLessRoomThanTheEars)
{
    // the direct-to-reverberant ratio at 5 cm exceeds the ears' at 2 m by at least 20 dB.
}

TEST(Unit_Dsp, RoomNodeMovingAroundTheCabinetChangesTheTone)
{
    // the ears at 60 degrees off axis at 2 m against on axis at 2 m: the 2 kHz third-octave band
    // drops by more than 6 dB while the 125 Hz band moves by under 1 dB.
}

TEST(Unit_Dsp, RoomNodeListenerChangeGoesThroughTheCrossfade)
{
    // switching listener mid-tone leaves the output within -60 dBFS of the same reference
    // raised-cosine crossfade Task 24 asserts for a scene publish.
}
```

Write all five out in full.

- [ ] **Step 2: Confirm they fail**; paste the error.
- [ ] **Step 3: Implement.** `set_parameter("listener", …)` stores the choice and rebuilds through
      the same path `set_scene` uses. The audio thread reads `RoomRenderSet::listener` and, for the
      microphone, takes channel 0 of the accumulation buffers alone and writes it to both outputs.
      Only one listener renders at a time (spec §8).
- [ ] **Step 4: Syntax-check**, paste. **Step 5: Commit**
      `DE4: RoomNode renders either the two ears or one microphone`.

**Acceptance:** the microphone listener at 5 cm reads 15.5 dB more in the 100 Hz third-octave band
than the same microphone at 3 m, within 1.0 dB, and its two output channels are bit-identical.

---

## Wave 10 (one task, serial)

### Task 27: `RoomNode` gains the binaural path

**Waits on:** Tasks 16, 17, 18, 26.

**Files:**
- Modify: `modules/room/include/SushiDSP/room/RoomNode.hpp`, `modules/room/src/RoomNode.cpp`,
  `tests/functional/unit/test_room_node.cpp`, `tests/functional/unit/test_rt_safety.cpp`,
  `modules/room/README.md`
- Modify: `apps/host_gui/main.cpp` — the load of `assets/hrtf/sadie2_ku100_48k.hrtf` beside the
  executable and the stderr line when it is absent. **This is the only task in the plan that
  touches `main.cpp`.**

**Interfaces:**
- Consumes: `BinauralDecoder`, `HrtfSetLoader`, `SphericalHeadHrtf`, `HrtfSource`,
  `RoomRenderSet::direct_left`, `direct_right`, `direct_itd_frames`, `direct_delay_frames`,
  `diffuse_left`, `diffuse_right`.
- Produces: `latency_samples()` starts reporting a real figure, and `hrtf_source()` starts
  answering `Measured` after a successful `set_hrtf`.

- [ ] **Step 1: Write the failing tests.**

```cpp
TEST(Unit_Dsp, RoomNodeDefaultsDirectDelayToCompensated)
{
    SushiDSP::room::RoomNode node;
    SushiDSP::PrepareInfo info{};   // 48 kHz, 256 frames, as the file's other cases build it
    node.prepare(info);
    EXPECT_FLOAT_EQ(node.get_parameter("direct_delay"), 0.0f);
    const std::uint32_t compensated = node.latency_samples();
    node.set_parameter("direct_delay", 1.0f);
    const std::uint32_t physical = node.latency_samples();
    EXPECT_EQ(compensated, 128u);            // one partition at 48 kHz
    EXPECT_EQ(physical - compensated, 280u); // round(2.0 m / 343 m/s * 48000)
}

TEST(Unit_Dsp, RoomNodeDirectOnsetMovesWithTheDirectDelayChoice)
{
    // the rendered direct-path onset in physical mode is 280 frames later than in compensated
    // mode, measured as the first frame above 1e-4 of the peak.
}

TEST(Unit_Dsp, RoomNodeReproducesWoodworthsInterauralDelayOnTheFallback)
{
    // a source at 30, 60 and 90 degrees azimuth, 2 m away: the lag of the interaural
    // cross-correlation peak is 0.2611, 0.4881 and 0.6558 ms, each within 20 microseconds, which
    // is one sample at 48 kHz.
}

TEST(Unit_Dsp, RoomNodeReportsSphericalHeadWithNoAssetAndMeasuredWithOne)
{
    // hrtf_source() is HrtfSource::SphericalHead after prepare(); after set_hrtf() with the
    // six-direction fixture it is HrtfSource::Measured; set_hrtf(HrtfSet{}) puts it back.
}

TEST(Unit_Dsp, RoomNodeTailIsCoherentLowAndIncoherentHigh)
{
    // the interaural cross-correlation of the tail alone is above 0.8 in the 125 Hz octave and
    // below 0.3 in the 4 kHz octave. Spec §6.5.
}

TEST(Unit_Dsp, RoomNodeDecoderMagnitudeMatchesTheMeasuredSet)
{
    // only when assets/hrtf/sadie2_ku100_48k.hrtf exists, else GTEST_SKIP() with a message naming
    // the path: 24 test directions, third-octave magnitudes from 200 Hz to 8 kHz within 3 dB of
    // the measured HRTF. Spec §9.5.
}

TEST(Unit_Dsp, RoomNodeCarriesTheRearNotchWithAMeasuredSet)
{
    // the same skip rule: a source at 180 degrees azimuth carries a spectral notch between 6 and
    // 10 kHz at least 6 dB deep.
}
```

Write the last six out in full.

- [ ] **Step 2: Confirm they fail**; paste the error.
- [ ] **Step 3: Implement.** The direct path gets its own fractional delay, its 1/r gain, its
      fitted filter and its two 128-tap HRIR convolutions, with the ITD applied to the far ear.
      The 16 accumulation buffers go through `BinauralDecoder`. The tail's two outputs pass the
      diffuse-field filters. Every path is delayed to match the longest, which is one partition
      plus — in `physical` mode — the direct propagation delay, and `latency_samples()` reports
      exactly that. State in a comment that the three binaural paths share one HRTF set, which is
      the invariant that keeps the seam inaudible.
- [ ] **Step 4: RT safety.** Append `TEST(Unit_Dsp, BinauralDecoderProcessPerformsNoHeapAllocation)`
      and `TEST(Unit_Dsp, RoomNodeBinauralProcessPerformsNoHeapAllocation)` to
      `test_rt_safety.cpp`.
- [ ] **Step 5: Add the host's load.** In `apps/host_gui/main.cpp`, beside the existing cabinet-IR
      load, resolve `assets/hrtf/sadie2_ku100_48k.hrtf` next to the executable, call
      `HrtfSetLoader::load`, install it through `dynamic_cast<IRoomHost*>`, and on failure print
      one stderr line naming the path and saying the room is running on the spherical-head model.
      Nothing else in `main.cpp` changes.
- [ ] **Step 6: Syntax-check**, paste. **Step 7: Commit**
      `DE5: RoomNode renders binaurally and defaults direct_delay to compensated`.

**Acceptance:** a fresh `RoomNode` reports `direct_delay` as 0 (`compensated`) with
`latency_samples()` of 128; setting it to `physical` raises that to 408, exactly the 280 frames
round(2.0 m / 343 m/s × 48 000) gives.

---

## Wave 11 (two tasks, parallel, disjoint files)

### Task 28: the room panel

**Waits on:** Task 27.

**Files:**
- Create: `apps/host_gui/ui/RoomPanel.hpp`, `apps/host_gui/ui/RoomPanel.cpp`
- Modify: `apps/host_gui/RigController.hpp`, `apps/host_gui/RigController.cpp`,
  `tests/functional/unit/test_rig_controller.cpp`

**Interfaces:**
- Consumes: `IRoomHost`, `RoomScene`, `MaterialId`, `Scattering`, `HrtfSource`,
  `IRoomHost::schroeder_frequency_hz`.
- Produces:

```cpp
namespace SushiDSP::ui
{
    /// @brief A top-down plan of the room: drag the cabinet, the head and the microphone, pick a
    /// material per surface, and read the Schroeder frequency the scene produced. Spec §14, DE6.
    void DrawRoomPanel(RigController& rig, std::size_t slot, std::string& room_error);
}
```

and on `RigController`:

```cpp
    static constexpr double kRoomPublishHz = 30.0;

    /// @brief The room node in a slot, or nullptr when that slot holds something else.
    SushiDSP::room::IRoomHost* room_host(std::size_t slot);

    /// @brief Publishes a scene at most kRoomPublishHz times a second, keeping the newest.
    bool set_room_scene(std::size_t slot, const SushiDSP::room::RoomScene& scene,
                        std::string& error_out);
```

- [ ] **Step 1: Implement the plan view.** A top-down rectangle scaled to the room, three draggable
      markers, and the three dimensions as fields. A drag writes a scene and calls
      `set_room_scene`, which drops any publish inside 1/30 s of the last and keeps the newest.
- [ ] **Step 2: Implement the surface list.** Six rows, each a combo of the shipped materials and a
      combo of the three scattering words, following the combo idiom in
      `apps/host_gui/ui/StagePanel.cpp`.
- [ ] **Step 3: Implement the readouts.** The computed Schroeder frequency, the mode count, and the
      HRTF source as a word: the file's name for `Measured`, or "spherical head — front and back
      sound alike" for `SphericalHead`.
- [ ] **Step 4: Put the scene in the preset.** `RigController::save_to_json` gains a `room` object
      per slot — dimensions, six materials, six scattering words, the three poses and the cabinet
      preset — and the loader defaults a missing `room` to `RoomScene{}`.
- [ ] **Step 5: Write the failing preset tests** in `test_rig_controller.cpp`:

```cpp
TEST(Unit_Dsp, RigControllerRoundTripsARoomSceneThroughJson)
{
    // save_to_json -> load -> save_to_json, string equality, with a non-default room on slot 1.
}

TEST(Unit_Dsp, RigControllerLoadsAPresetWithNoRoomAsTheDefaultScene)
{
    // a preset written before DE6 loads with scene() equal to RoomScene{}.
}

TEST(Unit_Dsp, RigControllerCoalescesADragToThirtyPublishesPerSecond)
{
    // 300 set_room_scene calls inside 100 ms publish at most 3 sets, counted through a stub
    // IRoomHost that increments on set_scene.
}
```

Write all three out in full.

- [ ] **Step 6: Syntax-check**, paste. **Step 7: Commit**
      `DE6: a top-down room panel places the cabinet, the head and the microphone`.

**Acceptance:** 300 `set_room_scene` calls inside 100 ms produce at most 3 publishes, and a
non-default scene round-trips through `save_to_json` to an identical string.

---

### Task 29: the benchmark rows

**Waits on:** Task 27.

**Files:**
- Modify: `tests/benchmark/main.cpp`

**Interfaces:**
- Consumes: `RoomNode`, `build_render_set`, and the `tests/benchmark/Benchmark.hpp` idiom the file
  already uses.
- Produces: eight printed rows, shaped like the file's existing ones.

- [ ] **Step 1: Add the steady-state row.** The garage scene, ears listener, 48 kHz, 256-frame
      blocks, 2000 blocks, reported in milliseconds per block. The spec's estimate is 1.63 ms and
      §12 item 10 asks for 1.8 ms or under.
- [ ] **Step 2: Add the swap row.** The same with a publish every second block, so every block
      renders the early set twice. Estimate 2.57 ms, ceiling 2.8 ms.
- [ ] **Step 3: Add the microphone row.** Estimate 1.05 ms.
- [ ] **Step 4: Add the three per-brick rows** for `EarlyReflectionRenderer::render`,
      `ModalBank::process` and `BinauralDecoder::process` on their own, because §10's table is per
      brick and DE7's decision is made on the first of them. Estimates 0.94, 0.32 and 0.22 ms.
- [ ] **Step 5: Add the room-beside-the-amplifier row.** A JCM800 chain and the room in one
      `RigGraph`, reporting the room's own share. Over 2.0 ms opens DE7.
- [ ] **Step 6: Syntax-check**, paste. **Step 7: Commit**
      `DE1: benchmark the room's steady state, its swap block and its three heaviest bricks`.

**Acceptance:** the benchmark prints all eight rows, and the maintainer's run records the
steady-state figure against the 1.8 ms ceiling and the room's share beside the JCM800 against the
2.0 ms figure that decides DE7.

---

## Wave 12 (one task, serial)

### Task 30: documentation

**Waits on:** Tasks 28 and 29.

**Files:**
- Modify: `docs/design/sushidsp.md` (a §9 roadmap row per DE phase and an "As built" section for
  the room path), `docs/README.md`, `docs/reference/CHANGELOG.md`,
  `docs/reference/KNOWN_ISSUES.md`, `modules/room/README.md`

- [ ] **Step 1: One CHANGELOG line per landed task**, under a new "DE — Room acoustics" heading
      naming the spec and this plan; each line the date, a past-tense verb, what changed and where
      in backticks, under 240 characters, with no reason and no second sentence.
- [ ] **Step 2: Add the known issues this plan knowingly leaves open**, each with its figure, its
      mechanism and what closes it: the eight untranscribed material rows; the SM57 preset's six
      missing numbers; the modal layer's 30 % damping error at a gypsum boundary (α = 0.29 at
      125 Hz); the FDN's own modes being crossed out rather than corrected; and
      `RigController::publish()` resetting a dragged node's state, which cuts the room's tail on a
      structural edit.
- [ ] **Step 3: Finish `modules/room/README.md`** — which class owns which block, which of them the
      audio thread calls, the two-bank crossfade, the six materials, and every approximation named
      as such.
- [ ] **Step 4: Add the `docs/README.md` entries** for `modules/room/` and `assets/hrtf/`, each
      naming the spec and this plan.
- [ ] **Step 5: Commit** `DE: document the room module, its assets and what it leaves open`.

**Acceptance:** every path and every link cited in the five changed documents resolves, checked by
opening each one.

---

## Wave summary

| Wave | Tasks | Waits on | Files, disjoint per wave | Build the controller runs |
| --- | --- | --- | --- | --- |
| 1 | 1, 2 | nothing | `BandResponse.hpp`; `tools/dsp/convert_hrtf.py` + `assets/hrtf/README.md` | one build; Task 2 needs none |
| 2 | 3, 4, 5, 6, 7, 8, 9, 10, 11 | 1 (tasks 3–7) | `Materials.*`, `AirAbsorption.*`, `BandResponseFit.*`, `CabinetDirectivity.*`, `MicrophoneModel.*`, the three record headers, `CrossoverPair.*`, `HrtfSet.*`, `AmbisonicEncoder.*`; one new unit test file each | one build for all nine, then `sd test` |
| 3 | 12, 13, 14, 15, 16, 17, 18 | 3, 6, 7 (12); 8 (13, 14, 15); 10 (16, 17, 18) | `RoomScene.*` + `tests/common/RoomFixtures.hpp`, `EarlyReflectionRenderer.*`, `LateReverb.*`, `ModalBank.*`, `SphericalHeadHrtf.*`, `HrtfSetLoader.*` + the fixture, `BinauralDecoder.*` | one build for all seven |
| 4 | 19, 20 | 3, 4, 12 (19); 3–8, 11, 12 (20) | `EyringDecay.*`; `ImageSourceSolver.*` | one build for both |
| 5 | 21, 22 | 5, 8, 19, 20 (21); 3, 8, 12, 19 (22) | `LateReverbDesign.*`; `ModalSolver.*` | one build for both |
| 6 | 23 | 16, 17, 20, 21, 22 | `RoomRenderSet.*`, `IRoomHost.hpp` | build, then `sd test` |
| 7 | 24 | 13, 14, 23 | `RoomNode.*`, `modules/room/README.md`, `test_room_node.cpp`, `test_rt_safety.cpp`, `NodeRegistry.cpp` | build, then every golden unchanged |
| 8 | 25 | 9, 15, 24 | `RoomNode.*`, `test_room_node.cpp`, `test_rt_safety.cpp` | build |
| 9 | 26 | 6, 7, 20, 25 | `RoomNode.*`, `test_room_node.cpp` | build |
| 10 | 27 | 16, 17, 18, 26 | `RoomNode.*`, `test_room_node.cpp`, `test_rt_safety.cpp`, `apps/host_gui/main.cpp` | build, then `sd test` |
| 11 | 28, 29 | 27 | `ui/RoomPanel.*` + `RigController.*` + `test_rig_controller.cpp`; `tests/benchmark/main.cpp` | one build for both, then the GUI opened by the maintainer |
| 12 | 30 | 28, 29 | the five documents | build |

`test_rt_safety.cpp` is held only by Tasks 24, 25 and 27, which sit in three different waves.
`apps/host_gui/main.cpp` is held only by Task 27. `RoomNode.{hpp,cpp}` is held only by Tasks 24 to
27, one wave each, which is why those four waves carry one task and why growing the node is four
builds rather than one.

## Self-review

**Spec coverage.** §3 → Tasks 1 to 23, one brick each, with `max/` deferred to DE8's own plan.
§4 → Tasks 3, 4, 5, 12, 20. §5 → Tasks 13, 23, 24. §6 → Tasks 8, 14, 19, 21. §7 → Task 6, folded
into the taps by Task 20 and heard in Task 26. §8 → Tasks 7 and 26. §9 → Tasks 2, 10, 11, 16, 17,
18, 27. §9a.1 to §9a.4, the wave layer → Tasks 9, 15, 22, 25. §9a.3, the ray tail → DE8's plan.
§10 → Task 29. §11 → what no task builds; the two items this plan adds to that list, the
untranscribed materials and the missing SM57 curve, land in Task 30's known issues. §12 item by
item: 1 → Task 20, 2 → Task 5, 3 → Tasks 19 and 21, 4 → Task 27, 5 → Task 6, 6 → Tasks 7 and 26,
7 → Tasks 16 and 27, 8 → Task 24, 9 → the traps in Tasks 24, 25 and 27, 10 → Task 29, 11 → Task 27,
12 → Task 25, 14 → Tasks 2, 17 and 27; item 13 is DE8's. §14 → the wave table above, in the same
dependency order.

**The three rulings of 2026-09-08.** The modal layer in both quality modes: Tasks 9, 15, 22 and 25
build it with no `quality` gate, and Task 25's acceptance is measured in the hybrid mode.
`direct_delay` defaulting to `compensated`: the parameter table in Task 24 and the acceptance in
Task 27, which asserts both the default and the 280-frame difference. The asset in the tree: Task 2
writes the converter and the README that records the Apache 2.0 licence and the citation, Task 17
reads the file, and Task 27 asserts both `HrtfSource` answers and the host's stderr line.

**I checked the arithmetic in every acceptance criterion, and two of the spec's numbers were
wrong.** The ITD at 60° is (0.0875/343)·(π/3 + sin(π/3)) = 0.4881 ms, not the 0.479 ms the draft
printed; §9.5 and §12 item 7 now carry 0.488 and say the draft was wrong. The cardioid at 30 cm
gains 3.7 dB at 100 Hz under the same mixed pressure-plus-gradient model that produces the 15.5 dB
the spec asserts at 5 cm, not the 6.8 dB the draft printed, which is the pure gradient form; §8 now
carries 3.7 dB and names which half of the pattern the boost applies to. Everything else recomputed
as the spec has it: image counts 7, 25, 63, 129; t_c = 5·2.5/343 = 36.44 ms; the 8397-frame ring
for a 12 m room; the 25 200-frame line for a 3 s RT60; 1.41 Mflop for 128 taps × 43 flops × 256
frames and 0.94 ms at 1.5 Gflop/s; 0.48 Mflop and 0.32 ms for 143 modes × 13 flops; k·a = 1 at
436.7 Hz and 4.58 at 2 kHz; 280 frames of propagation at 2 m and 48 kHz. The reference rooms' RT60
figures, their Schroeder frequencies, the garage's 187 modes, its sixteen prime line lengths
summing to 79 882 frames against the 78 003 the overlap rule demands, the seven first-order arrival
times, the piston and array factors at 2 kHz, and the two SN3D encode vectors were each computed
for this plan and are what the tests assert.

**Placeholder scan.** No task says "add error handling" or "similar to Task N". The two places this
plan cannot supply a number say so and say who supplies it: the eight untranscribed material rows
(Task 3) and the SM57's six band values (Task 7). Both are forbidden to invent and both are in
Task 30's known issues, so neither goes quiet.

**Type consistency.** `BandResponse`, `kBandCount`, `kBandCentresHz`, `kSpeedOfSound`,
`MaterialId`, `Scattering`, `reflection_factor`, `AirAbsorption`, `BandResponseFit::Coefficients`,
`CabinetGeometry`, `PolarPattern`, `EarlyTap`, `EarlyTapSet`, `FdnDesign`, `ModalDesign`,
`CrossoverPair`, `HrtfSet::Pair`, `AmbisonicEncoder::kChannels`, `RoomScene`, `Surface`,
`ListenerKind`, `EyringDecay`, `ImageSourceSolver::Options`, `LateReverbDesign`, `ModalSolver`,
`RoomRenderSet`, `HrtfSource`, `IRoomHost`, `RoomNode`, `room_descriptor`, `DrawRoomPanel` — each
is defined in the task that first produces it and spelled the same wherever it is consumed.
`ListenerKind` is declared in `ImageSourceSolver.hpp` in Task 20 and appears in Task 21's
signature, which is why Task 21 waits on Task 20 and not only on Task 19.

**The one number that is a judgement rather than a derivation.** The 2562-point spherical grid in
Tasks 6 and 11 is an icosahedron subdivided four times. Any grid whose quadrature error stays under
0.1 dB would do; this is the smallest subdivision that clears it.

