# `modules/foundation/log`

SushiDSP's front over SushiLog: the `SD_LOG` macros, the `sushidsp` log project and a capture
for tests. SushiLog holds the logger, its levels, its queue and its sinks; its
`docs/guides/LOGGING_GUIDE.md` says how to use them.

## Owns

- `modules/foundation/log/include/SushiDSP/foundation/log/log.hpp`
- `modules/foundation/log/include/SushiDSP/foundation/log/log_capture.hpp`
- `modules/foundation/log/include/SushiDSP/foundation/log/sushidsp_log_project.hpp`

## Depends on

SushiLog, the version `cli/sushistack.deps.toml` pins. `sd` resolves its checkout and hands it
to CMake as `SUSHILOG_DIR`. A target whose `SUSHIDSP_LOG_STATIC` property is on links
`SushiLog::SushiLogStatic`; every other target links the shared `SushiLog::SushiLog`.

## Using it

A module declares its one category in `<module>_log_category.hpp` and logs only under it:

```cpp
inline constexpr SushiDSP::LogCategory K_ROOM_LOG_CATEGORY =
    SushiDSP::make_log_category(SushiDSP::K_SUSHIDSP_LOG_PROJECT, "room");
```

Source: `modules/nodes/room/include/SushiDSP/nodes/room/room_log_category.hpp`

A call reads `SD_LOG_WARN(K_ROOM_LOG_CATEGORY, "format {}", value)`. Each translation unit's
ceiling is `K_SUSHIDSP_LOG_CEILING`: `Error` when `SD_ENABLE_LOGGING` is 0, `Debug` under
`NDEBUG`, `Trace` otherwise. `SD_LOG_LEVEL` sets the levels of the `sushidsp` project, such as
`SD_LOG_LEVEL=warn,room=debug`.

`LogCapture` collects every record synchronously for its lifetime and then restores stderr;
tests read its `lines()`.

## Plug-ins

The CLAP plug-in links `SushiLogStatic`, so it holds a private logger. A host joins it to its
own by calling the plug-in's exported `sushidsp_join_log` with its `sl_forward_record`, before
the plug-in logs.

## The audio thread

The audio thread never logs. A failure there is counted in an atomic, the way
`TrackRecorder::dropped_blocks()` counts a refused block, and the control thread logs the count.