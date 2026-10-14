# tests/foundation/io

## `test_ir_loader.cpp`

### `band_mean_decibels`

`/// @brief Returns the mean magnitude in dB over a band by a direct DFT of the samples.`

Mean magnitude in dB over [low_hz, high_hz], measured by a direct DFT of the samples zero-padded
to fft_size - independent of IrLoader's own FFT and bin arithmetic.

### `IrLoaderNormalisesShapedBandAverageToZeroDb`

`// A two-tap comb makes the band selection matter.`

A two-tap comb: |1 + 0.9 e^-jwK| ripples between 0.1 and 1.9 across the 100 Hz - 5 kHz band, so
unlike a single-tap IR the band selection and the averaging rule both matter.

## `test_wav_file.cpp`

### `WavFileLoadsMonoPcm24SamplesWithSignExtension`

`// The samples cover zero, both extremes and -1 LSB.`

0x000000 = 0, 0x400000 = +0.5 (positive, bit 23 clear), 0x7FFFFF = max positive, 0xFFFFFF = -1
LSB (all-ones, negative), 0x800000 = most-negative (bit 23 set, sign extension).

### `WavFileDownmixesStereoAsAverageOfChannels`

`// Same-sign values make a sum twice the average.`

Same-sign, unequal per-channel values: sum-instead-of-average is off by a factor of \~2 here
(unlike opposite-signed values, which would let sum and average both land near zero and hide an
averaging bug under a loose tolerance).
