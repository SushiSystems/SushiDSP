# Room Furnishings and Finishes
**Status:** Open — phase 4 of 4, the low-band built-room loss unresolved

The room module (`modules/nodes/room`) models a shoebox from six surface materials. It has no
furniture, so the room a musician plays in, with a bed, a desk and a wardrobe, cannot be
described, and the default scene decays about three times longer than the furnished rooms it
stands for. This document records the design that closes the gap. The task order is
[`docs/agent/2026_09_22_ROOM_FURNISHINGS/PLAN.md`](../agent/2026_09_22_ROOM_FURNISHINGS/PLAN.md).
The physics of the module as built is in
[`modules/nodes/room/README.md`](../../modules/nodes/room/README.md); the original spec is
archived at
[`docs/archive/superpowers/specs/2026-09-08-room-acoustics-design.md`](../archive/superpowers/specs/2026-09-08-room-acoustics-design.md).

## 1. Goal

A user builds a garage, a bathroom, a living room or a bedroom in three layers, and the module
renders the decay, the early reflections and the standing waves that room really has:

1. **Shell.** Dimensions and openings (doors, windows).
2. **Finishes.** One material per surface.
3. **Furnishings.** Objects in the room: a bed, a wardrobe, a desk, a sofa, curtains.

The starter scene, the room a fresh room slot loads, is the maintainer's own: 6.0 x 4.0 x 2.6 m
(a TOKI clear height), a Turkish 3+1 flat, painted plaster over brick on one side and over
concrete on the other, parquet under a 12 m2 rug, a heavy curtain over the window (4 m2), a
wooden door, a double bed, a desk, a wardrobe 1.77 m wide (three modules) and a chair, and no
sofa. It is one function, `starter_scene()`, in its own file. `RoomScene{}` stays the bare 6 x 4 x
2.5 m gypsum-and-carpet box that the tests and the module README pin. The height, the curtain,
the door and the rug are the author's choices where the maintainer gave none; the panel changes
each.

"Correct" means the module's decay time and direct-to-reverberant ratio agree with measured
rooms of the same kind, not that the tail was turned down until it sounded right.

## 2. What the measurements say

The shipped default (gypsum board on studs, carpet, no furniture, 60 m3) decays for 1.3 s at
500 Hz. Measured furnished rooms of this size do not.

| Source | Finding |
| --- | --- |
| Díaz et al., 11 687 Spanish dwelling rooms, 11 457 furnished | Furnished bedrooms and living rooms behave alike and decay less at higher frequency. |
| Mašović et al., TELFOR, 91 furnished and 151 unfurnished rooms | Furnished rooms under 50 m3 sit near 0.5 s from 125 Hz up; unfurnished rooms average 2.5 s below 500 Hz. Equivalent absorption area is 8 to 10 m2 in small furnished rooms and about 18 m2 in large ones. |
| Klein et al., JASA 2014, 30 Brazilian homes | Median T30 at 1 kHz is 0.49 s in bedrooms (34 m3) and 0.68 s in living rooms (64 m3). |
| Trans. Korean Soc. Noise Vib. Eng. 25(4), 2015, Korean apartments | Unfurnished living rooms average 1.11 s, furnished 0.48 s, furnished bedrooms 0.44 s. One bedroom, empty 0.81 s: a desk brings 0.76 s, a bed 0.58 s, a sofa in two steps 0.47 s and 0.38 s. |
| Pettersson et al., Build. Environ. 2022, a 27 m3 concrete bedroom | Empty, up to 1.78 s at 1.6 kHz; furnished, about 0.5 s from 50 Hz up. Furniture does not change absorption below 50 Hz. |

The last row is the closest to a TOKI room: concrete shell, furniture doing the work. The
Korean row gives the order of contribution: of the desk, the bed and the sofa the bed is the
largest single step (0.76 to 0.58 s), the desk the smallest (0.81 to 0.76 s).

## 3. Representation

