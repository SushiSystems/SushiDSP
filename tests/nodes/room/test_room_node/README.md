# tests/nodes/room/test_room_node

## `test_behaviour.cpp`

### `find_peaks`

`// Prominence is measured to the deeper valley.`

The prominence is measured to the deeper of the two valleys before the spectrum climbs back
above the peak on either side.

### `lagrange_impulse`

`// Weight k lands an impulse at whole + 2 - k.`

Weight k reads the sample k - 2 frames past the integer delay, so an impulse at 0 lands at whole
+ 2 - k with that weight.

### `RoomNodePlacesTheFirstOrderPeaksWhereTheSolverSaid`

`// Nothing but taps reaches the first 1749 frames.`

The seven first-order paths of the example scene, in metres: direct sqrt(2.5^2 + 0.7^2) =
2.596151, floor sqrt(2.5^2 + 1.7^2) = 3.023243, ceiling sqrt(2.5^2 + 3.3^2) = 4.140048, the two
y walls sqrt(2.5^2 + 4^2 + 0.7^2) = 4.768648, -x sqrt(5.5^2 + 0.7^2) = 5.544367 and +x
sqrt(6.5^2 + 0.7^2) = 6.537584; times 48000 / 343 that is 363.310, 423.078, 579.365, 667.333
(twice), 775.888 and 914.881 frames. The tail's input waits for t_c = 5 \* 2.5 / 343 s = 1749
frames, so nothing but taps reaches the first 1749 frames of the response. The cabinet is a 1 cm
piston: the 4x12's directivity fit puts the ceiling image's peak, 53 degrees above the baffle's
axis, one frame late at 580, which is the filter's group delay and not the tap's placement.

### `RoomNodeCrossfadesAPublishWithoutAStep`

`// The arriving bank starts from zero after a 0.36 m move.`

The microphone moves 0.36 m, which reorders the taps and refits every filter, so the arriving
bank starts from zero. Measured through the seam's high half: a step ratio of 1.0033 and a
residual of 0.0051, -45.8 dBFS, where the taps alone measured 0.0020, -53.9 dBFS and the
two-shelf fit 1.0051 and 0.0233, -32.7 dBFS. It read -52.1 dBFS while the tail sat 12 dB under
its diffuse-field level and its output cascade was the room's alone; the cascade now follows
what the taps leave of the field, so a move steps its coefficients on the network's running
state, 3 % in the second section here. Spec 5.3 and the README's crossfade section carry the
figures.

### `RoomNodeCrossfadesASmallMoveFromTheCarriedState`

`// 78 of the 96 taps carry their state over one centimetre.`

One centimetre moves every delay by under 1.4 frames; 78 of the 96 taps refit within the match
bound and carry their state, the other 18 start from zero. Measured through the seam's high
half: a step ratio of 1.0000 and a residual of 0.0010, -59.6 dBFS, where the taps alone measured
-60.2 dBFS and the two-shelf fit carried 52 and measured 0.0160, -35.9 dBFS.

### `RoomNodeRepublishesTheSameSceneWithoutAResidual`

`// The same scene refits to the same taps: zero residual.`

The same scene refits to the same taps, every tap carries its state, and the arriving bank
renders the retiring bank's block sample for sample. Measured: a residual of exactly zero.

### `RoomNodeReclaimsARetiredSetOnlyAfterTheAudioThreadMovedOn`

`// The node never reads a slot pointer past its block.`

The node copies a set's taps into its own bank the block it first sees the set, and never reads
a slot pointer past that block, so a reclaim can free a retired set at any time without a read
from freed memory. test_rt_swap_slot.cpp proves the slot's own deferral; this case walks the
node through the same sequence and checks that the block after each reclaim still carries the
room.

`// The direct tap arrives 363 frames in.`

The direct tap arrives 363 frames in, so the room is silent for the first block and the energy
check starts at the second.

### `RoomNodeTakesTheDirectImageOutOfTheEarsBankAndLeavesTheMicrophones`

`// Tap 0 of a solved set is the direct image.`

Tap 0 of a solved set is the (0, 0, 0) image, the shortest path. For the ears the HRIR pair
renders that path, so the bank the node renders from carries reflections only and the direct tap
goes to the direct path; the microphone's direct sound is a tap and stays.

### `RoomNodeCompensatesAnEarsSetByItsRoundedPropagationDelay`

`// The ears' render moves earlier by round(d / c * fs).`

The ears' whole render moves earlier by round(d / c \* fs), 363 frames for the example scene's
363.31: tap 0 lands 0.31 frames past zero, every reflection keeps its spacing to it unless that
would put it under a frame, and the tail's input delay moves with them, floored at zero, since
the network's shortest line is latency the design cannot give back. A microphone set is left as
it was.

