# tests/foundation/log

## Log tests

One test file per header of `modules/foundation/log/include/SushiDSP/foundation/log/`.
`test_log.cpp` defines `SD_ENABLE_LOGGING` as 0 before including `log.hpp`, so it holds the
cases of the compile-time cut and nothing else. `test_logger.cpp` sets an environment variable of
its own, `SD_LOGGER_TEST_LEVEL`, for the cases that read levels from the environment, and leaves
`SD_LOG_LEVEL` alone. The test binary delivers every record synchronously: `tests/main.cpp`
calls `set_log_synchronous(true)` before the first test. The logging cases of
the track recorder, the room node and the directory listing each provoke one handled failure
and read the single line it logs through a capturing sink.
