# tests/foundation/log

## Log tests

`test_log_formatter.cpp`, `test_log_compile_time_cut.cpp`, `test_logger.cpp`,
`test_log_line_queue.cpp` and `test_async_log_sink.cpp` cover the logger of
`modules/foundation/log/include/SushiDSP/foundation/log/`. The compile-time cut has a file of its own because it sets
`SD_LOG_MIN_LEVEL` before including `log.hpp`. `test_track_recorder_logging.cpp`,
`test_room_node_logging.cpp` and `test_directory_listing_logging.cpp` each provoke one
handled failure and read the single line it logs through a capturing sink.
