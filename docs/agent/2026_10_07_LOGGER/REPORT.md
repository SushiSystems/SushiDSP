# Logger report

**Status:** Shipped — 2026-10-07

SushiDSP had no logger. The core library has one under `include/SushiDSP/log/` and `src/log/`,
and the handled failures the estate audit found dropped are logged.

## Shape

`SD_LOG_TRACE` to `SD_LOG_FATAL` take a category, a format with positional `{}` markers and the
values. A level below `SD_LOG_MIN_LEVEL` compiles to nothing; a level off at run time does not
evaluate its values. One brick per header: level, category, record, argument, formatter, sink
interface, stderr sink, bounded line queue, asynchronous sink, capturing sink, logger, the
logger of the process, and a scoped guard that installs a sink and flushes it.

The asynchronous sink copies a line into a queue its constructor allocated and never blocks or
allocates on the caller. A full queue drops the line and counts it; the worker reports the
count once. The audio thread does not log.

Categories: `K_CAPTURE_LOG_CATEGORY`, `K_ROOM_LOG_CATEGORY`, `K_HOST_GUI_LOG_CATEGORY`, and
`K_LOG_LOG_CATEGORY` for the drop report.

## Failures now recorded

| Site | Level | What the line names |
| --- | --- | --- |
| `TrackRecorder::arm`, opening the take file | ERROR | the path, the cause, that the recorder stays disarmed |
| `TrackRecorder::arm`, starting the writer thread | ERROR | the path, the cause, that the file is closed |
| `TrackRecorder::drain_once`, appending | ERROR | frames asked and written, the cause, that recording stops |
| `RoomNode::set_hrtf`, preparing the decoder | ERROR | direction count, rate, the cause, that the set is refused |
| `TopologyApplier::roll_back`, publishing | WARN | removed slots, the cause, that the earlier graph keeps playing |
| `SlotGraph::apply_cabinet_to_live_node` | WARN | the path, slot and rate, that the slot forgets the path |
| `ui::list_directory` | WARN | the cause and the path, that the listing is shown empty |

The `SlotGraph` site also logs the case where the node returns false, which the audit had not
listed. The three `main` functions install the asynchronous sink and flush it on every return.

## Evidence

- `sd build` completed; `sd test` ran 1 123 tests with none failing.
- The 23 `Unit_Log` cases pass. Of the three call-site tests, `TrackRecorder` and `RoomNode`
  pass and `list_directory` reports itself skipped on Windows.
- The four checkers print nothing.

## Not done

Listed in `docs/design/REMAINING_WORK.md` under "Logger follow-ups".