### `RoomNodeRingsAtTheGaragesEigenfrequencies`

`// The first six modes lie between 28.583 and 71.458 Hz.`

A unit impulse through the garage, 262144 frames at 48 kHz and a 262144-point transform, 0.183
Hz per bin. The first six modes under f_s = 278.18 Hz are (c / 2) sqrt((nx / 6)^2 + (ny / 4)^2 +
(nz / 2.5)^2) at c = 343: 28.583, 42.875, 51.529, 57.167, 68.600 and 71.458 Hz. The (0, 1, 1)
mode at 80.896 Hz bounds the search from above.

### `RoomNodeModeWidthsMatchTheirDamping`

`// A resonator's -3 dB width is 2.199 / T60 Hz.`

A resonator whose pole radius is r has a -3 dB width of (1 - r) fs / pi; with r = 10^(-3 / (T60
fs)) that is 3 ln(10) / (pi T60) = 2.199 / T60 Hz. The solver's decays give the widths the
spectrum must show within 20 %: 1.485, 0.821, 1.519, 1.485 and 0.842 Hz for the first five
modes. The sixth, (2, 1, 0) at 71.458 Hz, sits 2.86 Hz above (0, 0, 1) with widths of 1.52 and
0.84 Hz, so their -3 dB contours run into each other and the spectrum reads 3.65 Hz for the
pair; it is left out here.

### `RoomNodePressesTheBassIntoTheCorner`

`// Every mode with an odd index has a node at the centre.`

The listener stands in the corner at (0.15, 0.15, 0.15) and then at the centre (3.0, 2.0, 1.25),
where every mode with an odd index is at a node. The ears carry the taps and the tail with them,
and what those pass under 100 Hz through the LR4 pair, two octaves under f_s, moves the figure
by hundredths of a decibel: 14.06 dB with the geometric receiver held still, 14.02 dB with it in
the corner too.

### `RoomNodeIsContinuousAcrossTheSchroederFrequency`

`// Both layers carry DiffuseField's level at f_s.`

Spec 9a.4 and 9a.6: the two layers meet at f_s when each carries the level the room's diffuse
field holds there, DiffuseField, the one reference ModalSolver scales its octave to and
LateReverbDesign hands the tail the remainder of. The modal octave at one pose is that level
times the pose's mode-shape factor, which the rays have no counterpart for and which reads under
the volume average at interior poses, so the modal octave is held against the design's own
figure at the pose and the geometric octave against the reference through the high half, both
averaged over six poses as energy and each within 2 dB, in all three reference rooms. The raw
step between the octaves is recorded; it is the reference's own change across the seam, the
4x12's directivity index rising 4.6 dB between the 250 and 500 Hz bands, less the interior
deficit, and no bound of physics. Measured 2026-09-09 before the shared reference: 7.7 dB in the
garage at one pose.

### `RoomNodeSwitchesTheWaveLayerOffInADeadRoom`

`// f_s under 40 Hz switches the modal layer off.`

f_s under 40 Hz gives a design with count zero and a crossover of zero, so the node's output is
the geometric bricks alone: the early renderer's channel 0 plus the tail, sample for sample, on
both outputs, with the crossovers bypassed.

### `RoomNodePublishesNewModalGainsWithoutRebuildingAResonator`

`// A moved cabinet or listener rebuilds no resonator.`

Spec 9a.2: a moved cabinet changes psi_n at one point, a moved listener at the two ears of an
ears set, and no resonator changes frequency or damping. A dimension change rebuilds every
section.

`// 161 of the 187 modes have a non-zero x index.`

Every mode with a non-zero x index feels the cabinet's move along x, 161 of the 187; every mode
with a non-zero y index feels the listener's move along y, 150 of them.

### `RoomNodeCarriesTheStandingWavesAcrossAMovedCabinet`

`// The bank keeps its state; only the source gains ramp.`

The cabinet moves 0.2 m along x. The resonators are the same, so the bank keeps its state and
only the source gains ramp: no step, and the four blocks after the fade hold the arriving pose's
steady energy. Measured: a step ratio of 1.0000 and 0.02 dB. The residual against the blend of
the two steady renders is -29 dB and is not bounded: the resonators hold the old drive and take
their decay time to settle to the new one, as the room's own standing waves would after the
cabinet is moved.

### `RoomNodeRampsTheEarGainsAsTheOutputBlendWould`

`// The bank's output is linear in the ear gains.`

