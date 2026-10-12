# `applications/host`: the device sanity check

`sushidsp_host` opens the default SDL2 output at 48 kHz with 256-frame blocks, plays a 440 Hz
sine through the Tube Screamer into the JCM800 for three seconds, and exits. It exists to prove
that a device opens and a compiled `RigGraph` renders into it. Nothing links it.

`SineToneNode` is the tone's source. It is a diagnostic node with no `NodeDescriptor`, and it is
not part of the public SushiDSP API.

## `main.cpp`

The pedal and amplifier settings are written into `main()` and set once, before the graph
compiles. This host has no rig editor, so nothing could drive them while the tone plays.

The rig stops at the amplifier. `sushidsp.cabinet` is a separate node and this demo does not
wire one in, so what reaches the device is the amplifier's dry signal. `applications/host_asio/main.cpp`
builds a rig that adds a cabinet.

`main()` installs an `AsyncLogSink` over a `StderrLogSink` through a `ScopedLogSink`, which
flushes the sink on every return path. What `main()` prints with `printf` is its own output and
does not go through the logger.

## Dependencies

Links `SushiDSP::sushi_dsp_sdl2`, `SushiDSP::tubescreamer` and `SushiDSP::jcm800`.