Furniture is an equivalent absorption area, not geometry. Each object carries a per-band area
in square metres: the increase in a room's equivalent absorption area the object brings, floor
and walls under it included (ISO 354 object absorption, ISO 20189). The scene sums them:

    A_f(f) = sum over items of quantity x area_per_unit(f)

`A_f` is treated as one more volumetric loss beside the air. In a diffuse field the mean free
path is 4V / S, so a body with equivalent area `A_f` removes energy at `c x A_f / (4V)` per
unit energy, which is the same as an attenuation coefficient per metre:

    m_f(f) = A_f(f) / (4 V)

Three places in the module already carry the air's coefficient `m`, and each takes `m_f` at the
same seam:

| Layer | Today | With furnishings |
| --- | --- | --- |
| `EyringDecay` | denominator `-S ln(1 - a) + 4 m_air V` | `+ A_f`, that is `4 (m_air + m_f) V` |
| `ImageSourceSolver` | each tap folds `AirAbsorption::over_path(d)` | a reflected tap also folds `exp(-m_f d / 2)` in pressure |
| `ModalSolver` | wall admittance gives each mode's damping | `+ c m_f / 2` nepers per second |

`DiffuseField` and `LateReverbDesign` read Eyring's decay, so the tail, its level and its
per-line filters follow without a change. All four layers agree because they read one number.
Feeding Eyring alone would shorten the tail and leave the early taps and the standing waves at
the bare room's decay, which opens a level step at the seam the module's acceptance closes.

A covering is not an object. A curtain over a window or a rug on a floor replaces the absorption
of the surface under it, and `RoomScene::patches` already models that: a patch takes its area
from its face and brings its own material. Curtains and rugs are therefore `Materials` rows used
as patches, and a `Furniture` row is always per item. Counting a curtain's coefficient as an
area on top of the untouched window would count the glass twice: 49 % too much at 500 Hz for
3 m2 over the default window.

The per-metre loss is a statistical mean over many mean free paths. A single early tap loses
what a path of its length loses on average, not what its own path meets. The direction is right
and the error is largest on the first few taps, which set the direct-to-reverberant ratio; the
Motus comparison of section 8 is where it is measured.

Two consequences are decided here.

- **The direct path is not attenuated.** The image `(0, 0, 0)` is a line of sight. Furniture
  absorbs what reflects off it and scatters into it; it does not thin a clear path. Only
  reflected taps fold `m_f`.
- **Absorption is uniform.** A bed at one end of the room and at the other read the same.
  Placement, occlusion and shadowing are out of scope, as they were in spec section 11; a
  geometric object needs visibility tests at ten to a hundred times the control-thread cost.

## 4. Bricks

| Concern | Brick | Shape it copies |
| --- | --- | --- |
| Per-item absorption area per band, a citation per row | `furniture.hpp`, `FurnitureId` | `materials.hpp` |
| The items a scene holds and their quantities | `RoomScene::furnishings` | `RoomScene::patches` |
| Sum of a scene's items per band | `RoomScene::furnishing_absorption_area()` | `RoomScene::mean_absorption()` |
| The room's volumetric loss per metre, per band | `MediumAbsorption` | `AirAbsorption` |
| Preset words for materials and furniture | a `key` in each table row | replaces the word tables in `rig_persistence.cpp` |

`MediumAbsorption` is the seam the three consumers call. It returns the air's coefficient plus
`m_f`, so a further loss adds inside it and no consumer changes. `AirAbsorption` is untouched.
`RoomScene` cannot call `MediumAbsorption`, which reads `RoomScene`, so the validator's bound
(section 5) is computed in the scene from `furnishing_absorption_area()`, as
`min_mean_absorption()` already is.

Every furniture row is per item: a bed, a wardrobe module, a chair. `Furnishing` is
`{ FurnitureId item; double quantity; }`.

## 5. The validator

`RoomScene::is_valid` refuses a non-positive or non-finite quantity and more than
`K_MAX_FURNISHINGS` (16) entries. The 12 s decay bound counts furniture, since furniture is real
absorption: the surfaces' effective absorption becomes `1 - (1 - a_s) exp(-A_f / S)`, compared
with the existing `min_mean_absorption()` floor, which keeps its definition and its tests.

