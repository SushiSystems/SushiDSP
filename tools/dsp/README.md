# tools/dsp

Design-time scripts. Nothing here runs in CI or at build time; their outputs are committed.

- `design_halfband.py` — regenerates `include/SushiDSP/math/HalfbandCoefficients.hpp`.
  Needs `numpy` and `scipy` (`pip install numpy scipy`). Run from the repo root:
  `python tools/dsp/design_halfband.py > include/SushiDSP/math/HalfbandCoefficients.hpp`
  (write the file without a BOM). The band edges and stopband targets live in the script's
  `STAGES` table; the rationale is spec D6a.3 and the D6a plan's "Rulings" section.
