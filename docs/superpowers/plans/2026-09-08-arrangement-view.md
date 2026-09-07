# Arrangement view: bars, a metronome, clips and dropped files (DT) Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Give the host musical time — a ruler in bars, a metronome that clicks the beat, lanes
holding clips placed at a frame, a file dragged onto a lane, and a playback speed that slows a
take down the way a tape machine does.

**Architecture:** Five leaf bricks land first, none of which knows about any other: the
frames-to-bars mapping, the control-thread owner of takes, the variable-rate clip reader, the
transport's speed word, and the queue that carries an Explorer drop into the GUI frame. Three
consumers follow — the metronome, the immutable per-lane clip list, and the pixels-to-frames
conversion. `TrackPlayer` is then rebuilt on top of all of them, the host controller learns to
place a clip and save one, and the UI is replaced last.

**Tech Stack:** C++17, gtest, `SushiDSP/core` (`RtSwapSlot`, `SmoothedParameter`, `SpscRing`),
`SushiDSP/math/Resampler`, `SushiDSP/capture` (`Take`, `Transport`, `TrackPlayer`,
`RecordingBus`), Dear ImGui and GLFW in `apps/host_gui`, `sd build` / `sd test`.

**Spec:** `docs/superpowers/specs/2026-09-08-arrangement-view-design.md`

## Global Constraints

- C++17; Allman braces; `snake_case` functions, `PascalCase` types, `kPascalCase` constants,
  trailing-underscore members; no abbreviations.
- Every `.hpp`/`.cpp`, tests included, opens with the 76-column license box copied verbatim from
  `include/SushiDSP/core/PortLayout.hpp`, only the filename line changed, then a `@file` block
  with one `@brief` and `@author`. `@brief` on every symbol, private members included. A comment
  states an invariant, never a reason; the reason lives in the spec and is cited by path.
- The core (`include/SushiDSP`, `src/`) depends on the C++17 standard library and SIMD intrinsics
  only. `modules/capture` depends only on `sushi_dsp`. Only `apps/host_gui` may gain a
  third-party dependency, and only with the maintainer's word.
- Anything reachable from `process()`, from `Metronome::process` or from a real-time `write()` is
  allocation-free, lock-free, syscall-free and throw-free, and carries an `operator new` trap case
  in `tests/functional/unit/test_rt_safety.cpp`.
- **No `std::shared_ptr` in any type the audio thread reads.** A clip borrows a raw
  `const Take*`; `TakeLibrary` owns takes on the control thread and `RtSwapSlot::reclaim()` is
  what makes the borrow safe. Spec §5.
- Playback speed is a resampling ratio, so the pitch falls with the speed. Nothing in this plan
  stretches time. Spec §5a and §9.
- The metronome reaches no recorded file: it is not an `IAudioNode`, and the host calls it after
  `RecordingBus::write`. Spec §4.
- `kMaxTracks` = 8, `kMaxClips` = 16, one tempo and one meter per session.
- Parameter order in a `NodeDescriptor` is a contract: appended, never reordered.
- **Subagents run no `sd`, `cmake`, `ninja` or `ctest` and edit no CMake file.** The controller —
  the maintainer — owns every `CMakeLists.txt`, runs every build and every test run, and never
  terminates a running `sushidsp_host_gui.exe`. A task's last step is a syntax check through
  `compile_commands.json` with `clang -fsyntax-only`, pasted.
- Commit by path with a plain `-m` subject, never `--amend`; another session shares this
  checkout. Trailers on every commit.

---

## Wave 1 (five tasks, parallel, disjoint files)

### Task 1: `MusicalClock`

**Files:**
- Create: `modules/capture/include/SushiDSP/capture/MusicalClock.hpp`,
  `modules/capture/src/MusicalClock.cpp`
- Test: `tests/functional/unit/test_musical_clock.cpp`
- Controller adds the source to `modules/capture/CMakeLists.txt` and the test to
  `tests/CMakeLists.txt`.

**Interfaces:**
- Consumes: nothing.
- Produces:

```cpp
namespace SushiDSP
{
    struct BarBeat
    {
        std::uint64_t bar;      ///< Bar number, counted from 1.
        std::uint32_t beat;     ///< Beat within the bar, counted from 1.
        double        fraction; ///< Position inside the beat, in [0, 1).
    };

    class MusicalClock
    {
    public:
        void set_tempo(double beats_per_minute);
        void set_meter(std::uint32_t beats_per_bar, std::uint32_t beat_unit);
        void set_sample_rate(double sample_rate);

        /// @brief Frames per beat in 32.32 fixed point. Audio thread; one relaxed load.
        std::uint64_t beat_step_q32() const noexcept;

        std::uint32_t beats_per_bar() const noexcept;
        double        beats_per_minute() const noexcept;
        std::uint32_t beat_unit() const noexcept;

        BarBeat       position_of(std::uint64_t frame) const;
        std::uint64_t frame_of(std::uint64_t bar, std::uint32_t beat) const;
    };
}
```

- [ ] **Step 1: Write the failing tests** in `test_musical_clock.cpp`, following the
      `TEST(Unit_Dsp, ...)` naming every file in `tests/functional/unit` uses:

```cpp
TEST(Unit_Dsp, MusicalClockPlacesBarOneHundredAtTheExactFrame)
{
    SushiDSP::MusicalClock clock;
    clock.set_sample_rate(48000.0);
    clock.set_tempo(120.0);
    clock.set_meter(4, 4);
    EXPECT_EQ(clock.frame_of(100, 1), 9504000u);
    const SushiDSP::BarBeat at = clock.position_of(9504000u);
    EXPECT_EQ(at.bar, 100u);
    EXPECT_EQ(at.beat, 1u);
    EXPECT_NEAR(at.fraction, 0.0, 1e-9);
}

TEST(Unit_Dsp, MusicalClockRoundTripsEveryFrameToItsOwnBeat)
{
    for (const double rate : { 44100.0, 48000.0, 96000.0, 192000.0 })
    {
        for (const double bpm : { 120.0, 137.4 })
        {
            SushiDSP::MusicalClock clock;
            clock.set_sample_rate(rate);
            clock.set_tempo(bpm);
            clock.set_meter(4, 4);
            const std::uint64_t step = clock.beat_step_q32();
            for (std::uint64_t frame = 0; frame < 4000000u; frame += 7919u)
            {
                const SushiDSP::BarBeat at = clock.position_of(frame);
                const std::uint64_t beat_start = clock.frame_of(at.bar, at.beat);
                EXPECT_LE(beat_start, frame);
                EXPECT_LT(frame - beat_start, (step >> 32) + 1u);
            }
        }
    }
}

TEST(Unit_Dsp, MusicalClockCountsTheBeatTheMeterNames)
{
    SushiDSP::MusicalClock clock;
    clock.set_sample_rate(48000.0);
    clock.set_tempo(120.0);
    clock.set_meter(6, 8);
    // A 120 BPM quarter is 24000 frames, so an eighth is 12000 and a 6/8 bar is 72000.
    EXPECT_EQ(clock.beat_step_q32() >> 32, 12000u);
    EXPECT_EQ(clock.frame_of(2, 1), 72000u);
}

TEST(Unit_Dsp, MusicalClockDoesNotAccumulateErrorAcrossFiveMinutes)
{
    SushiDSP::MusicalClock clock;
    clock.set_sample_rate(48000.0);
    clock.set_tempo(137.4);
    clock.set_meter(4, 4);
    const std::uint64_t step = clock.beat_step_q32();
    for (std::uint64_t beat = 0; beat < 687u; ++beat)
    {
        const std::uint64_t expected = (beat * step) >> 32;
        const std::uint64_t bar = beat / 4u + 1u;
        const std::uint32_t in_bar = static_cast<std::uint32_t>(beat % 4u) + 1u;
        EXPECT_EQ(clock.frame_of(bar, in_bar), expected);
    }
}
```

