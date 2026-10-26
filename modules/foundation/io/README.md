# `modules/foundation/io`

Impulse responses and WAV files.

## Owns

- `modules/foundation/io/include/SushiDSP/foundation/io/io_log_category.hpp`
- `modules/foundation/io/include/SushiDSP/foundation/io/ir.hpp`
- `modules/foundation/io/include/SushiDSP/foundation/io/ir_loader.hpp`
- `modules/foundation/io/include/SushiDSP/foundation/io/wav_file.hpp`
- `modules/foundation/io/include/SushiDSP/foundation/io/wav_writer.hpp`

## Depends on

`math` and `log` (same tier).

## Logging

The module logs under `K_IO_LOG_CATEGORY`. `IrLoader::prepare` writes one `INFO` naming both
rates when it resamples an impulse response, and one `WARN` naming the impulse response's length
and the limit when `max_length_ms` cuts its tail. A response at the target rate and inside the
limit writes nothing.

## Notes cited from the source

A comment in this module's files states one invariant and names this file. The reasoning
behind each invariant follows, under the name of the file that cites it.

## Designs

<!-- designs:begin -->
- [SUSHIDSP](../../../docs/design/SUSHIDSP/README.md): The standalone amp and pedal simulation framework: the rig graph, the DSP math library, the amplifiers and the studio, timeline and room paths.
<!-- designs:end -->
