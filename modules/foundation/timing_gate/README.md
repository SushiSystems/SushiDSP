# timing_gate

Internal module: the gate that skips a wall-clock bound when the test binary is not optimised. Its
headers are never installed, and no product links it.

The header includes `gtest.h`, so a test that includes it links GoogleTest itself.

## timing_gate.hpp

Defines `SUSHIDSP_REQUIRE_OPTIMISED_BUILD()`, which skips the calling test unless `NDEBUG` is
defined, and `SushiDSP::test::is_optimised_build()`, which reports the same condition. A wall-clock
budget is a Release figure: asserting it against an unoptimised binary compares two different
programs.
