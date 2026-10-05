# tests/common

Code every test executable shares.

| File | What it holds |
| --- | --- |
| `test_main.cpp` | The entry point of the test suite |
| `RoomFixtures.hpp` | The three reference rooms the room-acoustics acceptance tests measure against |
| `TimingGate.hpp` | The gate that skips a wall-clock bound when the binary is not optimised |

## test_main.cpp

`main` initialises GoogleTest and runs every registered case. SushiDSP is a portable DSP
library with no dependencies, so the tests exercise the real kernels and the graph directly on
the host. There are no mocks, and there is no device setup to perform here.
