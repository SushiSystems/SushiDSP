# `modules/foundation/log`

The logger of the process, its per-category levels, its sinks, the `{}` formatter and the
`SD_LOG` macros.

## Owns

- `modules/foundation/log/include/SushiDSP/foundation/log/async_log_sink.hpp`
- `modules/foundation/log/include/SushiDSP/foundation/log/capturing_log_sink.hpp`
- `modules/foundation/log/include/SushiDSP/foundation/log/default_logger.hpp`
- `modules/foundation/log/include/SushiDSP/foundation/log/delegate.hpp`
- `modules/foundation/log/include/SushiDSP/foundation/log/log.hpp`
- `modules/foundation/log/include/SushiDSP/foundation/log/log_argument.hpp`
- `modules/foundation/log/include/SushiDSP/foundation/log/log_category.hpp`
- `modules/foundation/log/include/SushiDSP/foundation/log/log_delivery.hpp`
- `modules/foundation/log/include/SushiDSP/foundation/log/log_formatter.hpp`
- `modules/foundation/log/include/SushiDSP/foundation/log/log_level.hpp`
- `modules/foundation/log/include/SushiDSP/foundation/log/log_level_setting.hpp`
- `modules/foundation/log/include/SushiDSP/foundation/log/log_level_table.hpp`
- `modules/foundation/log/include/SushiDSP/foundation/log/log_line.hpp`
- `modules/foundation/log/include/SushiDSP/foundation/log/log_line_queue.hpp`
- `modules/foundation/log/include/SushiDSP/foundation/log/log_project.hpp`
- `modules/foundation/log/include/SushiDSP/foundation/log/log_log_category.hpp`
- `modules/foundation/log/include/SushiDSP/foundation/log/log_record.hpp`
- `modules/foundation/log/include/SushiDSP/foundation/log/log_record_sink.hpp`
- `modules/foundation/log/include/SushiDSP/foundation/log/log_sink.hpp`
- `modules/foundation/log/include/SushiDSP/foundation/log/log_site.hpp`
- `modules/foundation/log/include/SushiDSP/foundation/log/log_thread.hpp`
- `modules/foundation/log/include/SushiDSP/foundation/log/logger.hpp`
- `modules/foundation/log/include/SushiDSP/foundation/log/scoped_log_sink.hpp`
- `modules/foundation/log/include/SushiDSP/foundation/log/stderr_log_sink.hpp`
- `modules/foundation/log/include/SushiDSP/foundation/log/sushi_log_record.h`
- `modules/foundation/log/include/SushiDSP/foundation/log/sushidsp_log_project.hpp`

## Depends on

Nothing from the foundation tier.

## Using it

Every category belongs to `K_SUSHIDSP_LOG_PROJECT`, the project `sushidsp` whose levels
`SD_LOG_LEVEL` sets. A module declares its one category in `<module>_log_category.hpp` beside its
own headers and logs only under it:

```cpp
inline constexpr SushiDSP::LogCategory K_LOG_LOG_CATEGORY = SushiDSP::make_log_category(SushiDSP::K_SUSHIDSP_LOG_PROJECT, "log");
```

Source: `modules/foundation/log/include/SushiDSP/foundation/log/log_log_category.hpp`

A call reads `SD_LOG_WARN(K_ROOM_LOG_CATEGORY, "format {}", value)`. The formatter takes `{}` and
nothing else: a value that needs a width, a precision or hex is turned into a string before the
call. It prints a bool, a `char`, any integer, a `float` or `double` with six significant digits,
a C string, a `std::string` and a `std::string_view`. A message longer than
`K_MAX_LOG_MESSAGE_LENGTH`, 1024 characters, is cut there.

Every record is one line:

```
[12:04:31.207] [WARN] [sushidsp.room] [t3] [room_node.cpp:88] slot 2 refused the set; keeping the old one
```

The fields are local time to the millisecond, the level, `project.module`, the thread's log number
and the file and line of the call. `format_log_line` writes that shape and nothing else does.

