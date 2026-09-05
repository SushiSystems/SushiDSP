# tools/dsp

Design-time scripts. Nothing here runs in CI or at build time; their outputs are committed.

- `design_halfband.py` — regenerates `include/SushiDSP/math/HalfbandCoefficients.hpp`.
  Needs `numpy` and `scipy` (`pip install numpy scipy`). Run from the repo root:
  `python tools/dsp/design_halfband.py > include/SushiDSP/math/HalfbandCoefficients.hpp`
  (write the file without a BOM). The band edges and stopband targets live in the script's
  `STAGES` table; the rationale is spec D6a.3 and the D6a plan's "Rulings" section.
- `design_resampler.py` — regenerates `include/SushiDSP/math/ResamplerCoefficients.hpp`, the
  Kaiser-windowed-sinc prototype filter table the arbitrary-ratio resampler polyphase-interpolates
  against. Needs `numpy` and `scipy` (`pip install numpy scipy`). Run from the repo root:
  `python tools/dsp/design_resampler.py > include/SushiDSP/math/ResamplerCoefficients.hpp`
  (write the file without a BOM). `HALF_WIDTH`, `PHASES`, `CUTOFF` and `BETA` live at the top
  of the script; the design targets are the DR1+DR2 plan's N1 and the spec's "New shared
  brick" section, and `HALF_WIDTH`'s value (34, not the originally planned 32) is fixed by the
  DR1+DR2 plan's Task 1.1 controller ruling on the stopband conflict.
