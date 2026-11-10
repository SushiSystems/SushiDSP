# tests/nodes/capture/test_track_recorder

## `test_rt_safety.cpp`

### `WritePerformsNoHeapAllocation`

`// The trap sees the ring copy and the drop counter.`

The one capture path the audio thread touches. arm() allocates the rings and starts the writer
thread before the trap opens, and one block is written outside it. The trap counts allocations of
the thread that arms it, so the writer thread draining the ring is never waited for: what the trap
sees is the ring copy and the drop counter.
