# tests/nodes/room/test_room_scene

Every expected number below is derived by hand from the shipped material and furniture tables, whose
coefficients are stored as `float`. The expected arrays in the tests use the float-rounded
coefficients, written out to 17 significant digits.

## `test_records.cpp`

### `RoomRecordsAreSmallEnoughToPublishPerGuiFrame`

`// A 128-tap set is 19472 bytes and 512 modes are 28688.`

One EarlyTap is 4 + 4 + 40 + 40 + 64 = 152 bytes, a multiple of the double's alignment, so a
128-tap set is 19456 plus 12 of header, padded to 19472; one Mode is 40 + 4 + 8 = 52 padded to
56, so 512 of them are 28672 plus 12, padded to 28688. Both stay under their budgets, 24 KiB and
64 KiB; the test pins the budgets and not the layout.

## `test_behaviour.cpp`

### The reference rooms

`// Every row is derived in tests/nodes/room/test_room_scene/README.md.`

A mean absorption is the area-weighted mean of the faces, a patch replacing its own area on its
face. Per band, with the shipped coefficients:

- Fresh scene, 6.0 x 4.0 x 2.5 m: 74 m2 of gypsum board on studs and 24 m2 of heavy carpet over
  98 m2, so 125 Hz is (74 \* 0.29 + 24 \* 0.02) / 98 = 0.223878 and 500 Hz is
  (74 \* 0.05 + 24 \* 0.14) / 98 = 0.072041.
- Garage: 92 m2 of unpainted concrete and 6 m2 of open doorway over 98 m2, so 125 Hz is
  (92 \* 0.01 + 6 \* 1.0) / 98 = 0.070612 and 500 Hz is (92 \* 0.02 + 6 \* 1.0) / 98 = 0.080000.
- Bedroom: 14 m2 of carpet, 51.5 m2 of gypsum board and 1.5 m2 of glass over 67 m2, so 500 Hz is
  (14 \* 0.14 + 51.5 \* 0.05 + 1.5 \* 0.18) / 67 = 0.071716.
- Block room, 5.0 x 4.0 x 2.7 m: one material everywhere, so the mean is that material's row.
- Dead room: the 12 m cube whose four walls are open doorways over carpet: 576 m2 of doorway and
  288 m2 of carpet over 864 m2, so 125 Hz is (576 + 288 \* 0.02) / 864 = 0.673333.
- Small concrete room, 3.0 x 2.5 x 2.4 m: one material, the concrete row.
- Gypsum cube, 10 m: one material, the gypsum row.

### `RoomSceneFloorIsEyringInvertedAtTheDecayBound`

`// The floor is 1 - exp(-0.161 V / (12 S)); see tests/nodes/room/test_room_scene/README.md.`

Eyring's decay time 0.161 V / (-S ln(1 - a)) set to the 12 s bound gives
a = 1 - exp(-0.161 V / (12 S)). The default scene and the garage have V = 60 m3 and S = 98 m2:
0.008181. The dead room's cube has V / S = 2 m: 1 - exp(-0.161 \* 2 / 12) = 0.026477. The small
concrete room has V = 18 and S = 41.4: 0.005816. The bedroom has V = 36.4 and S = 67: 0.007263.
The block room has V = 54 and S = 88.6: 0.008144. The gypsum cube has V / S = 10 / 6: 0.022113.

### `RoomSceneRefusalNamesTheFailingBandItsFloorAndTheBound`

`// The concrete cube rings 32.04 s against the 12 s bound.`

The 12 m cube of unpainted concrete: 0.010000 at 125 Hz against a floor of 0.026477, Eyring's
32.04 s from its surfaces alone against the 12 s bound.

### `RoomSceneRefusesARoomWhoseLowestBandAloneIsUnderTheFloor`

`// Carpet clears the floor from 250 Hz up and misses it at 125 Hz alone.`

The 12 m cube of heavy carpet has 0.02 at 125 Hz and 0.06 or more from 250 Hz up, against the
floor of 0.026477, so only the lowest band is under it and the refusal names that band.

### `RoomSceneAdmitsAConcreteCubeOnlyUnderTheBound`

`// A bare concrete cube crosses the bound at 4.495 m.`

A bare unpainted concrete cube crosses the bound at an edge of 4.495 m, where 0.161 L / 6 over
-ln(1 - 0.01) reaches 12 s; 4.4 m gives a floor of 0.009791, under the coefficient 0.01, and 4.6 m
gives 0.010233, over it.

### `RoomSceneFloorRisesWithVolumeOverArea`

`// The same concrete passes in a 6.0 x 4.0 x 2.5 m shoebox.`

Absorption alone does not decide it: the same concrete at 6.0 x 4.0 x 2.5 m has a floor of
0.008181, under the concrete's 0.01, so it passes, while the 4.6 m cube's floor is 0.010233.

## `test_furnishings.cpp`

### `RoomSceneBoundUsesTheExactFormNotTheLinearOne`

`// Rows hold a quantity between the linear form and the exact form.`

The 12 m concrete cube has S = 864 m2, a floor f = 0.026477 and a surface absorption a = 0.01 at
125 Hz, its binding band. The furnished absorption is 1 - (1 - a) exp(-A / S), so the area that
reaches the floor is A = S \* -ln((1 - f) / (1 - a)) = 14.5005 m2, while the linear form
S \* (f - a) gives 14.2357 m2. A two-seat sofa gives 2.6 m2 at 125 Hz, so the exact form needs
5.577 sofas and the linear form 5.475. 5.52 sofas lie between them and are refused; 5.63 sofas
clear the exact form and pass.
