# tests/foundation/core/test_rig_graph

## `test_behaviour.cpp`

### `RigGraphProcessZeroesOutputWhenGraphIsEmpty`

`// The sentinel proves process() writes zero itself.`

Pre-fill with a sentinel to prove process() actively writes zero rather than merely leaving
whatever the caller's buffer already held (e.g. SDL's raw stream buffer replaying stale audio
from a previous block).

### `RigGraphRootNodeStillReceivesNullptrWhenGraphHasNoInput`

`// The graph passes nullptr through to a root node.`

Unchanged behavior: a root node must not crash or misbehave when the graph itself has no live
input (e.g. the existing SDL2 output-only path), GainNode would dereference a null
context.inputs, so this uses ConstantSourceNode (0 input ports) to prove the \*graph\* passes
nullptr through, without the test node needing to read it.

### `RigGraphDisconnectRemovesOnlyThatEdge`

`// A root node is fed context.inputs directly.`

gain_b now has no upstream edge; RigGraph feeds root nodes context.inputs directly, so it must
NOT receive source's output. Use a real live-input buffer here (rather than nullptr) since
GainNode unconditionally reads context.inputs[0] and a null root input would crash the fixture,
not RigGraph.

`// 8.0 comes out only if exactly one edge was removed.`

gain_b lost its edge from source, so it becomes a root fed by device_input (1.0) directly; with
the source->gain_a edge intact, gain_a still becomes last in topological order (unblocked only
once source is processed) and is what gets copied to the graph's output. Its value, 8.0
(source's 4.0 \* gain_a's gain of 2.0), only comes out right if BOTH held: disconnect() actually
removed source->gain_b (else gain_a would still be sandwiched between source and gain_b in a
different topological order, and the last node copied to output would be gain_b at
4.0\*3.0=12.0), and the untouched source->gain_a edge survived (else gain_a would be a root fed
by device_input directly, giving 1.0\*2.0=2.0 instead of 8.0).

### `RigGraphCompileThrowsWhenNoSinkNodeExists`

`// Two unconnected nodes form a graph with no sink.`

Two live nodes, no edges between them at all: neither node has an outgoing edge, but neither has
an incoming edge either (and live_node_count != 1), so neither qualifies as a sink candidate.
This is a genuine zero-sink, non-empty, acyclic graph shape, and compile() must reject it rather
than leaving sink_handle_ pointing at a stale/default handle for process() to misuse.

### `RigGraphFeedsRootNodeExternalInputThroughAFullWidthGatherArray`

`// Every node receives a K_MAX_FAN_IN_OUT-sized array.`

Regression: a multi-input node (MixerNode reads all K_MAX_FAN_IN_OUT slots) placed as a ROOT node , 
no incoming edges, reachable via a hand-authored or rig_preset-loaded JSON whose mixer has an
empty "inputs", used to be handed the host's raw context.inputs pointer. Hosts pass a 1-element
array, so the mixer read 7 pointers past the end of it and dereferenced whatever garbage
followed. Every node must now receive a K_MAX_FAN_IN_OUT-sized array whose unused slots are nullptr.

`// Unused slots are nullptr, never garbage.`

gain_0 defaults to 1.0 and every other slot must be silent (nullptr), never garbage, so the
output is exactly the external input.

### `RigGraphSumsFanInPerChannel`

`// A surplus edge is summed into the slot per channel.`

Two producers reach a consumer that declares one input slot, so the surplus edge is summed into
that slot channel for channel rather than dropped.