- [ ] **Step 2: Confirm they fail** by reading the compiler's diagnostic on the missing header;
      the controller runs the build, so paste the `clang -fsyntax-only` error naming
      `MusicalClock.hpp` as the evidence.
- [ ] **Step 3: Implement.** One `std::atomic<std::uint64_t> beat_step_q32_`, recomputed by each
      setter as `static_cast<std::uint64_t>(sample_rate_ * 60.0 / bpm_ * (beat_unit_ / 4.0
      inverted) * 4294967296.0)` — write the arithmetic so a 4/4 quarter at 120 BPM and 48 kHz is
      exactly `24000ull << 32`, and a 6/8 eighth is `12000ull << 32`. `beats_per_bar_`,
      `beat_unit_` and `beats_per_minute_` are plain members read only by the control thread.
      `position_of` is `beat = (frame << 32) / step` guarded against a zero step, then
      `bar = beat / beats_per_bar_ + 1`, `beat_in_bar = beat % beats_per_bar_ + 1`, and the
      fraction from the remainder. `frame_of` is `(((bar - 1) * beats_per_bar_ + (beat - 1)) *
      step) >> 32`. The class holds no position.
- [ ] **Step 4: Syntax-check** with `clang -fsyntax-only` through `compile_commands.json` and
      paste the command and its output.
- [ ] **Step 5: Commit** `git add modules/capture/include/SushiDSP/capture/MusicalClock.hpp
      modules/capture/src/MusicalClock.cpp tests/functional/unit/test_musical_clock.cpp` then
      `git commit -m "DT0: MusicalClock maps frames to bars and beats in 32.32 fixed point"`.

**Acceptance:** bar 100 at 120 BPM, 4/4, 48 kHz is frame 9504000 exactly, and `frame_of` of every
beat over five minutes equals `(beat * step) >> 32` with no accumulated error.

---

### Task 2: `TakeLibrary`

**Files:**
- Create: `modules/capture/include/SushiDSP/capture/TakeLibrary.hpp`,
  `modules/capture/src/TakeLibrary.cpp`
- Test: `tests/functional/unit/test_take_library.cpp`
- Controller adds the source and the test to CMake.

**Interfaces:**
- Consumes: `SushiDSP::Take` from `modules/capture/include/SushiDSP/capture/Take.hpp`.
- Produces:

```cpp
namespace SushiDSP
{
    /// @brief Owns every loaded Take and hands out borrowed pointers for clips to hold.
    /// Control thread only: it has no atomics and no real-time entry point. Spec §5.
    class TakeLibrary
    {
    public:
        const Take* insert(std::unique_ptr<const Take> take);
        void        retain(const Take* take);
        void        release(const Take* take);
        void        collect();
        std::size_t size() const noexcept;
        std::size_t reference_count(const Take* take) const noexcept;
    };
}
```

- [ ] **Step 1: Write the failing tests.** `Take` has no virtual destructor to instrument, so the
      tests assert on the library's own accounting — `size()` and `reference_count()` — which is
      what the audio thread's safety rests on anyway:

```cpp
TEST(Unit_Dsp, TakeLibraryKeepsATakeAliveWhileSomethingHoldsIt)
{
    SushiDSP::TakeLibrary library;
    const SushiDSP::Take* take =
        library.insert(std::make_unique<const SushiDSP::Take>(1u, 48000.0, 128u));
    library.retain(take);
    library.collect();
    EXPECT_EQ(library.size(), 1u);
    EXPECT_EQ(library.reference_count(take), 1u);
}

TEST(Unit_Dsp, TakeLibraryFreesATakeOnlyWhenTheLastHolderReleasesIt)
{
    SushiDSP::TakeLibrary library;
    const SushiDSP::Take* take =
        library.insert(std::make_unique<const SushiDSP::Take>(1u, 48000.0, 128u));
    library.retain(take);
    library.retain(take);
    library.release(take);
    library.collect();
    EXPECT_EQ(library.size(), 1u);
    library.release(take);
    library.collect();
    EXPECT_EQ(library.size(), 0u);
}

TEST(Unit_Dsp, TakeLibraryDropsATakeNothingEverHeld)
{
    SushiDSP::TakeLibrary library;
    library.insert(std::make_unique<const SushiDSP::Take>(1u, 48000.0, 128u));
    library.collect();
    EXPECT_EQ(library.size(), 0u);
}

TEST(Unit_Dsp, TakeLibraryGivesTwoHoldersOneCopyOfTheSamples)
{
    SushiDSP::TakeLibrary library;
    const SushiDSP::Take* first =
        library.insert(std::make_unique<const SushiDSP::Take>(2u, 48000.0, 4096u));
    library.retain(first);
    library.retain(first);
    const SushiDSP::Take* second = first;
    EXPECT_EQ(first->channel(0), second->channel(0));
    EXPECT_EQ(library.size(), 1u);
}
```

- [ ] **Step 2: Confirm they fail** on the missing header; paste the `clang -fsyntax-only` error.
- [ ] **Step 3: Implement.** One `std::vector<std::pair<std::unique_ptr<const Take>,
      std::size_t>>`; `insert` moves in with a count of zero and returns
      `entry.first.get()`; `retain` and `release` find the entry by pointer and step the count,
      and `release` on a count already zero is a programming error the `@pre` names; `collect`
      erases every entry at zero. A `Take` is heap-allocated once and never moves, so the borrowed
      pointer stays good for its whole life — state that as an invariant comment on `insert`.
- [ ] **Step 4: Syntax-check**, paste.
- [ ] **Step 5: Commit** `DT2: TakeLibrary owns takes on the control thread so clips can borrow them`.

**Acceptance:** a take inserted and retained twice survives one release and one `collect()`, and
is gone after the second release and `collect()`.

---

### Task 3: `VariRateReader`

**Files:**
- Create: `modules/capture/include/SushiDSP/capture/VariRateReader.hpp`,
  `modules/capture/src/VariRateReader.cpp`
