# tests/nodes/cabinet

## `test_cabinet_node.cpp`

### `CabinetNodeInstallsAndAppliesAnImpulseResponse`

`// An identity IR makes the cabinet a one-partition delay.`

An identity IR makes the cabinet a pure delay of one partition; every sample that has seen the
delayed input by now must reproduce it exactly.

### `CabinetNodeSurvivesVariableBlockSizes`

`// An identity IR must survive any host block size.`

StreamingConvolution re-blocks the stream internally, so the cabinet must survive a host that
hands the node a different frame count every callback - including counts that are not powers of
two and not multiples of the partition. An identity IR makes the whole cabinet a pure delay of
exactly one partition, so the processed stream must reproduce the raw input stream shifted by
latency_samples(), sample for sample.

### `ProcessPerformsNoHeapAllocationWithCabinet`

`// 500 blocks wrap the spectrum history at least once.`

Cabinet convolution is now sushidsp.cabinet's own surface, not any amp's: it runs
StreamingConvolution's FFT, spectrum history, and overlap-add buffers, all of which must have
been sized in prepare(). 500 blocks is more than the impulse response's partition count, so the
circular spectrum history wraps at least once under the trap.

### `SetCabinetSwapsWithoutAllocatingOnTheAudioThread`

`// The trap is armed around the blocks, not set_cabinet().`

A cabinet published from the control thread must reach process() through RtSwapSlot's single
atomic acquire - no allocation, no lock. Only the audio thread's half of a swap has to be
RT-safe (publish() builds the convolver and is expected to allocate), so the trap is armed
around the processing blocks and disarmed across each set_cabinet(). That way every trapped
block is one that picks up a freshly published cabinet, which is the case the name promises - a
version of this test that published everything up front would only be proving that convolving
allocates nothing.
