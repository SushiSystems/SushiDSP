# `modules/foundation/audio_format`

Hardware sample formats and device channel selection.

## Owns

- `modules/foundation/audio_format/include/SushiDSP/audio_format/asio_sample_converter.hpp`
- `modules/foundation/audio_format/include/SushiDSP/audio_format/channel_selection.hpp`

## Depends on

Nothing from the foundation tier.

## Notes cited from the source

A comment in this module's files states one invariant and names this file. The reasoning
behind each invariant follows, under the name of the file that cites it.

### `modules/foundation/audio_format/source/asio_sample_converter.cpp`

`float_to_asio_sample` scales an Int16 sample by `K_INT16_FULL_SCALE - 1.0f` in float and an Int32
sample by the same expression in double. At 2^31 a float's ULP is 256, so `K_INT32_FULL_SCALE - 1.0f`
rounds back to `K_INT32_FULL_SCALE` itself, and casting the product to `int32_t` overflows. Promoting
to double first keeps the subtraction exact at `INT32_MAX`, which the cast then truncates
correctly.