The validator re-implements the seam's arithmetic, since `RoomScene` cannot call
`MediumAbsorption`. A loss added to the medium later must be added to the scene's bound in the
same change, or `is_valid` admits rooms that ring past 12 s. `MediumAbsorption` and the three
consumers assume a scene that passed `is_valid`; none of them checks a furniture id.

A scene with no furnishings is the scene of today. Every existing test, golden and preset
renders bit for bit the same: `A_f = 0` adds exactly `0` to each sum.

## 6. Data and its provenance

A row lands only with a citation. Three kinds of row exist, and the citation says which.

- **Measured per object**, ISO 354 or ISO 20189: sofas (two measured test reports, about 2.6 to
  3.8 m2 for a two-seater), office and school furniture.
- **Tabulated**: chairs and people (EN 12354-6 annex C, Everest).
- **Coverings are materials, not furniture**: curtains (heavy velour, 18 oz/yd2 draped to half:
  0.14, 0.35, 0.55, 0.72, 0.70, 0.65 per square metre covered) and rugs join `Materials` and
  enter a scene as patches.
- **Derived**: a bed, a desk and a wardrobe. No measured per-object figure was found for
  household versions of the three. A derived row states its formula and inputs in the citation
  and is bounded by the room-level figures in section 2.

Surface finishes needed for the shell layer: plaster on masonry (smooth, 0.013 to 0.05 across
the bands), painted brick, ceramic tile, parquet on concrete. Every one lies between 0.01 and
0.05, so the choice between them matters less than the furniture. The values so far come from
reprints of Everest and Pohlmann; each row is checked against the book or EN 12354-6 before it
lands.

## 7. Persistence and panel

- `RigPersistence` writes `"furnishings": [{ "item": "bed", "quantity": 1 }]` in a room's JSON.
  A preset without the field loads as no furnishings. It also writes `"built_room_loss": true`
  or `false`; a room object without that field loads with the loss off.
- Each row carries a `key` string. Preset words come from the table, so adding a material or a
  furniture row is one edit and the codec does not change. Today a new material needs three.
- `RoomPanel` groups its controls under Shell, Finishes and Furnishings. Furnishings is a list:
  an item, a quantity, a remove button, and an add button. The shell layer carries a patch
  editor for openings and coverings (door, window, curtain, rug): a face, a material and an area.
  Without it a curtain cannot enter a scene from the panel.

## 8. Validation against measured rooms

Tier 1, in the unit suite: the Eyring figure with furnishings against hand arithmetic; a tap's
gain against `exp(-m_f d / 2)`; the seam of `RoomNodeIsContinuousAcrossTheSchroederFrequency`
repeated in a furnished room; a furnished 60 m3 room's mid-band decay inside the band the
measurements of section 2 give.

Tier 2, a tool and a small table checked in: the Motus dataset (Zenodo 4923187, one 4.9 x 4.4 x
2.9 m concrete room in 830 furniture configurations, ambisonic RIRs and 3D models). Its small
files hold the geometry, the object quantities and assumed textbook coefficients, and no
measured decay time; the measured figures come from the RIRs. `tools/dsp/motus_reference.py`
reads the empty room (measurement 0001), carpet only (0002) and carpet with shelves (0003) and
writes T20, T30, EDT, DRR and C50 per octave into `tests/fixtures/motus_reference.csv`, and
`tests/functional/unit/test_room_motus.cpp` builds the same scenes from the module's rows and
prints both. The dataset's own object areas are mesh areas with occluded faces counted, not
measured object absorption, so it tests the empty room, the covering and the method, not the
household rows. The raw set is tens of gigabytes (a 461 MB `best_of` subset exists) and stays out
of the tree.

The empty room, measured T30 against the model's, and the same room with the carpet down:

| Band | Measured, empty | Model, empty | Ratio | Measured, carpet | Model, carpet | Ratio |
| --- | --- | --- | --- | --- | --- | --- |
| 125 Hz | 2.572 s | 3.123 s | 1.214 | 2.704 s | 2.989 s | 1.105 |
| 250 Hz | 2.147 s | 3.856 s | 1.796 | 2.057 s | 3.473 s | 1.689 |
| 500 Hz | 2.009 s | 3.180 s | 1.583 | 1.635 s | 2.688 s | 1.644 |
| 1 kHz | 2.119 s | 3.429 s | 1.618 | 1.428 s | 2.087 s | 1.461 |
| 2 kHz | 1.958 s | 3.410 s | 1.741 | 1.166 s | 1.427 s | 1.224 |
| 4 kHz | 1.508 s | 2.080 s | 1.380 | 0.998 s | 0.973 s | 0.975 |

The model decays 1.10 to 1.80 times longer than the room, worst at 250 Hz, and the 10 % bound
this section asked for at 500 Hz and 1 kHz is not met. The carpet shortens both, and it shortens
the model further than it shortens the room: 39.2 % against a measured 32.6 % at 1 kHz, 58.2 %
against 40.4 % at 2 kHz, 53.2 % against 33.8 % at 4 kHz. The test therefore bounds the model at
twice the measurement and prints the rest. Measurement 0003, carpet with four shelves, is read
from the RIRs and not modelled: the shelves have no row. What the figures mean for the rows is
section 11.

## 9. What this does not do

- No object occludes or shadows anything. See section 3.
- No scattering by furniture. The module's `Scattering` words stay per surface.
- Below 125 Hz the furniture area is held at the 125 Hz band, as the modal solver holds its
  surface rows.
- The room's own walls stay rigid-surface rows. A wall built from layers (brick, plaster,
  paint) is not modelled: for a rigid wall the outermost finish sets the absorption, and for a
  light wall the measured absorption of the whole assembly is the honest input.

## 10. Phases

| Phase | Content | State |
| --- | --- | --- |
| 1 | Furniture table, scene field, `MediumAbsorption`, the three consumers, tier 1 tests | Done |
| 2 | Preset keys and `furnishings` JSON, the layered panel with its furnishing and patch lists | Done |
| 3 | Coverings as materials, finish rows, `starter_scene()` as what a fresh slot loads | Done |
| 4 | Tier 2 comparison against Motus and BRAS CR2, the built room's loss, the module README and known issues | Done from 500 Hz up; below 500 Hz the two rooms disagree |

## 11. Open items

- **The built room's loss, decided 2026-09-23.** Tabulated coefficients under-predict a built
  room's absorption. Measured minus modelled mean absorption, 125 Hz to 4 kHz, is +0.007, +0.020,
  +0.018, +0.018, +0.021 and +0.018 in the empty Motus room and +0.062, +0.046, +0.030, +0.027,
  +0.030 and +0.012 in the empty BRAS CR2 seminar room (146 m3, T30 from its ten dodecahedron
  responses, `tests/fixtures/bras_cr2_reference.csv`). The BRAS CR1 reverberation chamber, sealed
  and purpose-built, shows no excess: its fitted concrete absorption is 0.009 to 0.019, below the
  database's initial estimate. The excess therefore comes from what a built room carries (doors,
  windows, radiators, ducts, seams, leakage), not from the surface materials, and it is a scene
  property, `RoomScene::built_room_loss`, adding `K_BUILT_ROOM_LOSS` = 0.020, 0.025, 0.020, 0.020,
  0.020 and 0.015 to every face; the starter scene sets it, a bare scene does not. With it the
  model reads 0.96 to 1.04 of Motus and 1.14 to 1.21 of CR2 from 500 Hz to 2 kHz.
- **Below 500 Hz the two rooms disagree.** Motus asks for +0.007 at 125 Hz and CR2 for +0.062,
  so the model reads 0.75 of Motus and 2.1 of CR2 there. CR2's large windows, radiators and
  ribbed ceiling are the likely low-frequency absorbers, but that is not measured. Closing it
  wants a third room, or a low-frequency term tied to lightweight elements rather than to area.
- Whether the panel should read out the room's decay per band. It needs a read on `RoomHost`,
  a public interface, so it is a separate decision.