The listener moves 0.3 m along y. The resonators are the same, so the ear gains ramp, and the
bank's output is linear in those gains: ramping them under the raised cosine is the blend of the
two steady renders, sample for sample, -144 dB when nothing else moved. The same listener
carries the taps, the direct path and the tail with it, and what they pass of a 68.6 Hz tone
through the LR4 pair, two octaves under f_s, crossfades from cleared banks and rings from their
zero state after the fade: the geometric leakage's own residual. It measured -95.3 dB when the
ears heard channel 0 at unity, -88.3 dB through the HRIR pair and the decoder, and measures
-72.3 dB with the tail at its diffuse-field level, 12 dB up, its output cascade refitted for the
moved listener's taps on the network's running state; the bound follows that measurement and no
other. The step ratio reads 1 + 6e-8, float rounding of an exact blend.

### `RoomNodeRebuildsTheStandingWavesInAChangedRoom`

`// A changed room starts the spare bank from zero.`

A room 0.1 m wider designs every resonator anew, so the spare bank starts from zero under the
fade and the field takes its decay time to build: the four blocks after the fade hold -16.8 dB
of the steady energy, bounded here at a quarter.

### `RoomNodeReportsTheCrossoverTheSetSplitsAt`

`// The node reports the crossover its seam sits at.`

The garage's 187 modes fit, so the report is Eyring's 278.18 Hz as the set carries it. The 3.0 x
2.5 x 2.4 m concrete room has about 1250 modes under its Eyring figure of 877 Hz, so the solver
lowers the crossover to the 513th mode and the node reports that lowered figure, the one its
seam sits at. The dead room reports Eyring's 22 Hz with the layer off, so a panel still shows
the room's own figure.

### `RoomNodeMicrophoneListenerFillsBothChannelsIdentically`

`// The microphone's two outputs are equal bit for bit.`

The microphone is one point: its taps encode to channel 0 alone, its tail is one signal read
twice, and its standing waves are read at the microphone in both ear gains, so the two outputs
are equal bit for bit. The listener stands elsewhere, and a moved listener mid-render changes
nothing the microphone hears.

### `RoomNodeMicrophoneAtFiveCentimetresLiftsTheBottomEnd`

`// The tilt is expected at 13.68 dB against a point source.`

The cabinet is `point_cabinet()`, one 1 cm cone that never breaks up, so the seven elements of
its line of sight stand within a centimetre and the capsule hears a point. A 4x12's cones are
20 cm from a capsule at the baffle's centre, and its tilt there is under half this figure.

Spec 8: a cardioid's gradient half gains sqrt(1 + (c / (2 pi f r))^2) on a spherical wave, 14.2
dB at 125 Hz for r = 5 cm and 0.1 dB at r = 3 m. The whole chain carries 1 / r as well, 35.6 dB
between the two distances, so the figure that survives is the tilt: the 125 Hz third octave
against the 4 kHz third octave, where the boost is 0.16 dB at 5 cm and nil at 3 m, and air takes
0.08 dB more from 4 kHz over 3 m. The dead room keeps the modal layer off, so the band reaches
the taps unsplit, and the window holds the direct path alone. Expected 13.68 dB; measured 13.34
dB. The plan's 15.5 dB at 100 Hz is the model's own figure and does not reach the output: the
six-band fit is anchored at 125 Hz and its low shelf holds level below it, so the rendered 100
Hz third octave reads 13.79 dB, recorded here and not bounded.

### `RoomNodeMicrophoneHearsLessRoomThanTheEars`

`// The two ratios differ by at least 11.5 dB.`

The garage with the cabinet at (1.5, 2.0, 0.5). A cardioid 5 cm off the cone hears the direct
path at 1 / 0.05 and the nearest reflection, off the floor, at 1 / 1.0 and 140 frames later; the
ears at (3.5, 2.0, 1.2), 2.12 m away, hear the direct path at 1 / 2.12 through the HRIR pair at
the alignment delay, the propagation compensated, and the floor at 1 / 2.63, 71 frames later.
The capsule stands on the baffle's axis, between the four cones, so its direct sound comes from
cones 21 cm away and not from a point 5 cm away.

`// Between the four cones the difference measures 12.20 dB.`

12.20 dB with the cones in place. A single point at the baffle's centre would read 19.95 dB.

### `RoomNodeCarriesOneOverROnTheCabinetsAxis`

`// An omni capsule on a point source's axis reads 1 / r.`

The cabinet is `point_cabinet()`. A 4x12's impulse energy on the baffle's axis falls short of
1 / r at these distances, because above a few kilohertz its 28 elements arrive apart.

The one level in the module that comes from geometry rather than from DiffuseField: an omni
capsule on the cabinet's axis in the dead room reads 1 / r, so its direct window is 0 dB at 1 m
and falls 6.02 dB per doubling. Measured -0.30, -6.64 and -13.32 dB against 0, -6.02 and -12.04;
the shortfall is the air's, 1.3 dB over 4 m at 4 kHz. A test that scales both layers to one
reference cannot see that reference move, so this is the case that holds the module's absolute
level down.