- Test: `tests/functional/unit/test_vari_rate_reader.cpp`
- Modify: `tests/functional/unit/test_rt_safety.cpp` (append one case at the end of the file)
- Controller adds the source and the test to CMake.

**Interfaces:**
- Consumes: `SushiDSP::Resampler` from `include/SushiDSP/math/Resampler.hpp`, `SushiDSP::Take`.
- Produces:

```cpp
namespace SushiDSP
{
    /// @brief Reads a Take at a chosen speed, one Resampler per channel, in exact output counts.
    /// The pitch falls with the speed; nothing here stretches time. Spec §5a.
    class VariRateReader
    {
    public:
        static constexpr double kMinSpeed = 0.25;
        static constexpr double kMaxSpeed = 4.0;
        static constexpr std::uint32_t kMaxChannels = 2u;

        void prepare(std::uint32_t channels, std::uint32_t max_output_frames);
        void reset() noexcept;

        /// @brief Reads exactly output_frames samples per channel, starting at take_frame.
        /// @return The take frame the next call should start at.
        std::uint64_t read(const Take& take, std::uint64_t take_frame, double speed,
                           float* const* outputs, std::uint32_t channels,
                           std::uint32_t output_frames) noexcept;
    };
}
```

- [ ] **Step 1: Write the failing tests.**

```cpp
TEST(Unit_Dsp, VariRateReaderAtHalfSpeedLastsTwiceAsLong)
{
    SushiDSP::Take take(1u, 48000.0, 48000u);
    for (std::size_t frame = 0; frame < 48000u; ++frame)
    {
        take.mutable_channel(0)[frame] =
            static_cast<float>(std::sin(2.0 * 3.14159265358979 * 440.0 * frame / 48000.0));
    }
    SushiDSP::VariRateReader reader;
    reader.prepare(1u, 256u);
    std::vector<float> output(96000u, 0.0f);
    std::uint64_t cursor = 0u;
    for (std::uint32_t block = 0; block < 375u; ++block)
    {
        float* pointers[1] = { output.data() + block * 256u };
        cursor = reader.read(take, cursor, 0.5, pointers, 1u, 256u);
    }
    EXPECT_NEAR(static_cast<double>(cursor), 48000.0, 64.0);
}

TEST(Unit_Dsp, VariRateReaderAtHalfSpeedDropsAnOctave)
// 32768-point FFT of the output above; the peak bin is 220 Hz +/- one bin, not 440 Hz.

TEST(Unit_Dsp, VariRateReaderReturnsExactlyTheFramesAsked)
// every speed in { 0.25, 0.5, 1.0, 1.5, 2.0, 4.0 }: 256 output frames written, 256 read back.

TEST(Unit_Dsp, VariRateReaderClampsASpeedOutsideItsRange)
// speed 0.1 behaves as 0.25 and speed 9.0 as 4.0, measured by the returned cursor.

TEST(Unit_Dsp, VariRateReaderResetStartsFromSilence)
// read a full-scale take, reset(), read a silent take: the first output block is below -120 dBFS.
```

Write each of the four sketched cases out in full, with the same FFT idiom
`tests/functional/unit` already uses for spectral assertions.

- [ ] **Step 2: Confirm they fail**; paste the `clang -fsyntax-only` error.
- [ ] **Step 3: Implement.** One `Resampler` per channel, `prepare`d with
      `source_rate = 1.0`, `target_rate = 1.0`, `max_input_frames` = 1 and
      `minimum_ratio = 1.0 / kMaxSpeed`. `read` clamps the speed, calls `set_rates(speed, 1.0)`
      when it moved — `Resampler::set_rates` is real-time safe — then pushes take samples one
      frame at a time into each channel's converter until `output_frames` outputs have been
      written, zero-filling past the take's end. The loop is bounded by
      `output_frames * kMaxSpeed` plus the kernel's half span; say so in the `@brief`. It
      allocates nothing.
- [ ] **Step 4: RT safety.** Append to `test_rt_safety.cpp`:

```cpp
TEST(Unit_Dsp, VariRateReaderReadPerformsNoHeapAllocation)
```

following the file's existing trap idiom — prepare outside the trap, run 200 blocks at 0.25, 1.0
and 4.0 inside it.
- [ ] **Step 5: Syntax-check**, paste. **Step 6: Commit**
      `DT2: VariRateReader reads a take at a speed, with the pitch following it`.

**Acceptance:** a 440 Hz take read at speed 0.5 peaks at 220 Hz in a 32768-point FFT, lasts twice
as long, and `read` allocates nothing.

---

### Task 4: `Transport` gains a speed

**Files:**
- Modify: `modules/capture/include/SushiDSP/capture/Transport.hpp`,
  `modules/capture/src/Transport.cpp`
- Create: `tests/functional/unit/test_transport.cpp` — the tree has no test file for `Transport`
  today; its cases live inside `tests/functional/unit/test_track_player.cpp`. Add the new file
  rather than growing that one, and leave the existing cases where they are. Controller adds it
  to `tests/CMakeLists.txt`.

**Interfaces:**
- Consumes: nothing.
- Produces:

```cpp
    /// @brief Sets how fast the playhead moves, in timeline frames per output frame.
    /// Clamped to [VariRateReader::kMinSpeed, VariRateReader::kMaxSpeed]. Control thread.
    void set_speed(double speed) noexcept;

    /// @brief The playback speed in 32.32 fixed point. Audio thread; one relaxed load.
    std::uint64_t speed_q32() const noexcept;

    /// @brief The playback speed. Control thread; for the UI.
    double speed() const noexcept;
```

`Transport` must not include `VariRateReader.hpp` — it would invert the dependency. Declare
`static constexpr double kMinSpeed = 0.25;` and `kMaxSpeed = 4.0;` on `Transport` and add a
`static_assert` in `VariRateReader.cpp` that the two pairs agree.

- [ ] **Step 1: Write the failing tests** in the new `test_transport.cpp`:

```cpp
TEST(Unit_Dsp, TransportAtHalfSpeedAdvancesHalfAsFar)
{
    SushiDSP::Transport transport;
    transport.play();
    transport.set_speed(0.5);
    for (std::uint32_t block = 0; block < 100u; ++block) { transport.advance(256u); }
    EXPECT_EQ(transport.position_frames(), 12800u);
}

TEST(Unit_Dsp, TransportCarriesTheSpeedFractionAcrossBlocks)
{
    SushiDSP::Transport transport;
    transport.play();
    transport.set_speed(0.3);
    for (std::uint32_t block = 0; block < 1000u; ++block) { transport.advance(64u); }
    EXPECT_EQ(transport.position_frames(), 19200u);   // exactly 0.3 * 64000, no drift
}

TEST(Unit_Dsp, TransportClampsASpeedOutsideItsRange)
TEST(Unit_Dsp, TransportSeekClearsTheSpeedFraction)
TEST(Unit_Dsp, TransportAtUnitySpeedAdvancesExactlyAsBefore)
```

