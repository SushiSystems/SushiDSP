# `tests/foundation/log`

Tests of SushiDSP's front over SushiLog: the `SD_LOG` macros, and the `sushidsp`
project. `tests/main.cpp` calls `SushiLog::set_log_synchronous(true)` before the first test,
so a record reaches its sink before the call that logs it returns.

`test_log_level_setting_reader.cpp` tests the reading of a project's level variable through an
`InMemoryEnvironment`, and its hand-over to SushiLog for a project of the test's own.

`test_in_memory_environment.cpp` and `test_process_environment.cpp` test the two `EnvironmentReader` adapters; the
process one is `Medium_` because it sets a process variable. `compile_environment_reader.cpp` checks
that the port is abstract.
