# `tools/spice`

`export.py` regenerates the CSV reference data in `docs/reference/spice/<block>/` from the
committed `.asc` schematics. It is a developer tool, not part of the build.

**CI never runs this script and never needs LTspice.** The tests read only the committed
CSVs; running this is how those CSVs get created or refreshed.

## Requirements

- Python 3 with `numpy`. Nothing else -- the rest is standard library.
- LTspice, from
  <https://www.analog.com/en/resources/design-tools-and-calculators/ltspice-simulator.html>.
  The default path is `%LOCALAPPDATA%\Programs\ADI\LTspice\LTspice.exe`; override it with
  `--ltspice`.

## Usage

```
python tools/spice/export.py --all
python tools/spice/export.py jcm_v1a jcm_fmv
python tools/spice/export.py --all --ltspice "C:\Program Files\LTC\LTspiceXVII\XVIIx64.exe"
```

Block names are `ts_clip`, `ts_tone`, `jcm_v1a`, `jcm_v1b_cold`, `jcm_cf`, `jcm_fmv`,
`jcm_ltp`, `jcm_pp_ot_nfb`.

## What it does per block

1. Copies `<block>.asc` into a temporary directory twice, alongside a copy of
   `docs/reference/spice/models/` so the schematics' `.include ../models/*.sub` still
   resolves.
2. In the first copy it turns the sheet's `;ac dec 40 10 20k` comment into a live
   `!.ac dec 40 10 20k` directive; in the second it does the same for
   `;tran 0 40m 20m 1u`. Exactly one analysis is live per run.
3. Runs `LTspice.exe -b -ascii <file>` on each.
4. Parses the ASCII `.raw` output (`Flags:`, `Variables:`, `Values:`), splitting it into one
   dataset per `.step` run wherever the independent variable stops increasing.
5. Writes, next to the schematic:
   - `<block>_ac.csv`: `frequency_hz,setting,magnitude_db,phase_degrees`, 200 log-spaced
     points from 10 Hz to 20 kHz per setting, interpolated from the raw sweep (phase is
     unwrapped before interpolation).
   - `<block>_tran.csv`: `time_s,setting,input_v,output_v`, the final 20 ms resampled onto a
     uniform 192 kHz grid (3840 rows per setting).

`setting` is the `.step` index, 0/1/2.

## Node conventions the script relies on

- The probed output is always `V(out)`.
- The stimulus node is read out of the schematic, not guessed. Every block defines its
  source as `V1 <node> 0 SINE(...) AC 1`, and `find_stimulus_node` takes `<node>` from that
  line and selects `V(<node>)` as the `input_v` column. Renaming a node in a schematic
  therefore carries the exporter with it, and a raw file that does not contain that node
  raises rather than silently exporting some other net.
