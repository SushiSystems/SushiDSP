# Room furnishings: the measured record

**Status:** Shipped — 2026-10-21

The measurements behind [the room furnishings design](../../design/ROOM_FURNISHINGS.md), which
took them out when it took the design format.

## The empty Motus room against the model

The empty room, measured T30 against the model's, and the same room with the carpet down:

| Band | Measured, empty | Model, empty | Ratio | Measured, carpet | Model, carpet | Ratio |
| --- | --- | --- | --- | --- | --- | --- |
| 125 Hz | 2.572 s | 3.123 s | 1.214 | 2.704 s | 2.989 s | 1.105 |
| 250 Hz | 2.147 s | 3.856 s | 1.796 | 2.057 s | 3.473 s | 1.689 |
| 500 Hz | 2.009 s | 3.180 s | 1.583 | 1.635 s | 2.688 s | 1.644 |
| 1 kHz | 2.119 s | 3.429 s | 1.618 | 1.428 s | 2.087 s | 1.461 |
| 2 kHz | 1.958 s | 3.410 s | 1.741 | 1.166 s | 1.427 s | 1.224 |
| 4 kHz | 1.508 s | 2.080 s | 1.380 | 0.998 s | 0.973 s | 0.975 |

The model decays 1.10 to 1.80 times longer than the room, worst at 250 Hz. The carpet shortens
both, and it shortens the model further than it shortens the room: 39.2 % against a measured
32.6 % at 1 kHz, 58.2 % against 40.4 % at 2 kHz, 53.2 % against 33.8 % at 4 kHz.

## The built room's loss

Decided 2026-09-23. Tabulated coefficients under-predict a built room's absorption. Measured
minus modelled mean absorption, 125 Hz to 4 kHz, is +0.007, +0.020, +0.018, +0.018, +0.021 and
+0.018 in the empty Motus room and +0.062, +0.046, +0.030, +0.027, +0.030 and +0.012 in the empty
BRAS CR2 seminar room (146 m3, T30 from its ten dodecahedron responses,
`tests/nodes/room/fixtures/built_room_loss/bras_cr2_reference.csv`). The BRAS CR1 reverberation
chamber, sealed and purpose-built, shows no excess: its fitted concrete absorption is 0.009 to
0.019, below the database's initial estimate. The excess therefore comes from what a built room
carries (doors, windows, radiators, ducts, seams, leakage), not from the surface materials.

`K_BUILT_ROOM_LOSS` = 0.020, 0.025, 0.020, 0.020, 0.020 and 0.015 per face. With it the model
reads 0.96 to 1.04 of Motus and 1.14 to 1.21 of CR2 from 500 Hz to 2 kHz, and 0.75 of Motus and
2.1 of CR2 at 125 Hz (Motus asks for +0.007 there and CR2 for +0.062).

## The starter scene

The owner ear-checked the starter scene on 2026-09-23; placing its furniture changes its
reflections, so `starter_scene()` places nothing. The furnishing placement itself was added on
2026-10-13.
