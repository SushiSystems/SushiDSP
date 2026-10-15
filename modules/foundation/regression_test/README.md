# regression_test

Internal module: the macro that writes a regression test. Its headers are never installed, and no
product links it.

The header includes `gtest.h`, so a test that includes it links GoogleTest itself.

## regression_test.hpp

Defines `SUSHIDSP_REGRESSION_TEST(suite, name)`, which expands to `TEST`. The name says the case
keeps a fixed defect fixed; the test build registers it like a `TEST`, with its file's size.
