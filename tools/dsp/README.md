# tools/dsp

Design-time scripts. Nothing here runs in CI or at build time; their outputs are committed.

- `design_halfband.py`, regenerates `modules/foundation/math/include/SushiDSP/foundation/math/halfband_coefficients.hpp`.
  Needs `numpy` and `scipy` (`pip install numpy scipy`). Run from the repo root:
  `python tools/dsp/design_halfband.py > modules/foundation/math/include/SushiDSP/foundation/math/halfband_coefficients.hpp`
  (write the file without a BOM). The band edges and stopband targets live in the script's
  `STAGES` table; the rationale is spec D6a.3 and the D6a plan's "Rulings" section.
- `design_resampler.py`, regenerates `modules/foundation/math/include/SushiDSP/foundation/math/resampler_design.hpp`, the six
  constants that describe the prototype's shape, and `modules/foundation/math/source/resampler_kernel.hpp`, the
  Kaiser-windowed-sinc table the arbitrary-ratio resampler polyphase-interpolates against.
  Needs `numpy` and `scipy` (`pip install numpy scipy`). Run from the repo root:
  `python tools/dsp/design_resampler.py`
  (it writes both files itself, without a BOM). `HALF_WIDTH`, `PHASES`, `CUTOFF` and `BETA` live at the top
  of the script; the design targets are the DR1+DR2 plan's N1 and the spec's "New shared
  brick" section, and `HALF_WIDTH`'s value (34, not the originally planned 32) is fixed by the
  DR1+DR2 plan's Task 1.1 controller ruling on the stopband conflict.
- `convert_hrtf.py`, converts a measured HRTF database into `assets/hrtf/sadie2_ku100_48k.hrtf`,
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
- `motus_reference.py`, computes octave-band T20, T30, EDT, DRR and C50 from Motus spherical-harmonic
  room impulse responses (Zenodo 4923187, CC-BY-4.0) and writes `tests/fixtures/motus_reference.csv`.
  Needs `numpy` and `scipy` (`pip install numpy scipy`). Run from the repo root:
  `python tools/dsp/motus_reference.py --rir 0001_1_sh_rirs.wav 0002_1_sh_rirs.wav 0003_1_sh_rirs.wav
  --csv tests/fixtures/motus_reference.csv`. The RIRs are three members of the 16.1 GB `sh_rirs.zip`;
  they are not in the repository. The bands, filter, noise truncation and every fit range are
  listed below under `motus_reference.py`, and `tests/fixtures/motus_reference.README.md` gives the
  provenance and the citation. Design intent: `docs/design/ROOM_FURNISHINGS.md` section 8.

## convert_hrtf.py

The reference input is SADIE II, University of York, Neumann KU100 subject, 48 kHz. Nothing in
the C++ tree reads SOFA or HDF5. The script runs once on a developer's machine, and the module
reads only the file it writes. The byte layout is fixed by the DE plan's Task 2 and by
`docs/archive/superpowers/specs/2026-09-08-room-acoustics-design.md` section 9.3.

Each direction is stored as a minimum-phase pair plus a separate interaural delay, so the
loader can blend neighbouring directions without comb filtering. The licence and citation block
written into the asset README is copied verbatim out of the downloaded database's own
documentation. With no such text in the download the script refuses to write the block.

In `rigid_sphere_magnitude` the series' polar axis points along the wave's travel, so the ear
facing the source is at Legendre argument -cos(incidence). That is the illuminated face, +6 dB
at high mu.

## design_halfband.py

The halfband structure is enforced exactly after the equiripple design. Every even-offset tap
other than the centre is set to 0 and the centre tap to 0.5, then the remaining taps are
rescaled so the DC gain is exactly 1. The response is measured again after that enforcement, so
the numbers printed in the header are the committed filter's, not the raw `remez` output's.

## design_resampler.py

The prototype is a Kaiser-windowed sinc, table-driven for polyphase interpolation. It is
normalised so that every polyphase branch has unity DC gain; the peak is not what is
normalised.

`HALF_WIDTH` is 34 zero crossings each side, not the plan's 32. 33 clears -100 dB with only
0.08 dB of margin at the true worst frequency, and 34 gives about 0.45 dB. The ruling on the
stopband conflict is Task 1.1 of
`docs/archive/superpowers/plans/2026-09-05-dr1-dr2-resampler-and-dd3.md`.

In `design`, the last table entry (t equal to `HALF_WIDTH`) sits at the Kaiser window's tapered
edge and is already negligible. It is the interpolator's outermost table point, so it is forced
to the exact zero the interface promises instead of being left at a near-zero float.

In `measure_response`, `freqz`'s raw DC gain is the tap count density, about `PHASES`. The
interpolating filter's own gain is that response divided by `PHASES`, which matches the
continuous-time integral it applies.

The brief names no numeric passband-ripple target, so the ripple is measured and printed but
not gated.

## motus_reference.py

The input is a Motus RIR (Zenodo 4923187, CC-BY-4.0: Gotz, Falcon Perez, Schlecht, Pulkki, I3DA
2021, doi 10.5281/zenodo.4923187), a 25-channel fourth-order ambisonic WAV in ACN/SN3D order.
Channel 0 is the omnidirectional W, and the other channels are ignored. Every figure comes from
that one channel, so no decoder or loudspeaker model enters the result.

`python tools/dsp/motus_reference.py --rir 0001_1_sh_rirs.wav` runs one response without
writing a CSV.

The parameters are all fixed.

| Parameter | Value |
| --- | --- |
| Bands | Octave bands centred on 125, 250, 500, 1000, 2000 and 4000 Hz, edges at fc / sqrt(2) and fc * sqrt(2) |
| Filter | Fourth-order Butterworth band-pass, applied forward and backward with `sosfiltfilt`, so the effective order is eight and the phase is zero. The zero phase leaves the direct sound smeared symmetrically about its true time, about 1 / bandwidth wide: about 11 ms at 125 Hz, about 0.4 ms at 4 kHz |
| Onset | The sample of the largest absolute value of the unfiltered W channel, taken as the direct sound. Every band uses that one index, so the bands share one time origin |
| Noise | The mean of the squared band signal over the last 10 percent of the response |
| Truncation | The smoothed squared band signal (a centred 20 ms moving average) is followed from the onset; the first sample at or below the noise level ends the integration. Later samples are dropped from every sum below |
| Decay curve | Schroeder backward integral of the squared band signal from the onset to the truncation, in dB relative to its start |
| T20, T30 | Least-squares line through the decay curve between its first crossing of -5 dB and its first crossing of -25 dB (T20) or -35 dB (T30); the time for a 60 dB fall on that line. A value is left empty when the curve never reaches the lower level before the truncation |
| EDT | The same fit between 0 dB and -10 dB, as a 60 dB fall |
| DRR | Energy from 2.5 ms before to 2.5 ms after the onset, against all energy from 2.5 ms after the onset to the truncation, in dB |
| C50 | Energy from 0 to 50 ms after the onset against energy from 50 ms to the truncation, in dB |

Nothing here is a fit of decay plus noise as the paper describes. The truncated Schroeder curve
is the simpler method, and the two differ where the noise floor is close to the decay.
`bras_reference.py` puts every file through the same band analysis.