The last one is the regression guard: every existing `advance` assertion in
`test_track_player.cpp` must still pass untouched.

- [ ] **Step 2: Confirm they fail**; paste the `clang -fsyntax-only` error on `set_speed`.
- [ ] **Step 3: Implement.** `std::atomic<std::uint64_t> speed_q32_{ 1ull << 32 }` and a
      `std::uint64_t speed_remainder_` the audio thread alone touches. `advance(frames)` computes
      `const std::uint64_t total = frames * speed_q32_.load(std::memory_order_relaxed) +
      speed_remainder_;` advances the position by `total >> 32` and keeps `total & 0xffffffffull`.
      `seek` zeroes the remainder. The loop wrap is unchanged and still reads the packed loop word.
      At `speed_q32_ == (1ull << 32)` the remainder stays zero and the arithmetic reduces to
      `position += frames`, which is why the unity path is bit-identical.
- [ ] **Step 4: Syntax-check**, paste. **Step 5: Commit**
      `DT2: Transport carries a playback speed in 32.32 fixed point`.

**Acceptance:** 1000 blocks of 64 frames at speed 0.3 land the position on frame 19200 exactly,
and unity speed advances as it did before.

---

### Task 5: `FileDropQueue`

**Files:**
- Create: `apps/host_gui/FileDropQueue.hpp`, `apps/host_gui/FileDropQueue.cpp`
- Test: `tests/functional/unit/test_file_drop_queue.cpp`
- Controller adds the source and the test to CMake.

**Interfaces:**
- Consumes: nothing.
- Produces:

```cpp
namespace SushiDSP::host
{
    struct FileDrop
    {
        std::string path;
        double      screen_x;
        double      screen_y;
    };

    /// @brief Carries paths from GLFW's drop callback, which fires outside the ImGui frame,
    /// into the next frame that can route them. Spec §7.
    class FileDropQueue
    {
    public:
        void push(std::string path, double screen_x, double screen_y);
        /// @brief Moves everything queued into out and empties the queue.
        void drain(std::vector<FileDrop>& out);
        bool empty() const;
    };
}
```

A `std::mutex` is correct here and is not a violation: neither `push` nor `drain` runs on the
audio thread. Say so in the class `@brief`.

- [ ] **Step 1: Write the failing tests.**

```cpp
TEST(Unit_Dsp, FileDropQueueDrainsInTheOrderPushed)
TEST(Unit_Dsp, FileDropQueueIsEmptyAfterADrain)
TEST(Unit_Dsp, FileDropQueueDrainAppendsToACallerSVector)
TEST(Unit_Dsp, FileDropQueueSurvivesConcurrentPushAndDrain)
```

Write all four out in full; the last pushes 1000 paths from one `std::thread` while the main
thread drains in a loop, and asserts the union of everything drained is the 1000 paths with none
lost and none duplicated.

- [ ] **Step 2: Confirm they fail**; paste the error.
- [ ] **Step 3: Implement.** `std::vector<FileDrop>` under a `std::mutex`; `drain` swaps into a
      local under the lock and appends outside it.
- [ ] **Step 4: Syntax-check**, paste. **Step 5: Commit**
      `DT4: FileDropQueue carries an Explorer drop into the next GUI frame`.

**Acceptance:** 1000 paths pushed from a second thread all come out of `drain`, once each, in
order.

---

## Wave 2 (three tasks, parallel, disjoint files)

### Task 6: `Metronome`

**Waits on:** Task 1.

**Files:**
- Create: `modules/capture/include/SushiDSP/capture/Metronome.hpp`,
  `modules/capture/src/Metronome.cpp`
- Test: `tests/functional/unit/test_metronome.cpp`
- Modify: `tests/functional/unit/test_rt_safety.cpp` (append one case)
- Controller adds the source and the test to CMake.

**Interfaces:**
- Consumes: `MusicalClock::beat_step_q32()`, `MusicalClock::beats_per_bar()` from Task 1;
  `Transport::position_frames()`, `Transport::state()`.
- Produces:

```cpp
namespace SushiDSP
{
    /// @brief An RT-safe click source that reads a Transport and adds into an output. It is not
    /// an IAudioNode, and the host calls it after RecordingBus::write, so the click reaches no
    /// recorded file. Spec §4.
    class Metronome
    {
    public:
        Metronome();

        void  set_enabled(bool enabled) noexcept;
        bool  enabled() const noexcept;
        void  set_level(float level) noexcept;
        float level() const noexcept;

        void process(float* const* outputs, std::uint32_t channels, std::uint32_t frames,
                     const Transport& transport, const MusicalClock& clock) noexcept;
    };
}
```

- [ ] **Step 1: Write the failing tests.**

```cpp
TEST(Unit_Dsp, MetronomeAddsNothingWhenDisabled)
TEST(Unit_Dsp, MetronomeAddsNothingWhenTheTransportIsStopped)
TEST(Unit_Dsp, MetronomeStartsAClickOnEveryBeatFrame)
// 5 minutes at 137.4 BPM, 48 kHz, blocks of 256: every onset frame — the first frame of a run
// whose magnitude crosses 1e-4 after silence — equals clock.frame_of() of its beat, exactly.
TEST(Unit_Dsp, MetronomeKeepsTheGridAcrossASeek)
// seek to frame 1234567 mid-render; the onsets after it still land on frame_of().
TEST(Unit_Dsp, MetronomeAccentsTheDownbeat)
// the beat-0-of-bar burst peaks at 1760 Hz, the others at 880 Hz, by FFT of one burst each.
TEST(Unit_Dsp, MetronomeCarriesABurstAcrossABlockBoundary)
// a 30 ms burst at 48 kHz is 1440 frames, so it spans six 256-frame blocks with no discontinuity
// larger than the waveform's own step.
```

- [ ] **Step 2: Confirm they fail**; paste the error.
- [ ] **Step 3: Implement.** The constructor builds two `std::vector<float>` one-shots, 30 ms at
      the sample rate the host will run — take the sample rate in `Metronome(double sample_rate)`
      and rebuild on a control-thread `set_sample_rate(double)` — 1760 Hz and 880 Hz sines under
      an exponential decay with a 1 ms raised-cosine attack. `process` returns immediately when
      disabled or stopped. Otherwise it reads `transport.position_frames()` and
      `clock.beat_step_q32()` once, derives the first beat index at or after the block start by
      integer division, and for each beat inside the block writes the burst from its frame offset.
      The only state is `burst_position_` and `burst_is_downbeat_`, which carry an overrunning
      burst into the next block. Derive every beat from the absolute position; never accumulate.
- [ ] **Step 4: RT safety.** Append `TEST(Unit_Dsp, MetronomeProcessPerformsNoHeapAllocation)` to
      `test_rt_safety.cpp`, constructing outside the trap.
