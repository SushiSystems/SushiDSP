# tests

The test programs of SushiDSP. Each unit's tests sit at the path that mirrors the unit's header, and
shared test code is an internal module under `modules/`; see `repository-layout`, TESTS.md.

## main.cpp

`main` initialises GoogleTest and runs every registered case. SushiDSP is a portable DSP
library with no dependencies, so the tests exercise the real kernels and the graph directly on
the host. There are no mocks, and there is no device setup to perform here.
