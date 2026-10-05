# `apps/host_asio`: the live ASIO host

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

The rig has one input and one output channel. The channel selection decides
only which physical ASIO channel feeds it and which physical channels its output reaches.

The rig renders into `mono_output`, a buffer `main()` owns and sizes
before the device starts. The callback maps that buffer onto the device's channels and
allocates nothing.

## Dependencies

Links `SushiDSP::sushi_dsp_asio`, `SushiDSP::tubescreamer`, `SushiDSP::jcm800` and
`SushiDSP::cabinet`.
