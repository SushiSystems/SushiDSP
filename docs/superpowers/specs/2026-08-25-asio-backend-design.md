# ASIO Audio Backend — Design Spec

**Status:** Approved by user, ready for implementation planning.

## Goal

Add a duplex, low-latency `IAudioDevice` backend built on the Steinberg ASIO SDK, so
SushiDSP can capture a real guitar signal (via a Roland Rubix22 or any ASIO-capable
interface) and route it live through a rig (e.g. TubeScreamer -> Jcm800) to real output
hardware — not just process a synthetic tone or offline file. This is the user's stated
motivation for the whole feature: playing guitar through the amp model in real time.

## Context

SushiDSP's current `IAudioDevice` seam (`include/SushiDSP/core/IAudioDevice.hpp`) and its
only implementation (`Sdl2AudioDevice`) are **output-only**: `DeviceCallback` fills output
buffers and has no input parameter, and `apps/host/main.cpp`'s rig source is always a
synthetic `SineToneNode`. There is no live audio-input path anywhere in the codebase today.
This spec closes that gap as part of adding ASIO, rather than deferring it — a backend that
can't capture input would not satisfy the feature's actual purpose.

The Steinberg ASIO SDK is dual-licensed (proprietary Steinberg license, requiring a signed
agreement before publishing; or GPLv3). It cannot be committed to this Apache-2.0 repository
or fetched automatically. The user has manually placed SDK 2.3 at `third_party/asio/`, now
excluded via `.gitignore`. The user has accepted the licensing tradeoffs (see "Licensing"
below) and understands the ASIO module may need to be removed or re-licensed before or at
public release.

## Architecture

Three new pieces, all additive — no change to any existing DSP node, and the existing
`Sdl2AudioDevice`/`apps/host` demo path keeps working unmodified in its own signal path
except for the interface's added `inputs` parameter (which it ignores, passing silence).

### 1. `IAudioDevice` becomes duplex

`include/SushiDSP/core/IAudioDevice.hpp`:

```cpp
struct DeviceFormat
{
    double        sample_rate;
    std::uint32_t block_size;
    std::uint32_t input_channels;
    std::uint32_t output_channels;
};

using DeviceCallback = std::function<void(const float* const* inputs, float* const* outputs, std::uint32_t frames)>;
```

`inputs` may be `nullptr` (or point to channels that are all-silence) for an output-only
device such as `Sdl2AudioDevice`. `input_channels == 0` signals "this device has no capture
path" to callers.

`ProcessContext` (`IAudioNode.hpp`) already has both `inputs` and `outputs`, so `RigGraph`
and every `IAudioNode` need no change — the device callback simply forwards its own
`inputs`/`outputs`/`frames` straight into a `ProcessContext` each block.

