# tests/foundation/circuits/stages

## `test_long_tailed_pair.cpp`

### `LongTailedPairDrivesItsTwoPlatesInOppositeDirections`

`// Matched amplitudes are not asserted.`

Whether the two amplitudes come out matched is not asserted: each grid sits behind its own
source and leak, and dossier question 6 leaves the pair's DC point open.

## `test_output_transformer_stage.cpp`

### `OutputTransformerStageSecondaryCarriesNoMean`

`// A transformer's secondary passes no mean.`

A push-pull pair driven through asymmetric clipping delivers a square whose two halves are not
equally long, and a saturating core compresses the taller half more than the shorter, so the
saturated waveform carries a mean even after the rolloff has removed the square's own. A
transformer's secondary sees d(flux)/dt and passes no mean at all (DA5 spec section 14).

Stimulus: a 220 Hz square of amplitude 2.2 at 40 % duty, two seconds at 48 kHz, the mean taken
over the second second. With the saturation as the last element the stage read -0.063500
(2026-09-07); with the secondary's mean removed it reads +0.000005.

## `test_power_supply_sag_stage.cpp`

### `Small_PowerSupplySagStageRecovery.GainRecoversAlongTheEnvelopeAfterSilence`

`// After the loud run the envelope is 1 - a^L with a = 1 - c; silence multiplies it by a^S, and the next unit sample steps it to a e + c.`

With zero input the sag envelope decays by a = 1 - c per sample, so a subsequent unity-input
sample reads the gain at the decayed envelope; the closed form is the oracle.