### `RoomNodeMovingAroundTheCabinetChangesTheTone`

`// The listener faces the cabinet at both spots.`

Spec 7: the 4x12's four pistons beam at 2 kHz and are omnidirectional at 125 Hz. The ears 2 m
from the cabinet, on axis and then 60 degrees off it, in the dead room's direct-path window: the
2 kHz third octave drops by more than 6 dB and the 125 Hz third octave moves by under 1 dB, each
within 1 dB of the piston model's own band value. The listener faces the cabinet at both spots,
so the head's response is its front response both times and the difference between the two
renders is the cabinet's alone.

### `RoomNodeEarsHearTheDirectSoundThroughTheHrirPair`

`// The floor image follows at frame 1551.`

In the dead room, whose modal layer is off, the ears 2 m behind the cabinet hear nothing before
the alignment delay and then, over the direct window, the direct path alone: tap 0 read at 128 +
(279.88 - 280) frames through the renderer's kernel, its fitted filter and 1 / r, then the
spherical head's response for a source in the median plane, the same in both ears. The floor
image, 12.17 m long, follows at 1703 - 280 + 128 = 1551.

### `RoomNodeDefaultsDirectDelayToCompensated`

`// Physical timing adds round(d / c * fs) frames.`

A fresh node reports direct_delay as compensated and one decoder partition of latency; physical
adds round(d / c \* fs). The plan's 280 frames assume the listener 2 m from the cabinet, which
the dead room places; the default scene's listener stands 2.596 m away, 363.31 frames, so a
fresh node reports 363 more when physical. The microphone renders at physical timing with no
partition in its path and reports 0 either way.

### `RoomNodeDirectOnsetMovesWithTheDirectDelayChoice`

`// The choice shifts the whole render by 280 frames.`

The rendered direct-path onset, the first frame above 1e-4 of the direct window's peak, sits 280
frames later in physical mode than in compensated mode in the dead room, and the floor image's
onset, after the direct window, moves by the same 280: the choice shifts the whole render and
not the direct sound alone. The index path stores the choice for the next publish, as the
listener's does.

### `RoomNodeReproducesWoodworthsInterauralDelayOnTheFallback`

`// Woodworth's delay is (a / c)(theta + sin theta).`

Spec 9.5: a source 2 m away at 30, 60 and 90 degrees azimuth, the listener facing -x in the dead
room; the lag of the interaural cross-correlation peak over the direct window is Woodworth's (a
/ c)(theta + sin theta), 0.2611, 0.4881 and 0.6558 ms for a = 8.75 cm and c = 343 m/s, within 20
microseconds, one sample at 48 kHz. The grid holds those three azimuths, so the pair is not
interpolated and the figure is the model's own. The window closes at 900 frames, before the
nearest image, off the +x wall at 90 degrees, lands at 1154 - 280 + 128 = 1002. The cabinet
faces the listener, so the direct tap's filter is the on-axis response: 60 degrees off the axis
its 2 kHz notch weighted the correlation toward the low frequencies, where a rigid sphere's
delay runs half again Woodworth's (Kuhn, JASA 62(1), 1977), and the lag read 0.58 ms.

### `RoomNodeReportsSphericalHeadWithNoAssetAndMeasuredWithOne`

`// An empty HRTF set restores the spherical head.`

hrtf_source() is SphericalHead after prepare(); the six-direction fixture installed through
set_hrtf makes it Measured, the node keeps rendering finite audio through the fixture's pair and
decoder, and an empty set restores the fallback.

### `RoomNodeTailIsCoherentLowAndIncoherentHigh`

`// A window 1024 frames after t_c holds the tail alone.`

Spec 6.5: the interaural cross-correlation of the tail alone, measured on the ear outputs, is
above 0.8 in the 125 Hz octave and below 0.3 in the 4 kHz octave. The dead room keeps the modal
layer off and its taps end at t_c = 8397 frames, compensated by 280 and delayed by the
partition, so a window opening 1024 frames after that holds the tail alone; the room's 0.21 s
RT60 has decayed 97 dB by the window's end and the correlation weights the loud start.

### `RoomNodeDecoderMagnitudeMatchesTheMeasuredSet`

`// The case runs only with the SADIE II set present.`

Spec 9.5 as measured, only with the SADIE II set present, 24 directions at -30, 0, +30 degrees:
near ear 3 dB below 5 kHz and 6 dB above, far ear 10 (assets/hrtf/README.md).

### `RoomNodeCarriesTheRearNotchWithAMeasuredSet`

`// The rear notch is within 1 dB of the set's own.`

Spec 9.5 as measured: the ears facing away from the cabinet, the source at 180 degrees, hear the
left ear's rear notch within 1 dB of the set's own (assets/hrtf/README.md).
