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
- `convert_hrtf.py` — converts a measured HRTF database into `assets/hrtf/sadie2_ku100_48k.hrtf`,
  the flat binary `SushiDSP::HrtfSetLoader` reads. Needs `numpy` and `scipy`, plus `netCDF4` or
  `h5py` for a SOFA input (`pip install numpy scipy netCDF4`); a WAV release needs neither. Run
  from the repo root: `python tools/dsp/convert_hrtf.py --input <SOFA file or WAV directory>
  --output assets/hrtf/sadie2_ku100_48k.hrtf --readme assets/hrtf/README.md`. It also rewrites
  the licence block in `assets/hrtf/README.md` from the download's own documents, and refuses to
  write one when the download carries no licence or citation text. `--self-test <path>` writes a
  four-direction rigid-sphere fixture and verifies the round trip with no database present; that
  file is the loader test's fixture. The byte layout and the −50 dB truncation and 3 dB decoder
  bounds are the room acoustics spec's §9.3 and §9.5, and `assets/hrtf/README.md` says how to
  reproduce the asset.