- [ ] **Step 5: Syntax-check**, paste. **Step 6: Commit**
      `DT1: Metronome clicks the beat off the transport position, outside the rig`.

**Acceptance:** over a five-minute render at 137.4 BPM including a mid-render seek, every click
onset frame equals `MusicalClock::frame_of` of its beat, to the sample.

---

### Task 7: `Clip` and `TrackTimeline`

**Waits on:** Task 2.

**Files:**
- Create: `modules/capture/include/SushiDSP/capture/TrackTimeline.hpp`,
  `modules/capture/src/TrackTimeline.cpp`
- Test: `tests/functional/unit/test_track_timeline.cpp`
- Controller adds the source and the test to CMake.

**Interfaces:**
- Consumes: `TakeLibrary::retain`, `TakeLibrary::release` from Task 2; `SushiDSP::Take`.
- Produces:

```cpp
namespace SushiDSP
{
    struct Clip
    {
        const Take*   take;          ///< Borrowed; owned by the TakeLibrary that built this timeline.
        std::uint64_t start_frame;   ///< Timeline frame the clip's first sample lands on.
        std::uint64_t take_offset;   ///< First take frame the clip plays.
        std::uint64_t length_frames; ///< How many take frames the clip plays.
    };

    static_assert(std::is_trivially_copyable<Clip>::value,
                  "A clip is read on the audio thread and must hold no refcount. Spec §5.");

    /// @brief One lane's clips, sorted by start_frame and immutable once published. Built on the
    /// control thread, read on the audio thread through RtSwapSlot. Spec §5.
    class TrackTimeline
    {
    public:
        static constexpr std::size_t kMaxClips = 16;
        static constexpr std::uint64_t kFadeFrames = 240;  ///< 5 ms at 48 kHz.

        /// @brief Builds a lane. Retains every distinct take in library.
        /// @param clips Sorted by start_frame on construction; must not overlap.
        /// @throw std::runtime_error when two clips overlap, naming both, or when clips
        /// holds more than kMaxClips entries.
        TrackTimeline(TakeLibrary& library, const std::vector<Clip>& clips);
        ~TrackTimeline();   ///< Releases every take it retained. Control thread only.

        TrackTimeline(const TrackTimeline&) = delete;
        TrackTimeline& operator=(const TrackTimeline&) = delete;

        std::size_t clip_count() const noexcept;
        const Clip& clip(std::size_t index) const noexcept;

        /// @brief First clip whose end is past frame. Audio thread; at most kMaxClips comparisons.
        /// @return An index into clip(), or clip_count() when none is.
        std::size_t first_clip_at_or_after(std::uint64_t frame) const noexcept;

        /// @brief The de-click ramp gain for a frame inside a clip, in [0, 1]. Audio thread.
        static float edge_gain(const Clip& clip, std::uint64_t clip_frame) noexcept;
    };
}
```

- [ ] **Step 1: Write the failing tests.**

```cpp
TEST(Unit_Dsp, TrackTimelineSortsItsClipsByStartFrame)
TEST(Unit_Dsp, TrackTimelineRefusesTwoOverlappingClipsAndNamesThem)
// EXPECT_THROW, and the what() string contains both start frames.
TEST(Unit_Dsp, TrackTimelineRefusesMoreThanSixteenClips)
TEST(Unit_Dsp, TrackTimelineFindsTheFirstClipPastAFrame)
TEST(Unit_Dsp, TrackTimelineRetainsEachDistinctTakeOnce)
// two clips off one take: library.reference_count(take) is 1 after construction, 0 after
// destruction.
TEST(Unit_Dsp, TrackTimelineFadesFiveMillisecondsAtEachEdge)
// edge_gain is 0 at clip_frame 0, 1 at kFadeFrames, 1 in the middle, 0 at length_frames.
TEST(Unit_Dsp, TrackTimelineFadesAShortClipWithoutOverlappingItsOwnRamps)
// a clip of 100 frames, shorter than 2 * kFadeFrames: the gain never exceeds 1 and is
// symmetric about the midpoint.
```

- [ ] **Step 2: Confirm they fail**; paste the error.
- [ ] **Step 3: Implement.** A `std::array<Clip, kMaxClips>` and a count, filled from the sorted
      input. The constructor sorts a local copy, checks each neighbour pair for overlap and
      throws `std::runtime_error` naming both start frames, then calls `library.retain` once per
      distinct `Take*`. The destructor calls `library.release` for the same set. `edge_gain` is
      a linear ramp over `std::min(kFadeFrames, clip.length_frames / 2)` at each end, so a short
      clip fades in and out without its ramps crossing.
- [ ] **Step 4: Syntax-check**, paste. **Step 5: Commit**
      `DT2: TrackTimeline holds one lane's clips, borrowed from the TakeLibrary`.

**Acceptance:** two clips off one take retain it once, release it once, and a lane whose clips
overlap is refused with both start frames in the message.

---

### Task 8: `TimelineScale`

**Waits on:** Task 1.

**Files:**
- Create: `apps/host_gui/ui/TimelineScale.hpp`, `apps/host_gui/ui/TimelineScale.cpp`
- Test: `tests/functional/unit/test_timeline_scale.cpp`
- Controller adds the source and the test to CMake.

**Interfaces:**
- Consumes: `MusicalClock::position_of`, `MusicalClock::frame_of` from Task 1.
- Produces:

```cpp
namespace SushiDSP::ui
{
    enum class SnapMode : std::uint32_t { None = 0u, Beat = 1u, Bar = 2u };

    /// @brief The one conversion between timeline frames and canvas pixels. The ruler, a clip's
    /// rectangle and a drop's landing frame all use it, so a clip is never drawn one pixel off
    /// the bar it plays on. Spec §6.
    class TimelineScale
    {
    public:
        void   set_pixels_per_frame(double pixels_per_frame);
        double pixels_per_frame() const;
        void   set_scroll_frames(std::uint64_t frames);
        std::uint64_t scroll_frames() const;

        float         x_of(std::uint64_t frame) const;
        std::uint64_t frame_of_x(float x) const;

        /// @brief Rounds a frame to the nearest bar, beat or nothing.
        std::uint64_t snap(std::uint64_t frame, SnapMode mode, const MusicalClock& clock) const;
    };
}
```

- [ ] **Step 1: Write the failing tests.**

```cpp
TEST(Unit_Dsp, TimelineScaleRoundTripsAFrameThroughAPixel)
TEST(Unit_Dsp, TimelineScalePutsTheScrollOriginAtXZero)
TEST(Unit_Dsp, TimelineScaleSnapsToTheNearestBar)
// 120 BPM 4/4 48 kHz: frame 100000 snaps to frame_of(2, 1) = 96000, frame 110000 to 192000.
TEST(Unit_Dsp, TimelineScaleSnapsToTheNearestBeat)
TEST(Unit_Dsp, TimelineScaleSnapModeNoneReturnsTheFrameUnchanged)
TEST(Unit_Dsp, TimelineScaleDoesNotUnderflowBeforeFrameZero)
// frame_of_x of a negative x is 0, never a wrapped uint64.
```

