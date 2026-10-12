# `applications/host_asio`: the live ASIO host

`sushidsp_host_asio` plays a guitar through the Tube Screamer and the JCM800 on an ASIO driver
until Enter is pressed. It takes the driver's name, `--input=L|R`, `--output=L|R|stereo` and an
optional cabinet impulse response; run with no arguments, it lists the registered drivers.

## `main.cpp`

The device is always opened with two inputs and two outputs, whatever
`--input` and `--output` say. A guitar wired to the right input (`--input=R`) and a stereo
output both then have a real ASIO channel to read from or write to.

The rig holds no `SineToneNode`. `tube_screamer_handle` has no upstream
edge, and `RigGraph` feeds a root node the device's own live input.

The amplifier does not host a cabinet. `sushidsp.cabinet` is a standalone node
that joins the chain only when an impulse response was named on the command line, so a run
without one ends at the amplifier, as it did before the cabinet became a node of its own.

The rig has one input channel and as many output channels as the device has, up to
`RigGraph::K_MAX_CHANNELS`. A mono amplifier chain is copied to both channels of a stereo device by
the graph itself. The channel selection decides only which physical ASIO channel feeds the rig and
which physical channels its output reaches.

The rig renders into `rig_output`, one block per rig channel, a buffer `main()` owns and sizes
before the device starts. Two pointer arrays name those blocks. The callback maps them onto the
device's channels and allocates nothing.

`main()` installs an `AsyncLogSink` over a `StderrLogSink` through a `ScopedLogSink`, which
flushes the sink on every return path. What `main()` prints with `printf` is its own output and
does not go through the logger.

## Dependencies

Links `SushiDSP::sushi_dsp_asio`, `SushiDSP::tubescreamer`, `SushiDSP::jcm800` and
`SushiDSP::cabinet`.
