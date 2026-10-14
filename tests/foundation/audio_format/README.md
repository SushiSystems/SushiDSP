# tests/foundation/audio_format

## `test_asio_sample_converter.cpp`

### `AsioSampleConverterFloatToSampleClampsOutOfRangeInput`

`// Out-of-range input clamps to full scale.`

A value outside [-1, 1] (e.g. from a hot upstream signal) must not overflow the integer
container - it clamps to the format's full-scale value instead.