- [ ] **Step 2: Confirm they fail**; paste the error.
- [ ] **Step 3: Implement.** `x_of` is `(frame - scroll_frames_) * pixels_per_frame_`;
      `frame_of_x` is the inverse, clamped at zero. `snap` reads `clock.position_of(frame)`,
      takes the bar or beat start and the next one, and returns whichever is nearer.
- [ ] **Step 4: Syntax-check**, paste. **Step 5: Commit**
      `DT3: TimelineScale converts frames to pixels and snaps to the grid`.

**Acceptance:** at 120 BPM, 4/4, 48 kHz with bar snap, frame 100000 snaps to 96000 and frame
110000 to 192000.

---

## Wave 3 (one task, serial)

### Task 9: `TrackPlayer` over timelines, at a speed

**Waits on:** Tasks 3, 4, 7.

**Files:**
- Modify: `modules/capture/include/SushiDSP/capture/TrackPlayer.hpp`,
  `modules/capture/src/TrackPlayer.cpp`, `tests/functional/unit/test_track_player.cpp`,
  `tests/functional/unit/test_rt_safety.cpp`

**Interfaces:**
- Consumes: `TrackTimeline`, `Clip`, `TakeLibrary`, `VariRateReader`, `Transport::speed_q32`.
- Produces:

```cpp
    /// @brief The library every lane's clips borrow from. Control thread.
    TakeLibrary& takes() noexcept;

    /// @brief Replaces one lane's clips. Control thread; publishes through the lane's slot.
    void set_timeline(std::size_t track, std::unique_ptr<TrackTimeline> timeline);

    /// @brief Frees what the audio thread has moved past. Reclaims every lane slot FIRST, then
    /// collects the library: the other order would free a take a retired timeline still names.
    /// Spec §5. Control thread.
    void reclaim();
```

Each `Track`'s `RtSwapSlot<Take> take` becomes `RtSwapSlot<TrackTimeline> timeline`, and each
gains a `VariRateReader reader`.

- [ ] **Step 1: Write the failing tests** in `test_track_player.cpp`, keeping every existing case
      in the file passing by expressing an old single-take track as a one-clip timeline at frame 0:

```cpp
TEST(Unit_Dsp, TrackPlayerPlaysAClipAtItsStartFrame)
// a unit impulse take placed at frames 0, 12345 and 1000000: the output impulse is at that frame.
TEST(Unit_Dsp, TrackPlayerPlaysTwoClipsOnOneLane)
TEST(Unit_Dsp, TrackPlayerIsSilentBetweenClips)
TEST(Unit_Dsp, TrackPlayerFadesAClipEdge)
// a clip cut mid-waveform starts at zero and reaches full level after kFadeFrames.
TEST(Unit_Dsp, TrackPlayerAtUnitySpeedReproducesItsTakeBitForBit)
// EXPECT_EQ on the float bits, not EXPECT_NEAR: the converter must be bypassed, not run at one.
TEST(Unit_Dsp, TrackPlayerAtHalfSpeedDropsAnOctave)
TEST(Unit_Dsp, TrackPlayerReclaimsLanesBeforeCollectingTheLibrary)
// publish a timeline, publish an empty one over it, call reclaim() with the audio thread's
// watermark never advanced: the take is still alive, because nothing has moved past.
TEST(Unit_Dsp, TrackPlayerFreesATakeOnceBothLanesUsingItAreCleared)
```

- [ ] **Step 2: Confirm they fail**; paste the error.
- [ ] **Step 3: Implement.** `process` acquires each lane's timeline with
      `acquire_for_audio_thread()`, reads `transport.speed_q32()` once, and walks from
      `first_clip_at_or_after(block_start)` to the first clip whose start is past the block end.
      For each clip inside the block it computes the take-relative position, applies
      `TrackTimeline::edge_gain`, and mixes with the existing gain, pan, mute and solo. At unity
      speed it copies the take's samples directly, exactly as `mix_run` does today; at any other
      speed it goes through the lane's `VariRateReader`. A speed change, a seek or a move to the
      next clip calls `reader.reset()`. `set_timeline` publishes; `reclaim()` calls every lane's
      `RtSwapSlot::reclaim()` and only then `takes_.collect()`, with a comment stating that order
      as an invariant.
- [ ] **Step 4: RT safety.** Extend `TrackPlayerProcessPerformsNoHeapAllocation` to eight lanes of
      16 clips each at speeds 1.0 and 0.5, and add
      `TEST(Unit_Dsp, TrackPlayerTimelineSwapPerformsNoHeapAllocationOnTheAudioThread)`.
- [ ] **Step 5: Syntax-check**, paste. **Step 6: Commit**
      `DT2: TrackPlayer mixes clip timelines at the transport's speed`.

**Acceptance:** a unit impulse take placed at three different frames lands on those output frames
exactly, at unity speed, bit for bit.

---

## Wave 4 (one task, serial)

### Task 10: `RigController` places, aligns and saves clips

**Waits on:** Task 9.

**Files:**
- Modify: `apps/host_gui/RigController.hpp`, `apps/host_gui/RigController.cpp`,
  `tests/functional/unit/test_rig_controller.cpp`
- Modify: `modules/capture/include/SushiDSP/capture/RecordingBus.hpp`,
  `modules/capture/src/RecordingBus.cpp` (the armed transport position beside `block_index()`)

**Interfaces:**
- Consumes: `TrackPlayer::takes`, `TrackPlayer::set_timeline`, `TrackTimeline`, `MusicalClock`,
  `Metronome`, `Transport::set_speed`.
- Produces:

```cpp
    /// @brief Loads path and places it on track as a clip beginning at start_frame.
    /// @return False and fills error_out when the file will not parse or the lane is full.
    bool place_clip(std::size_t track, const std::string& path, std::uint64_t start_frame,
                    std::string& error_out);

    /// @brief Removes one clip from a lane and republishes it.
    bool remove_clip(std::size_t track, std::size_t clip_index, std::string& error_out);

    /// @brief Moves one clip's start frame and republishes its lane.
    bool move_clip(std::size_t track, std::size_t clip_index, std::uint64_t start_frame,
                   std::string& error_out);

    MusicalClock& musical_clock() noexcept;
    Metronome&    metronome() noexcept;
```

`RecordingBus` gains:

```cpp
    /// @brief The transport frame arm() stamped, so a stopped recording becomes a clip there.
    std::uint64_t armed_frame() const noexcept;
```

and `arm(folder, take_name, std::uint64_t transport_frame)`.

- [ ] **Step 1: Write the failing tests.**

