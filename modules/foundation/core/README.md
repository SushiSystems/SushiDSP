# `modules/foundation/core`

The node and device interfaces, `RigGraph`, the real-time slots and the buffer pool.

## Owns

- `modules/foundation/core/include/SushiDSP/core/buffer_pool.hpp`
- `modules/foundation/core/include/SushiDSP/core/calibration.hpp`
- `modules/foundation/core/include/SushiDSP/core/denormal_guard.hpp`
- `modules/foundation/core/include/SushiDSP/core/audio_device.hpp`
- `modules/foundation/core/include/SushiDSP/core/audio_node.hpp`
- `modules/foundation/core/include/SushiDSP/core/cabinet_host.hpp`
- `modules/foundation/core/include/SushiDSP/core/node_descriptor.hpp`
- `modules/foundation/core/include/SushiDSP/core/parameter_change.hpp`
- `modules/foundation/core/include/SushiDSP/core/parameter_table.hpp`
- `modules/foundation/core/include/SushiDSP/core/port_layout.hpp`
- `modules/foundation/core/include/SushiDSP/core/rig_graph.hpp`
- `modules/foundation/core/include/SushiDSP/core/rt_graph_slot.hpp`
- `modules/foundation/core/include/SushiDSP/core/rt_swap_slot.hpp`
- `modules/foundation/core/include/SushiDSP/core/spsc_ring.hpp`

## Depends on

`io` (same tier).

## Notes cited from the source

A comment in this module's files states one invariant and names this file. The reasoning
behind each invariant follows, under the name of the file that cites it.

### `modules/foundation/core/include/SushiDSP/core/denormal_guard.hpp`

Every decaying state on the audio path (the halfband delay lines, the biquads, the coupling
capacitors, the `exp()` tails of the tube residuals) drifts through the denormal range on its way
to silence. On x86 a denormal operand costs on the order of a hundred times a normal one through a
microcode assist. Flushing denormals to zero is inaudible, since they sit near 1e-38 below full
scale, and it removes a CPU spike that shows up exactly when the signal is quiet. The mode is per
thread, so a guard on the audio thread changes nothing for the rest of the process.

### `modules/foundation/core/include/SushiDSP/core/cabinet_host.hpp`

`set_cabinet` builds the convolver in full on the control thread and hands it to the audio thread
as one atomic pointer swap, so `process()` never sees a partially built cabinet and never blocks.
The node retains the samples so that a later `prepare()` rebuilds the cabinet. It never resamples
them, so a `prepare()` at a different rate needs a fresh `set_cabinet()`. In every failing case
nothing is installed and nothing is retained.

### `modules/foundation/core/include/SushiDSP/core/rt_swap_slot.hpp` and `modules/foundation/core/include/SushiDSP/core/rt_graph_slot.hpp`

`RtGraphSlot` is the same mechanism as `RtSwapSlot<T>` with `RigGraph` in place of `T`; its
functions are defined in `modules/foundation/core/source/rt_graph_slot.cpp` and the notes below cover both.

A non-RT thread builds a whole new object and publishes it. The audio thread reads the active
object with one atomic load per callback. `reclaim()`, on the non-RT thread, frees a retired object
only once the audio thread has provably moved past it.

The threading contract has two halves. `publish()` and `reclaim()` must both be called from a
single, consistent non-RT thread; callers that need either from more than one thread synchronise
those calls themselves, because the class does not. `acquire_for_audio_thread()` must be called
from a single, consistent audio thread. The pointer it returns is valid only until that same
thread's next call to `acquire_for_audio_thread()` and must not be retained across calls: a
later `reclaim()` may free the object once this thread has moved past it.

`publish()` treats `active_` as a non-owning view of the newest entry in `retired_`. Ownership of
every object ever published lives in `retired_` from the moment it is inserted, keyed by the
generation it was published at. The value `store()` replaces needs no handling: the object it
pointed at already sits in `retired_` from its own `publish()` call, and `reclaim()` is what
eventually frees it. The order of the three steps matters for two reasons.

1. Exception safety. `retired_.emplace_back()` must take ownership of the new object before
   `active_` is ever pointed at it. If `emplace_back()` throws, for instance `std::bad_alloc`
   while growing the vector, the local `unique_ptr` unwinds and frees the object it owns. Had
   `active_` already been stored, it would be left dangling and visible to the audio thread.
2. The audio thread's invariant. `active_` must be stored strictly before `publish_generation_`
   observably advances, so that reading the generation before `active_` on the audio thread can
   never race ahead of a publish still in flight.

Since only one non-RT thread publishes, there is no concurrent `publish()` to race with. The next
generation is computed locally with no atomic read-modify-write, the object moves into `retired_`
first, then `active_` is stored, and only then is the new generation number published.

`acquire_for_audio_thread()` reads the generation before `active_`. Because `publish()` stores
`active_` strictly before bumping `publish_generation_`, any object published at a generation up
to k, the value read, has already had its `active_` store land. Reading `active_` second therefore
always observes an object whose generation is at least k, and `reclaim()`, bounded to generations
below k, can never free the object this call is about to return.

In `RtSwapSlot::reclaim()` a zero watermark means `acquire_for_audio_thread()` has never returned
a non-null object. No retired entry can then be held by the audio thread, and every entry but the
active object is past.

### `modules/foundation/core/source/rig_graph.cpp`

`compile` clears the cached latency, the sink handle and the topological order before it does
anything else. A `compile()` that throws partway through, on a cycle, on several sinks or on none,
can then never leave a stale latency or sink value behind from an earlier successful one.

A node with no upstream edge is a root node. `process` feeds it the graph's own live input, for
instance an ASIO device's captured audio, so a rig can start directly from live input without a
synthetic source node.

The host's own inputs array is never handed to a node directly. Hosts pass a one-element array,
while a multi-input node such as `MixerNode` reads all `K_MAX_FAN_IN_OUT` slots. Routing the external
input through slot 0 of the fixed-size gather array means every node sees a full-width array whose
unused slots are `nullptr`, and no node reads past the host's array.

An empty graph must still fully define its output. Leaving the caller's buffer untouched would
replay whatever it last held, SDL's raw stream buffer for one, which is audible garbage looping at
block rate.
