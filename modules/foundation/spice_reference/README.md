# spice_reference

Internal module: the loaders and the comparison steps for the LTspice reference corpus. Its headers
are never installed, and no product links it.

It depends on `math`, for the biquad that the comparison band-limits with. The same-tier edge is
listed in `cmake/SushiDSPLayers.cmake`.

## Entry points

| File | What it holds |
| --- | --- |
| `spice_csv.hpp` | Loaders for the CSVs `tools/spice/export.py` writes |
| `spice_block_compare.hpp` | The steps every block test applies to an LTspice reference before comparing |

## spice_csv.hpp

`split_csv_line` strips carriage returns so that `std::stod` never sees one. They appear when a
file authored on Windows is read in text mode elsewhere.

`load_ac_csv` and `load_transient_csv` skip the single header line, blank lines and any line
with fewer than four fields. A missing or unreadable file is not an error. It yields an empty
result, which lets a test that needs the corpus skip cleanly instead of aborting.

`load_transient_csv` derives the sample rate as `1 / (t[1] - t[0])` and leaves it at 0 when
fewer than two rows are present or the first two time stamps coincide.

## spice_block_compare.hpp

Each DK block test finds its CSV, band-limits both signals the same way and reads a magnitude
off a stepped sweep. Without this header three test files would carry three copies of a
fourth-order Butterworth.

`band_limit` applies the same filter shape and cutoff to the reference and to the circuit under
test, so the band-limiting cannot flatter either signal. It removes the top octave, where
LTspice's variable-step integrator and the fixed-step trapezoidal rule of `DkCircuit` are known
to disagree (D6b plan, Risks:
`docs/archive/superpowers/plans/2026-08-26-d6b-dk-circuit-core.md`).