```cpp
TEST(Unit_Dsp, RigControllerPlacesAClipAtTheFrameAsked)
TEST(Unit_Dsp, RigControllerRefusesAFileThatWillNotParseWithTheBrowsersMessage)
TEST(Unit_Dsp, RigControllerRefusesASeventeenthClipOnALane)
TEST(Unit_Dsp, RigControllerPlacesTwoClipsFromOneFileWithOneTakeInMemory)
// takes().size() is 1 after two place_clip() calls on the same path.
TEST(Unit_Dsp, RigControllerStopsARecordingIntoAClipAtTheArmedFrame)
TEST(Unit_Dsp, RigControllerRoundTripsATempoAndItsClipsThroughJson)
// save_to_json -> load -> save_to_json, string equality.
TEST(Unit_Dsp, RigControllerLoadsAPreDtPresetAsOneClipAtFrameZeroAtOneTwentyBpm)
TEST(Unit_Dsp, RigControllerDoesNotSaveThePlaybackSpeed)
// set_speed(0.4), save, load: speed() is 1.0. Spec §8.
```

- [ ] **Step 2: Confirm they fail**; paste the error.
- [ ] **Step 3: Implement.** `place_clip` loads through `Take::load`, inserts into
      `player().takes()`, rebuilds the lane's clip vector with the new entry appended, and
      publishes a fresh `TrackTimeline`; a throw from the constructor becomes `error_out` and the
      lane is left as it was. `save_to_json` gains the `tempo` object — `bpm`, `beats_per_bar`,
      `beat_unit`, `metronome_enabled`, `metronome_level` — and a `clips` array of `path`,
      `start_frame`, `take_offset`, `length_frames` per track row, keeping `loop` in frames. The
      loader defaults a missing `tempo` to 120, 4/4, metronome off, and a missing `clips` to the
      row's `file` as one clip at frame 0. Playback speed is written nowhere.
- [ ] **Step 4: Syntax-check**, paste. **Step 5: Commit**
      `DT5: RigController places clips, stamps a recording's start frame and saves the tempo`.

**Acceptance:** a session with a tempo, a meter, the metronome on and four clips round-trips
through `save_to_json` to an identical string, and playback speed is absent from it.

---

## Wave 5 (two tasks, parallel, disjoint files)

### Task 11: `ArrangementView`

**Waits on:** Tasks 8, 10.

**Files:**
- Create: `apps/host_gui/ui/ArrangementView.hpp`, `apps/host_gui/ui/ArrangementView.cpp`
- Controller adds the source to CMake. `ui/TrackPanel.{hpp,cpp}` stays on disk untouched until
  Task 13 removes its last caller; do not edit or delete them here.

**Interfaces:**
- Consumes: `TimelineScale`, `SnapMode`, `RigController::place_clip`, `remove_clip`, `move_clip`,
  `musical_clock`, `player`.
- Produces:

```cpp
namespace SushiDSP::ui
{
    /// @brief Draws the ruler, the playhead, eight lane headers and their clip canvases, and
    /// accepts a TAKE_FILE_PATH drop. Replaces DrawTrackPanel. Spec §6.
    void DrawArrangementView(RigController& rig, TimelineScale& scale, SnapMode& snap,
                             std::string& arrangement_error);
}
```

- [ ] **Step 1: Implement the lane headers**, moving the name, level, pan, R/M/S and the re-amp
      button out of `ui/TrackPanel.cpp` unchanged in behaviour, into a fixed-width strip on the
      left of each lane.
- [ ] **Step 2: Implement the ruler and the playhead** above the canvas, sharing one horizontal
      scroll with the lanes: bar numbers from `MusicalClock::position_of`, a line per bar and a
      lighter line per beat, and the playhead at `TimelineScale::x_of(transport.position_frames())`.
- [ ] **Step 3: Implement the clip rectangles**, each drawn from `x_of(start_frame)` to
      `x_of(start_frame + length_frames)`, labelled with its file name and its start frame in
      bars and beats, draggable to a new frame through `move_clip` with `snap` applied, and
      removable from a right-click menu.
- [ ] **Step 4: Implement the drop target.** A lane's canvas calls
      `ImGui::AcceptDragDropPayload("TAKE_FILE_PATH")`, following the `"GEAR_DESCRIPTOR_ID"` idiom
      in `apps/host_gui/ui/ChainStrip.cpp:225`, and routes the payload through a single
      `route_file_drop(RigController&, const TimelineScale&, SnapMode, const std::string& path,
      ImVec2 canvas_position, std::size_t track, std::string& error_out)` that Task 13 also calls
      for an Explorer drop. Add the matching `ImGui::SetDragDropPayload("TAKE_FILE_PATH", ...)`
      drag source to `ui/FileBrowser.cpp`'s `.wav` entries — that file is this task's alone.
- [ ] **Step 5: Syntax-check**, paste. **Step 6: Commit**
      `DT3: ArrangementView draws the ruler, the lanes and their clips`.

**Acceptance:** the maintainer opens the GUI after Task 13's build, drags a `.wav` from the file
browser onto lane 3 at bar 9 with bar snap on, and the clip's `start_frame` reads
`frame_of(9, 1)`.

---

### Task 12: `DrawTransportBar` gains musical time

**Waits on:** Tasks 1, 4, 6, 10.

**Files:**
- Modify: `apps/host_gui/ui/TransportBar.hpp`, `apps/host_gui/ui/TransportBar.cpp`

**Interfaces:**
- Consumes: `MusicalClock`, `Transport::set_speed`, `Metronome::set_enabled`,
  `Metronome::set_level`, `RigController::musical_clock`, `RigController::metronome`.
- Produces: `DrawTransportBar`'s existing signature, unchanged.

- [ ] **Step 1: Add the bars-and-beats readout**, `bar · beat · sixteenth` from
      `MusicalClock::position_of`, beside the seconds readout the bar shows today.
- [ ] **Step 2: Add the tempo controls**: a BPM field, a meter field (`beats_per_bar` and
      `beat_unit`), and a TAP button that averages the last four taps and discards a gap over two
      seconds.
- [ ] **Step 3: Add the metronome toggle and its level slider**, side by side, per spec §10's
      eighth ruling — not a ninth lane.
- [ ] **Step 4: Add the speed slider**, 0.25 to 4.0, double-click back to 1.0, labelled so it
      says the pitch falls with it. Spec §5a.
- [ ] **Step 5: Type the loop range in bars** instead of seconds, converting through
      `MusicalClock::frame_of`; the stored range stays in frames.
- [ ] **Step 6: Syntax-check**, paste. **Step 7: Commit**
      `DT3: the transport bar reads bars, sets the tempo and slows playback down`.

**Acceptance:** the readout shows `1 · 1 · 1` at frame 0 and bar 100 beat 1 at frame 9504000 with
120 BPM, 4/4, 48 kHz.

---

## Wave 6 (one task, serial)

### Task 13: the host callback and the Explorer drop

**Waits on:** Tasks 11, 12.

**Files:**
- Modify: `apps/host_gui/main.cpp`
- Delete: `apps/host_gui/ui/TrackPanel.hpp`, `apps/host_gui/ui/TrackPanel.cpp` (the controller
  removes them from CMake in the same build)

