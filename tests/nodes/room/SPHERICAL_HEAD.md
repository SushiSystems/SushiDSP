# Spherical head tests

## `test_spherical_head_hrtf.cpp`

The set is built once for the process and takes 2.1 s in a debug build. Each suite builds it in
`SetUpTestSuite`, so no case is charged for it.

### `MatchesWoodworthsInterauralDelay`

`// (a / c) * (theta + sin theta) with a = 0.0875 m and c = 343 m/s.`

a / c = 0.00025510 s; at 30, 60 and 90 degrees theta + sin theta is 1.0235988, 1.9132230 and
2.5707963, which gives 0.0002611, 0.0004881 and 0.0006558 s. Each is held to half a unit of its
seventh decimal.

### `FarEarIsShadowedByFourDecibelsInTheFourKilohertzOctave`

`// The far ear sits at the sphere's bright spot.`

With the ears on the axis, the far ear sits at the sphere's bright spot, so the level difference
at 90 degrees is the near ear's own rise: 4.7 dB in the 4 kHz octave, and it grows from octave
to octave. The test holds 4 dB, the 125 Hz octave within 2 dB, and the growth in each octave
from 250 Hz to 8 kHz.

### `EveryCellOfTheTenDegreeGridHasADirectionInTheSet`

`// A ring of 36 azimuths at every 10 degrees from -80 to 80, and the two poles.`

The grid is worked out in the test, 36 * 17 + 2 = 614 cells, and each cell must have a direction of the
set within sixteen float roundings of an angle as large as pi. A missing wedge fails here even when no
seeded direction lands in it.

### `EverySeededDirectionLiesWithinHalfAWideCellDiagonalOfAGridDirection`

`// cos d = cos(5 degrees)^2 at the centre of the widest cell.`

The widest cell is at the equator, 10 degrees by 10 degrees, so its centre is 5 degrees from each side and
the great circle to a corner is d = acos(cos(5 degrees)^2) = 7.06 degrees. Cells away from the equator are
narrower, and the cap between the 80 degree ring and the pole is at most 5 degrees across. The 500 seeded
directions must each sit within that angle of a grid direction, measured from the set's own directions.

### `SphericalHeadHrtfHearsTheSameDiffuseFieldAtBothEars`

`// The grid mirrors about the median plane.`

The ears sit on the sphere's axis and the grid mirrors about the median plane, so the two power
averages are the same sum in a different order. The two responses agree to the rounding of a sum
over the 614 directions.

### `DiffusePowerAtDirectCurrentIsTheSpheresOne`

`// The sphere's mean diffuse power is 1.8472, +2.67 dB.`

By Legendre orthogonality the sphere's power averaged over every direction is sum_m (2m+1) /
(mu^4 |h_m'(mu)|^2): 1 at DC, 1.7765 at 4 kHz, rising to 2 at high mu. Its mean over 0 to 48 kHz
is 1.8472, +2.67 dB; the 10-degree grid weighted by solid angle gives 1.8477, and 128 taps hold
all of it to 1e-10. The tests hold 2e-3 at DC, 0.02 at 4 kHz and 0.01 on the energy, which are
characterised figures.
