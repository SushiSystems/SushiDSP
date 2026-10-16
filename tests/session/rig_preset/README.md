# tests/session/rig_preset

## `test_rig_preset.cpp`

### `DeserializeRigFromJsonRebuildsBranchingTopologyAndParameters`

`// The mixer's inputs are handles 2 and 3, in that order.`

Topology: the mixer (preset id 4, handle 4 since deserialize preserves JSON-array add order)
must have exactly the two upstream nodes the JSON's "inputs": [2, 3] declares, the amp (handle
2) and the dry tubescreamer (handle 3), in that order.

### `SerializeThenDeserializeRoundTripsABranchingGraph`

`// Rebuilt handles line up 1:1 with the original's.`

Topology: deserialize_rig_from_json adds nodes into an empty graph in the same JSON-array order
that serialize_rig_to_json wrote them (which itself follows original.nodes()'s add_node()
order), so rebuilt's handles line up 1:1 with original's. Compare upstream_of() as ordered
vectors for every handle to catch a dropped connect() or a reversed fan-in that a "does some
node have the right value" check would miss.

### `DeserializeRigFromJsonRejectsMalformedEntriesWithoutTouchingTheGraph`

`// A rejected preset leaves the graph unmodified.`

Each of these is rejected in the first validation pass, so the caller's graph must come back
completely unmodified, never half-populated with orphan nodes that would then make the caller's
next compile() throw "no sink node found".

`// Id 7 is declared by no entry.`

Dangling "inputs" reference: id 7 is declared by no entry. Without the check this escapes as
std::out_of_range from std::map::at(), which does not derive from std::runtime_error.

### `SerializeAndDeserializeStillWorkWithNoCallback`

`// Call sites without a callback behave unchanged.`

Regression guard: every existing call site (no callback argument) must keep compiling and
behaving exactly as before this task.