- [ ] **Step 1: Call the metronome after `RecordingBus::write`.** In the audio callback at
      `apps/host_gui/main.cpp:344`, where the bus is fetched, the order becomes: run the rig, mix
      the player, write both recorder legs, and only then
      `rig_controller_ref.metronome().process(...)`. Put a one-line comment stating the invariant
      — the click is not in a recorded file — and cite spec §4 by path.
- [ ] **Step 2: Advance the transport once per block** as it does today; the speed lives inside
      `Transport::advance` and the callback learns nothing new.
- [ ] **Step 3: Swap `ui::DrawTrackPanel` for `ui::DrawArrangementView`** at
      `apps/host_gui/main.cpp:490`, holding the `TimelineScale` and the `SnapMode` as GUI-loop
      locals, and delete the two `TrackPanel` files.
- [ ] **Step 4: Install the GLFW drop callback.** After `glfwCreateWindow` at
      `apps/host_gui/main.cpp:125`, `glfwSetDropCallback(window, ...)` pushes each path and
      `glfwGetCursorPos`'s position into the `FileDropQueue` of Task 5. Drain it at the top of the
      GUI loop, resolve each screen position to a lane through the rectangles
      `DrawArrangementView` recorded on the previous frame, and call the same
      `route_file_drop` Task 11 wrote. One routing function, two entry points.
- [ ] **Step 5: Syntax-check**, paste. **Step 6: Commit**
      `DT4: the host clicks after recording and routes a dropped file to a lane`.

**Acceptance:** the maintainer drags the same `.wav` from Windows Explorer onto the same pixel as
Task 11's ImGui drop and gets the same clip on the same lane at the same frame.

---

## Wave 7 (one task, serial)

### Task 14: documentation

**Waits on:** Task 13.

**Files:**
- Modify: `docs/design/sushidsp.md` (a section for the timeline path and the DT roadmap rows),
  `modules/capture/README.md`, `docs/README.md`, `docs/reference/CHANGELOG.md`,
  `docs/reference/KNOWN_ISSUES.md`
- Create: `docs/reference/GLOSSARY.md` — `docs/reference/` holds no glossary today. Controller
  approves the new file's place in `docs/README.md`.

- [ ] **Step 1: Move the KNOWN_ISSUES entry.** `docs/reference/KNOWN_ISSUES.md:100`, "Spec §5's
      timeline alignment is not implemented", is deleted, and its closure becomes one CHANGELOG
      line. It closes only when the re-amp alignment case of the spec's §11 passes — a dry take
      recorded from bar 5 and re-amped matches the processed file it was recorded beside.
- [ ] **Step 2: One CHANGELOG line per task**, each the date, a past-tense verb, what changed and
      where in backticks, under 240 characters, no reason and no second sentence.
- [ ] **Step 3: Update `modules/capture/README.md`** with `MusicalClock`, `Metronome`,
      `TakeLibrary`, `TrackTimeline` and `VariRateReader`, and with the take-ownership rule: the
      audio thread borrows, `TakeLibrary` owns, `RtSwapSlot::reclaim()` makes the borrow safe.
- [ ] **Step 4: Add `bar`, `beat`, `clip`, `take` and `playback speed` to the glossary**, with
      playback speed saying the pitch follows it.
- [ ] **Step 5: Commit** `DT: document the arrangement view and close the alignment issue`.

**Acceptance:** every path and link cited in the changed documents resolves.

---

## Wave summary

| Wave | Tasks | Waits on | Files, disjoint per wave | Build the controller runs |
| --- | --- | --- | --- | --- |
| 1 | 1, 2, 3, 4, 5 | none | `MusicalClock.*`, `TakeLibrary.*`, `VariRateReader.*` + one `test_rt_safety.cpp` case, `Transport.*`, `FileDropQueue.*`; one new unit test file each | one build for all five, then `sd test` |
| 2 | 6, 7, 8 | 1 (tasks 6, 8), 2 (task 7) | `Metronome.*`, `TrackTimeline.*`, `TimelineScale.*`; one new unit test file each | one build for all three |
| 3 | 9 | 3, 4, 7 | `TrackPlayer.*`, `test_track_player.cpp`, `test_rt_safety.cpp` | build, then every golden unchanged |
| 4 | 10 | 9 | `RigController.*`, `RecordingBus.*`, `test_rig_controller.cpp` | build, then `sd test` |
| 5 | 11, 12 | 8, 10 (task 11); 1, 4, 6, 10 (task 12) | `ui/ArrangementView.*` + `ui/FileBrowser.cpp`; `ui/TransportBar.*` | one build for both |
| 6 | 13 | 11, 12 | `main.cpp`, `ui/TrackPanel.*` deleted | build, then the GUI opened by the maintainer |
| 7 | 14 | 13 | the six documents | build |

Task 5 has no dependency on anything and could run in any wave; it sits in wave 1 because wave 1
is the wave with room.

## Self-review

- **Spec coverage.** §3 → Task 1; §4 → Tasks 6 and 13; §5 → Tasks 2, 7 and 9; §5a → Tasks 3, 4, 9
  and 12; §6 → Tasks 8, 11 and 12; §7 → Tasks 5, 11 and 13; §8 → Task 10; §11 → each task's
  acceptance, plus the re-amp alignment case, which lands in Task 10 because only the host holds a
  recording, a rig and a timeline at once.
- **The four rulings.** Speed with pitch: Tasks 3, 4, 9, 12, and the `TrackPlayerAtUnitySpeed
  ReproducesItsTakeBitForBit` guard. `kMaxClips = 16`: Task 7's `TrackTimelineRefusesMoreThan
  SixteenClips` and Task 9's RT trap at eight lanes of sixteen. The click in no file: Task 13's
  step 1 and Task 6's placement outside the graph. No `shared_ptr`: Task 7's `static_assert`,
  Task 2's library, Task 9's reclaim order.
- **Names.** `MusicalClock`, `BarBeat`, `beat_step_q32`, `TakeLibrary`, `insert`, `retain`,
  `release`, `collect`, `VariRateReader`, `kMinSpeed`, `kMaxSpeed`, `speed_q32`, `Metronome`,
  `Clip`, `TrackTimeline`, `kMaxClips`, `edge_gain`, `first_clip_at_or_after`, `TimelineScale`,
  `SnapMode`, `x_of`, `frame_of_x`, `snap`, `FileDropQueue`, `place_clip`, `armed_frame`,
  `DrawArrangementView`, `route_file_drop` — each defined in the task that first produces it and
  spelled the same everywhere it is consumed.
- **The one number that is a judgement, not a derivation.** `kMinSpeed` 0.25 and `kMaxSpeed` 4.0
  sit inside what `Resampler` can do (`kMaximumRatio` is 16), but the range itself is a choice
  about what a guitarist wants. Narrowing it later is a one-line change in two constants and a
  slider bound.
