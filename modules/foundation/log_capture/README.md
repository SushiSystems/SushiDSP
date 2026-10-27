# `modules/foundation/log_capture`

A test-side scope that collects every SushiLog record. No product links it.

## Owns

- `modules/foundation/log_capture/include/SushiDSP/foundation/log_capture/log_capture.hpp`
- `modules/foundation/log_capture/include/SushiDSP/foundation/log_capture/log_level_scope.hpp`

## Depends on

`log`, for the SushiLog it captures from.

## Using it

`LogCapture` turns on synchronous delivery and replaces the sink for its lifetime; `lines()`
returns each record's level, module, message and line. When it ends it restores stderr, not a
sink set before it, so a test holds one capture at a time.

`LogLevelScope` sets one category's level for its lifetime and sets the level it found back
when it ends. A test that expects an INFO or DEBUG record holds one, since every category
starts at WARN.
