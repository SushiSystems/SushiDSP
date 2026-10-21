# room_scenes

Internal module: the reference rooms and the port table that the room node's tests and the benchmark
share. Its headers are never installed, and no product links it.

It depends on `room`, the same-tier edge listed in `cmake/SushiDSPLayers.cmake`.

## Entry points

| File | What it holds |
| --- | --- |
| `room_fixtures.hpp` | The three reference rooms the room-acoustics acceptance tests measure against |
| `room_ports.hpp` | The port table and the spread arrangement the room node's tests and benchmarks share |

## Designs

<!-- designs:begin -->
- [ROOM](../../../docs/design/ROOM/README.md): The room a cabinet is heard in: the shell, its finishes and its furnishings, and the node that renders them.
<!-- designs:end -->
