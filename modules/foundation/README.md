# `modules/foundation`: the core library

The foundation tier holds the core DSP library, one module per area. The tier is the lowest
one: every product module in `modules/nodes/`, `modules/catalog/` and `modules/session/` depends
on it, and it depends on nothing above itself.

| Module | Holds |
| --- | --- |
| [`audio_format/`](audio_format/README.md) | Hardware sample formats, device channel selection |
| [`circuits/`](circuits/README.md) | Circuit blocks: tone stack, equaliser, feedback, clamp, tube stages, device models |
| [`core/`](core/README.md) | The node and device interfaces, `RigGraph`, the real-time slots, the buffer pool |
| [`device/`](device/README.md) | The ASIO and SDL2 audio devices |
| [`io/`](io/README.md) | Impulse responses and WAV files |
| [`log/`](log/README.md) | The logger, its sinks, the `{}` formatter and the `SD_LOG` macros |
| [`math/`](math/README.md) | Filters, FFT, convolution, resampling, oversampling, the scalar and nodal solvers |

Dependencies between the modules of this tier are listed in
[`cmake/SushiDSPLayers.cmake`](../../cmake/SushiDSPLayers.cmake). A module that depends on a
sibling the table does not list fails to configure.

The public headers of each module sit under its `include/SushiDSP/<module>/`, so a header keeps
its include path `<SushiDSP/<module>/<name>.hpp>`. The files under `source/` are private to the
module.