## Levels

Seven levels, `Trace` to `Fatal`, then `Off`, which is a setting and never the level of a call.
Every category starts at `Warn`.

`SD_LOG_LEVEL` sets the levels at the first log call: a level for every category, then any number
of `category=level` pairs, in any case, such as `SD_LOG_LEVEL=warn,room=debug,golden_compare=info`.
A value the logger cannot read is reported once on stderr and the levels stay as they are. In
code, `set_log_level(K_SUSHIDSP_LOG_PROJECT, level)` sets every category and
`set_log_level(category, level)` sets one; a call made before the first log call wins over the
environment. `log_level` reads either back.

Each category's level is one atomic in a `LogLevelTable` of `K_MAX_LOG_CATEGORIES`, 64 slots,
found by the category's hash, which `LogCategory` computes at compile time. Reading a level takes
no lock: the hash is probed from its slot until it or a free slot is found, usually at the first.
Setting a level takes a mutex. A sixty-fifth category keeps the level set for every category.

## Two gates

A call passes two gates before its arguments are evaluated. At compile time each translation
unit has a ceiling, `SD_LOG_CEILING`: `Off` when `SD_ENABLE_LOGGING` is defined as 0, `Debug`
under `NDEBUG` and `Trace` otherwise. A call below it compiles to `((void)0)`. `SD_LOG_ERROR` and
`SD_LOG_FATAL` are never compiled out. At run time a call below its category's level returns
after the level is read.

## Delivery

`default_logger()` delivers from one logger thread, an `AsyncLogSink` it starts at the first
record. A record is formatted on the caller's stack and copied into the bounded queue; a record
that finds the queue full is dropped and counted, and the next record delivered is preceded by
a `WARN` under `log` naming how many were lost. The queue holds `K_DEFAULT_LOG_QUEUE_CAPACITY`,
256 records. `set_log_synchronous(true)` first waits for the queue, then delivers each record before the call
returns: `tests/main.cpp` does it once for the test binary.

A `Fatal` record is delivered on the calling thread after everything queued, then the sink is
flushed. A sink that throws loses nothing: the logger writes the exception and the record to
stderr. A sink never logs, and never sets the sink, a level or the delivery.

`set_log_sink(sink)` replaces the sink with a `LogSink`, a callable of up to 48 bytes held in a
`Delegate` with no heap allocation, and an empty sink restores stderr. Inside the module a sink is
a `LogRecordSink`; `ScopedLogSink` installs one for a scope and flushes it on the way out. `CapturingLogSink` stores each record,
its message and its whole line, before `write` returns.

The logger of the process is never destroyed, so a call is valid during static initialisation
and at exit. At static destruction it delivers what is queued, stops its thread and delivers on
the caller from then on. A process ended without static destruction loses its queue.

## Joining another logger

`log_forward_sink()` returns a sink that enters another logger's record into this one. The record
passes no level gate here, keeps its `time_ns`, and gets this logger's thread number and line. A
SushiDSP logger sends its records to SushiRuntime's with two calls at start-up:

```cpp
SushiDSP::set_log_synchronous(true);
SushiDSP::set_log_sink(SushiRuntime::log_forward_sink());
```

Source: not compiled

A host outside C++ uses `sushi_log_record.h`: `sd_set_log_sink(function, user)` hands each record
to a C function as a `SushiLogRecord`, whose strings are not NUL-terminated and whose `size` is
the `sizeof` of the logger that filled it; a null function restores stderr.
`sd_set_log_synchronous(1)` is the C form of `set_log_synchronous(true)`.

## The audio thread

The audio thread never logs. `Logger::log` formats on the caller's stack, reads the time, and
takes a mutex for the length of one `shared_ptr` copy, none of which a real-time callback may do.
A failure on the audio thread is counted in an atomic, the way
`TrackRecorder::dropped_blocks()` counts a refused block, and the control thread logs the count.
