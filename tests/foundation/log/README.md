# `tests/foundation/log`

Tests of SushiDSP's front over SushiLog: the `SD_LOG` macros, the `sushidsp` project and
`LogCapture`. `tests/main.cpp` calls `SushiLog::set_log_synchronous(true)` before the first test,
so a record reaches its sink before the call that logs it returns.