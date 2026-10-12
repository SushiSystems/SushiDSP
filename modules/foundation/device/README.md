# `modules/foundation/device`

The ASIO and SDL2 audio devices. Their backend targets are built by this module's CMake file.

## Owns

- `modules/foundation/device/include/SushiDSP/device/asio_audio_device.hpp`
- `modules/foundation/device/include/SushiDSP/device/sdl2_audio_device.hpp`

## Depends on

`core` and `audio_format` (same tier).

## Notes cited from the source

A comment in this module's files states one invariant and names this file. The reasoning
behind each invariant follows, under the name of the file that cites it.

### `modules/foundation/device/include/SushiDSP/device/asio_audio_device.hpp`

`active_instance_` is the target of the trampolines ASIO's free-function callbacks go through. The
SDK assumes one open driver process-wide. A second `AsioAudioDevice` open at the same time is a
programming error that the class documents and does not guard at run time, which is how this
codebase treats constructor-level preconditions elsewhere.

### `modules/foundation/device/source/asio_audio_device.cpp`

`asiodrivers.cpp` defines `loadAsioDriver` and declares it in no header. The SDK's own host sample
declares it locally too, so a local `extern` is the sanctioned way to reach it.

Once `loadAsioDriver` has succeeded, any failure in the constructor must unload the driver. If it
does not, the process-wide SDK state stays wedged and the next open of any driver fails.

`ASIOInit` wants a window handle in `sysRef`. Console and library hosts have no application
window; the desktop window is the conventional stand-in, and drivers that ignore `sysRef` are
unaffected.

`ASIOCanSampleRate` is the only non-destructive way to ask whether a rate is available. The
constructor sets the rate only when the driver confirms it, so an unsupported request leaves the
hardware's current clock alone and does not fail the whole open.

Every channel in one direction must share a container format. The device keeps a single format
per direction and no per-channel table, so it rejects a mixed-format driver where a table-less
read would misinterpret the samples.

`asio_message` answers the driver's selectors as follows. Resync, latency-change and overload
notices are acknowledged so the driver does not keep re-asking; the device does not act on them,
because reopening the driver is the host application's call. A reset request gets 0. Answering 1
would claim the device tears down and re-creates its buffers in response, which it does not do. A
real reset handler belongs at the host layer that owns rebuilding the format and the buffers, and
that layer does not exist yet. So 0 is the truthful "not handled" answer, and the reset selector
is left out of the `kAsioSelectorSupported` list for the same reason. Time info is declined, which
keeps the driver on the plain `bufferSwitch` path; that path is all the device needs.
`buffer_switch_time_info` is reachable only if a driver ignores that answer, and it forwards the
block so that nothing is dropped.

`sample_rate_did_change` is empty. `format()` reports the rate negotiated at construction, and a
mid-stream clock change is a driver-level event this device does not chase.

### `modules/foundation/device/source/sdl2_audio_device.cpp`

Once stopped, the device must not keep a reference to whatever the caller's callback captured.
`SDL_PauseAudioDevice` waits for any in-flight callback to finish, so `stop` clears the callback
right after it.
