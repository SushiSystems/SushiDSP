# `modules/foundation/log`

The logger, its sinks, the `{}` formatter and the `SD_LOG` macros.

## Owns

- `modules/foundation/log/include/SushiDSP/foundation/log/async_log_sink.hpp`
- `modules/foundation/log/include/SushiDSP/foundation/log/capturing_log_sink.hpp`
- `modules/foundation/log/include/SushiDSP/foundation/log/default_logger.hpp`
- `modules/foundation/log/include/SushiDSP/foundation/log/log_sink.hpp`
- `modules/foundation/log/include/SushiDSP/foundation/log/log.hpp`
- `modules/foundation/log/include/SushiDSP/foundation/log/log_argument.hpp`
- `modules/foundation/log/include/SushiDSP/foundation/log/log_category.hpp`
- `modules/foundation/log/include/SushiDSP/foundation/log/log_formatter.hpp`
- `modules/foundation/log/include/SushiDSP/foundation/log/log_level.hpp`
- `modules/foundation/log/include/SushiDSP/foundation/log/log_line_queue.hpp`
- `modules/foundation/log/include/SushiDSP/foundation/log/log_record.hpp`
- `modules/foundation/log/include/SushiDSP/foundation/log/logger.hpp`
- `modules/foundation/log/include/SushiDSP/foundation/log/scoped_log_sink.hpp`
- `modules/foundation/log/include/SushiDSP/foundation/log/stderr_log_sink.hpp`
- `modules/foundation/log/include/SushiDSP/foundation/log/log_log_category.hpp`

## Depends on

Nothing from the foundation tier.

## Notes cited from the source

A comment in this module's files states one invariant and names this file. The reasoning
behind each invariant follows, under the name of the file that cites it.

### `log/`

One brick per header. `LogLevel`, `LogCategory` and `LogRecord` are the values a line is made
of. `LogArgument` prints one value and `format_log_message` walks the `{}` markers. `LogSink` is the
interface a line leaves through, and `StderrLogSink`, `AsyncLogSink` and `CapturingLogSink`
implement it. `LogLineQueue` is the bounded queue inside `AsyncLogSink`. `Logger` owns the
current sink and the runtime minimum level, `default_logger()` returns the logger of the process,
`ScopedLogSink` installs a sink for a scope, and `Log.hpp` defines the macros.

A call reads `SD_LOG_WARN(K_ROOM_LOG_CATEGORY, "format {}", value)`. Each module that logs
declares its one category in `<module>_log_category.hpp` beside its own headers. The formatter
takes `{}` and nothing else: a value that needs a width, a precision or hex is turned into a
string before the call. It prints a bool, a `char`, any integer, a `float` or `double` with six
significant digits, a C string, a `std::string` and a `std::string_view`. A message longer than
`K_MAX_LOG_MESSAGE_LENGTH`, 1024 characters, is cut there.

A level below `SD_LOG_MIN_LEVEL` compiles to `((void)0)` and its arguments are never evaluated.
The constant is `SD_LOG_LEVEL_INFO` when `NDEBUG` is defined and `SD_LOG_LEVEL_TRACE` otherwise,
and a translation unit may define it before including `Log.hpp`. A level that is compiled in but
below `Logger::minimum_level()` is skipped at run time, again without evaluating its arguments.
The logger of the process starts at `LogLevel::Info` with a `StderrLogSink`, so a line logged
before an application installs a sink still reaches stderr.

### `modules/foundation/log/include/SushiDSP/foundation/log/logger.hpp` and `modules/foundation/log/include/SushiDSP/foundation/log/async_log_sink.hpp`

The audio thread never logs. `Logger::log` formats on the caller's stack and takes a mutex for
the length of one `shared_ptr` copy, which is cheap and still not something a real-time callback
may do. A failure on the audio thread is counted in an atomic, the way
`TrackRecorder::dropped_blocks()` counts a refused block, and the control thread logs the count.

An application installs an `AsyncLogSink` over a `StderrLogSink` when it starts, through a
`ScopedLogSink` that flushes it on the way out of `main`. After its constructor has allocated
the queue and started the worker, `AsyncLogSink::write` copies the line into a slot of the queue
and returns. It takes no lock and allocates nothing. When the queue is full the line is dropped
and counted, and the worker writes one `WARN` under the `log` category that says how many lines
were lost. The wake-up `write` sends takes no lock either, so it can arrive just before the
worker goes to sleep and be missed; the worker's wait ends after 50 ms at the latest and the
line is written then. `flush()` returns once every line queued before the call has been written
and the sink below has been flushed.

Tests install a `CapturingLogSink`, which stores each line before `write` returns, so the order
they read is the order the code logged in.
