# `modules/nodes/microphone`, the microphone rig element

One `AudioNode`, mono in and mono out, registered as `sushidsp.microphone`. A room places a
microphone and computes what it hears; `MicrophoneNode` is the gear that signal passes through on
its way to a channel strip.

## What it holds

| Parameter | Kind | Default | Meaning |
| --- | --- | --- | --- |
| `pattern` | Choice, 0 to 3 | 1 | The capsule's polar pattern: omni, cardioid, supercardioid, figure-eight (`MicrophoneNode::Pattern`) |
| `proximity` | Boolean | 1 | Whether the capsule's gradient share takes the proximity effect |
| `polarity` | Boolean | 0 | Inverts the signal |
| `room` | Boolean | 0 | Marks an overhead microphone, which a rig sums on its Room strip |

The node has no position. Where a microphone stands and faces belongs to the room's arrangement
(`modules/nodes/room`, `Room::Arrangement`), keyed by the identity of this rig element.

`polarity` is the only setting `process()` applies. `pattern` and `proximity` change what the room
computes at the microphone's place, so a host reads them here and writes them into the
microphone's entry in the arrangement. `room` changes routing, which is the host's.

## Dependencies

`SushiDSP::sushi_dsp_core` only. The module does not link the room: the pattern is its own
enumeration, in the order `room::PolarPattern` counts the same four patterns.

## Tests

`tests/functional/unit/test_microphone_node.cpp`: the pass-through, the inversion, each parameter
read back, and the port layout.
