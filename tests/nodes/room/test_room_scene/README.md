# tests/nodes/room/test_room_scene

## `test_records.cpp`

### `RoomRecordsAreSmallEnoughToPublishPerGuiFrame`

`// One EarlyTap is 152 bytes and one Mode is 56.`

One EarlyTap is 4 + 4 + 40 + 40 + 64 = 152 bytes, a multiple of the double's alignment, so a
128-tap set is 19456 plus 12 of header, padded to 19472; one Mode is 40 + 4 + 8 = 52 padded to
56, so 512 of them are 28672 plus 12, padded to 28688.

## `test_behaviour.cpp`

### `RoomSceneWeightsAPatchByItsArea`

`// 92 m2 of concrete and 6 m2 of doorway over 98 m2.`

92 m2 of unpainted concrete and 6 m2 of open doorway over 98 m2: 125 Hz (92 \* 0.01 + 6 \* 1.0)
/ 98 = 0.070612; 500 Hz (92 \* 0.02 + 6 \* 1.0) / 98 = 0.080000.

### `RoomSceneComputesTheBedroomsWeightedAbsorption`

`// At 500 Hz the bedroom's mean absorption is 0.071716.`

14 m2 carpet, 51.5 m2 gypsum, 1.5 m2 glass over 67 m2: 500 Hz (14 \* 0.14 + 51.5 \* 0.05 + 1.5
\* 0.18) / 67 = 0.071716.

### `RoomSceneAcceptsEveryReferenceRoom`

`// The default scene clears its floor by a factor of 8.8.`

The default scene is 6.0 x 4.0 x 2.5 m of gypsum board on studs over a carpeted floor: 74 m2 of
board and 24 m2 of carpet over 98 m2 give (74 \* 0.29 + 24 \* 0.02) / 98 = 0.223878 at 125 Hz
and (74 \* 0.05 + 24 \* 0.14) / 98 = 0.072041 at 500 Hz, its least absorbent band, which clears
the 0.008181 floor this shoebox needs by a factor of 8.8.

`// The dead room reads 0.673 against a floor of 0.026477.`

The 12 m carpeted room with four open faces the node tests render, whose absorption runs 0.673
at 125 Hz against a floor of 0.026477.

### `RoomSceneRefusesARoomThatRingsPastTheModulesDecay`

`// The concrete cube rings 32.04 s against the 12 s bound.`

The 12 m cube of unpainted concrete: 0.010000 at 125 Hz against a floor of 0.026477, Eyring's
32.04 s from its surfaces alone against the 12 s bound.

`// The bound is on every band, not on the mid one.`

The bound is on every band, not on the mid one: a 10 m cube of gypsum board on studs is dead at
125 Hz, 0.29, and rings 6.57 s at 1 kHz, so a room whose bass is fine and whose mid is not stays
valid while the same shell in concrete does not.

`// A bare concrete cube crosses the bound at 4.495 m.`

A bare unpainted concrete cube crosses the bound at an edge of 4.495 m, where 0.161 L / 6 over
-ln(1 - 0.01) reaches 12 s; 4.4 m gives 11.75 s and 4.6 m gives 12.28 s.

`// The floor rises with the room's volume over its area.`

Absorption alone does not decide it: the same concrete at 6.0 x 4.0 x 2.5 m passes, since the
floor rises with the room's volume over its area.