`Sdl2AudioDevice` is updated only enough to satisfy the new signature (constructs with
`input_channels = 0`, passes `nullptr` for `inputs`). It stays output-only; SDL2 capture
is out of scope for this feature (the user's real-input need is served by ASIO).

### 2. `AsioAudioDevice`

New class, `include/SushiDSP/device/AsioAudioDevice.hpp` / `src/device/AsioAudioDevice.cpp`,
mirroring `Sdl2AudioDevice`'s role: the only class that touches ASIO SDK types.

```cpp
class AsioAudioDevice : public IAudioDevice
{
public:
    /// @brief Lists the ASIO driver names currently registered on this machine.
    static std::vector<std::string> enumerate_drivers();

    /// @param driver_name An exact name from enumerate_drivers().
    /// @param requested_format sample_rate/block_size are requests; the opened device's
    ///        actual negotiated format is returned by format() and may differ.
    AsioAudioDevice(const std::string& driver_name, const DeviceFormat& requested_format);
    ~AsioAudioDevice() override;

    DeviceFormat format() const override;
    void start(DeviceCallback callback) override;
    void stop() override;

private:
    // ... ASIOBufferInfo[], sample-format-conversion scratch buffers, etc.
};
```

**Single-instance constraint.** The classic ASIO SDK's callbacks
(`bufferSwitch`/`bufferSwitchTimeInfo`/`sampleRateDidChange`/`asioMessage`) are free
functions, not member functions, and the SDK assumes one open driver process-wide. Because
`IAudioDevice` already models "one thing talking to one physical device," `AsioAudioDevice`
uses a single static instance pointer as the trampoline target for these callbacks; a second
concurrently-`start()`ed `AsioAudioDevice` is a programming error (documented, not
guarded — matches this codebase's existing precedent of documenting rather than
runtime-checking constructor preconditions elsewhere).

**Sample format conversion.** ASIO hardware reports its native sample type via
`ASIOGetChannelInfo` (commonly `ASIOSTInt32LSB` or `ASIOSTFloat32LSB`, hardware-dependent).
`AsioAudioDevice` converts every block to/from this codebase's `float` in `[-1, 1]` using the
SDK's own `ASIOConvertSamples` helper. This conversion is a pure, allocation-free function
extractable and unit-testable in isolation (see Testing).

**Channel mapping.** The constructor takes the *driver's* format request but always presents
exactly `input_channels`/`output_channels` logical channels through `IAudioDevice`, mapped to
ASIO hardware channel indices `0..N-1` (i.e., "first N hardware channels"). No UI/config for
arbitrary channel selection in this pass — YAGNI'd per the approved design discussion;
add if a real need appears.

### 3. Build integration

Root `CMakeLists.txt`:

```cmake
set(ASIO_SDK_DIR "${CMAKE_SOURCE_DIR}/third_party/asio" CACHE PATH "Path to the Steinberg ASIO SDK")
option(SUSHIDSP_BUILD_ASIO "Build the ASIO device backend" ON)

if(SUSHIDSP_BUILD_ASIO AND WIN32 AND EXISTS "${ASIO_SDK_DIR}/common/asio.h")
    add_library(sushi_dsp_asio STATIC
        src/device/AsioAudioDevice.cpp
        ${ASIO_SDK_DIR}/common/asio.cpp
        ${ASIO_SDK_DIR}/common/asiodrvr.cpp
        ${ASIO_SDK_DIR}/common/combase.cpp
        ${ASIO_SDK_DIR}/common/register.cpp
        ${ASIO_SDK_DIR}/host/asiodrivers.cpp
        ${ASIO_SDK_DIR}/host/pc/asiolist.cpp
        ${ASIO_SDK_DIR}/host/ASIOConvertSamples.cpp
    )
    target_include_directories(sushi_dsp_asio PRIVATE
        ${ASIO_SDK_DIR}/common ${ASIO_SDK_DIR}/host ${ASIO_SDK_DIR}/host/pc)
    target_link_libraries(sushi_dsp_asio PUBLIC SushiDSP::sushi_dsp PRIVATE ole32 oleaut32)
    add_library(SushiDSP::asio ALIAS sushi_dsp_asio)
else()
    message(STATUS "ASIO SDK not found at ${ASIO_SDK_DIR} (or not on Windows) - sushi_dsp_asio target skipped")
endif()
```

Follows the exact pattern `SUSHIDSP_BUILD_SDL2` already established: absent dependency (or
wrong platform) silently skips the target rather than failing the configure step, so Linux CI
and every DSP-core-only workflow are unaffected. `.github/workflows/ci.yml` needs no change —
it never sets `ASIO_SDK_DIR`, so the target is always skipped there, which is correct (ASIO
cannot be built in CI without the SDK present, and it's Windows-only besides).

### 4. `apps/host_asio` — the real playable application

A new, separate executable alongside `apps/host` (which stays SDL2/synthetic-tone, for
demo/CI-friendly purposes). `apps/host_asio/main.cpp` lists available drivers, opens one
(named via CLI arg), builds the same kind of rig as `apps/host` but with **no synthetic
source node** — the device's own `inputs` becomes the rig's live input — and runs until the
user stops it. This is the actual "play guitar through SushiDSP" program.

Like `apps/host`, `apps/host_asio` is built but never *run* by an agent (it opens real audio
hardware); the user runs and auditions it themselves.

## Licensing

Recorded here per this project's honesty principle, not as legal advice:

- The ASIO SDK is used under its dual license (proprietary-with-signed-agreement, or GPLv3);
  the user has chosen to proceed informally for now and accepts they may need to remove or
  relicense the ASIO integration before/at public release.
- `third_party/asio/` is `.gitignore`d — the SDK itself never enters this Apache-2.0
  repository's history.
- The `AsioAudioDevice`/`sushi_dsp_asio` module is architecturally isolated (one class, one
  optional CMake target, behind the same `IAudioDevice` seam as every other backend) so it
  can be deleted or re-licensed independently of the rest of SushiDSP without touching any
  DSP code.

## Testing

- `AsioAudioDevice` itself cannot be exercised by the automated suite (no ASIO driver exists
  in CI, and the class is never compiled in CI at all per the CMake guard above) — this
  matches the already-established, documented precedent for `Sdl2AudioDevice` (D1's plan:
  "opens real audio hardware," no test coverage of its own).
- The sample-format-conversion logic is extracted as a small, pure, allocation-free function
  (or a thin class with no ASIO-SDK-type members) so it *can* be unit-tested on any platform:
  round-trip Int32LSB/Float32LSB <-> `float[-1,1]` conversion with known values, independent
  of whether the rest of the ASIO target builds.
- `DeviceFormat`'s new two-channel-count shape and `Sdl2AudioDevice`'s updated (silent-input)
  behavior get regular unit coverage alongside existing device-format tests.

## Deferred / explicitly out of scope

- SDL2 capture / duplex SDL2 (only ASIO is duplex in this pass).
- Arbitrary ASIO channel selection/routing UI (fixed to "first N channels" for now).
- ASIO Control Panel invocation (`ASIOControlPanel()`) — drivers are configured via their own
  vendor control panel outside SushiDSP for now.
- Sample-rate/buffer-size negotiation UI — the app requests a format and reports whatever the
  driver actually opened with; no retry/fallback logic beyond that.
