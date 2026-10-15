# tests/nodes/capture/test_track_recorder

## `test_rt_safety.cpp`

### `WritePerformsNoHeapAllocation`

`// The trap sees the ring copy and the drop counter.`

The one capture path the audio thread touches. arm() allocates the rings and starts the writer
thread before the trap opens, and the first block is written and drained outside it so that
WavWriter's stream buffer already exists: what the trap then sees is the ring copy and, once the
ring fills, the drop counter.
