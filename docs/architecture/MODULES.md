# Modules

A module is one library under `modules/<tier>/<module>/` with its own `README.md`. The README
says which class owns which block, which constants and approximations the module carries, and
where it departs from the design spec. This page links to those READMEs and states nothing
else about a module.

The tiers, lowest first, are `nodes`, `catalog` and `session`. A module may include its own
tier and the ones below it; `tools/layering/check_layering.py` holds the order in
`K_TIER_ORDER` and checks it.

## `nodes`

| Module | Registered as | What it is |
| --- | --- | --- |
| [`cabinet`](../../modules/nodes/cabinet/README.md) | `sushidsp.cabinet` | Cabinet impulse-response convolution |
| [`capture`](../../modules/nodes/capture/README.md) | none | Recording takes and playing them back |
| [`dd3`](../../modules/nodes/dd3/README.md) | `sushidsp.dd3` | Boss DD-3 digital delay |
| [`diezel_vh4`](../../modules/nodes/diezel_vh4/README.md) | `sushidsp.diezel_vh4` | Diezel VH4, channel 3 |
| [`jcm800`](../../modules/nodes/jcm800/README.md) | `sushidsp.jcm800` | Marshall JCM800 2203 |
| [`mesa_mark2c`](../../modules/nodes/mesa_mark2c/README.md) | `sushidsp.mesa_mark2c` | Mesa/Boogie Mark IIC+, lead mode |
| [`peavey5150`](../../modules/nodes/peavey5150/README.md) | `sushidsp.peavey5150` | Peavey 5150, lead channel |
| [`microphone`](../../modules/nodes/microphone/README.md) | `sushidsp.microphone` | A microphone as a rig element: pattern, proximity, polarity and the room flag |
| [`room`](../../modules/nodes/room/README.md) | `sushidsp.room` | One room holding up to three cabinets, a listener and up to eight microphones |
| [`routing`](../../modules/nodes/routing/README.md) | `sushidsp.splitter`, `sushidsp.mixer`, `sushidsp.pan`, `sushidsp.channel_strip`, `sushidsp.stereo_strip`, `sushidsp.bus` | The rig graph's routing nodes and the mixer's strips and buses |
| [`tubescreamer`](../../modules/nodes/tubescreamer/README.md) | `sushidsp.tubescreamer` | Ibanez Tube Screamer |

## `catalog`

| Module | What it is |
| --- | --- |
| [`node_registry`](../../modules/catalog/node_registry/README.md) | The product catalogue, `node_registry()` and `make_node()` |

## `session`

| Module | What it is |
| --- | --- |
| [`rig_preset`](../../modules/session/rig_preset/README.md) | Rig presets as JSON |
