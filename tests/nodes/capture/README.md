# tests/nodes/capture

## `test_musical_clock.cpp`

### `MusicalClockPublishesTheStepTheTempoAndMeterName`

`// An eighth-note beat is exactly half the quarter.`

An eighth-note beat is half the quarter, exactly, because the halving is a division by two of a
value the double holds without loss.

### `MusicalClockPlacesAnAwkwardMeterAtAHandComputedFrame`

`// The frame sits 4999.484 into a 10480.349 frame beat.`

The eighth truly begins at frame 2955458.516, so the frame sits 4999.484 into a 10480.349 frame
beat.

### `MusicalClockKeepsBarStartsWithinOneSampleFarDownTheTimeline`

`// Every start derives from the absolute beat index.`

The step is rounded to the nearest 2^-32 of a frame and every start is derived from the absolute
beat index, so the gap against the ideal start never grows with the bar number.

## `test_take_library.cpp`

### `TakeLibraryHoldsATakeATimelineStillInTheRetireListNames`

`// A retired timeline still names the take.`

The first timeline is retired but the audio thread's watermark has not passed it, so both
timelines still name the take and the take must survive.

## `test_track_player.cpp`

### `TrackPlayerPlaysAClipAtItsStartFrame`

`// The impulse sits at the clip's fourth frame.`

The impulse sits at the clip's fourth frame, so it lands three frames past the clip's start
frame in the block that holds it.

### `TrackPlayerFadesAClipEdge`

`// A clip cut inside its take carries the ramp.`

The clip starts 1000 frames into the take, so its head is a cut and carries the ramp; a clip
covering a whole take is cut at neither end and stays flat, which is what keeps the single-take
cases above sample for sample.

### `TrackPlayerAtHalfSpeedDropsAnOctave`

`// 24000 frames hold 220 cycles of 440 Hz.`

Half speed halves 880 Hz to 440 Hz; 24000 frames at 48 kHz hold 220 cycles of it, and a cycle
crosses zero twice.

### `TrackPlayerReclaimsLanesBeforeCollectingTheLibrary`

`// The first timeline is alive, so its take is held.`

The audio thread's watermark still names the generation the first timeline was published at, so
that timeline is alive and its take is not collectable.

## `test_vari_rate_reader.cpp`

### `VariRateReaderDoesNotDriftFarDownTheTake`

`// At half speed 256 outputs cost 128 take frames.`

At half speed the step is exactly 0.5, so 256 outputs cost exactly 128 take frames once the
kernel has filled. An accumulator that loses the fraction across a block boundary shows up here
and not in the first bar.
