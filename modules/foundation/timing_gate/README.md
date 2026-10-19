# timing_gate

Internal module: the gate that skips a wall-clock bound when the test binary is not optimised. Its
headers are never installed, and no product links it.

The header includes `gtest.h`, so a test that includes it links GoogleTest itself.

## timing_gate.hpp

Defines `SUSHIDSP_REQUIRE_OPTIMISED_BUILD()`, which skips the calling test unless `NDEBUG` is
defined and no sanitiser is on, and `SushiDSP::test::is_optimised_build()` and
`is_sanitised_build()`, which report the two conditions. A wall-clock
budget is a Release figure: asserting it against an unoptimised or sanitised binary compares two
different programs.
