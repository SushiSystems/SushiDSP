# Bundled HRTF

`sushidsp_host_gui` looks for `assets/hrtf/sadie2_ku100_48k.hrtf` next to its executable on
startup and installs it into every room it creates through `IRoomHost::set_hrtf`. The build
copies this directory beside the executable, so dropping the file here is enough.

Nothing in the C++ tree reads SOFA or HDF5. `tools/dsp/convert_hrtf.py` converts a measured
database into this flat file once, on a developer's machine, and `HrtfSetLoader` reads it with
`std::ifstream`.

## Requirements for `sadie2_ku100_48k.hrtf`

- A measured head-related transfer function set, or a set derived from one.
- Licensed under Apache License 2.0, or a licence compatible with it — the file ships inside
  the repository, so anything with a non-redistributable or non-commercial clause cannot go
  here.
- The byte layout `tools/dsp/convert_hrtf.py` writes and
  `docs/superpowers/specs/2026-09-08-room-acoustics-design.md` §9.3 fixes: magic `SDHRTF\0\0`,
  version 1, then the header's sample rate, direction count, tap count, decoder order and
  flags, then one record per direction, then the Ambisonic decoder, then the diffuse-field
  average per ear. Little-endian `float32` throughout.
- 128 taps per ear, third-order decoder, 256 taps per decoder filter. At 1550 directions that
  is 32 + 1550 × 1036 + 32 × 1024 + 2 × 512 = 1 639 624 bytes.

## Where the set comes from

**SADIE II**, University of York, Neumann KU100 dummy-head subject, 48 kHz. The database page
is <https://www.york.ac.uk/sadie-project/database.html>; the KU100 release the tool was written
against carries 1550 directions. The same project publishes Ambisonic binaural decoder filter
sets from the same head in the ACN/SN3D convention
(<https://www.york.ac.uk/sadie-project/ambidec.html>), which `--decoder-from-file` takes instead
of designing one.

MIT KEMAR (Gardner and Martin, MIT Media Lab TR 280, 1994) is the fallback if SADIE II is ever
withdrawn. The tool reads either, since both ship as SOFA and as WAV.

## Reproducing the file

The converter needs `numpy` and `scipy`, and `netCDF4` or `h5py` to read a SOFA file. A WAV
release needs neither:

    pip install numpy scipy netCDF4

Download the KU100 set, then from the repository root:

    python tools/dsp/convert_hrtf.py \
        --input <the KU100 SOFA file or its WAV directory> \
        --output assets/hrtf/sadie2_ku100_48k.hrtf \
        --readme assets/hrtf/README.md

The tool reads every measured pair, takes each one's minimum-phase part by cepstral folding,
measures its interaural delay as the difference of the two onsets at 20 dB below each channel's
peak, truncates to 128 taps and reports the discarded energy. It exits non-zero if that energy
exceeds −50 dB, which is the bound §9.3 sets. It then designs the third-order decoder by
magnitude least squares (Schörkhuber, Zaunschirm and Höldrich, DAGA 2018): plain least squares
below 2 kHz, magnitude with continued phase above it. It prints the worst third-octave
magnitude error between the decoded and the measured set from 200 Hz to 8 kHz, which is the
number §9.5's 3 dB bound is read from. Finally it rewrites the licence block below from the
download's own documents, and prints the SHA-256 of what it wrote.

A WAV release must name each file with its azimuth and elevation in degrees, the way SADIE's
own release does — `azi_15,0_ele_0,0.wav`, comma or full stop as the decimal mark. The tool
refuses a name it cannot read rather than guessing a direction.

To check a file already in the tree, compare `sha256sum` against the hash in the licence block,
and run

    python tools/dsp/convert_hrtf.py --self-test build/hrtf_fixture.hrtf

which builds four directions from the rigid-sphere model, writes them, reads them back and
exits non-zero unless every field survives the round trip. It writes 37 968 bytes and prints
its four azimuths, which are 0, π/2, π and 3π/2. That fixture is what the loader's unit test
reads, so a change to the layout fails there before it reaches this file.

## Licence and citation

`convert_hrtf.py` writes the block between the two markers below, copying the licence statement
and the citation request verbatim out of the downloaded database's own documents. It refuses to
write the block at all when the download carries neither, so nothing here is a transcription
from memory. The block is empty until someone downloads the database and runs the tool.

The set the maintainer chose is Apache License 2.0, the licence this repository already
carries, so a converted copy ships in the tree without a second licence file. The SADIE project
asks that work using the database cite Armstrong, Thresh, Murphy and Kearney, "A Perceptual
Evaluation of Individual and Non-Individual HRTFs: A Case Study of the SADIE II Database",
*Applied Sciences* 8(11), 2018 — the tool copies the project's own wording of that request
below, and that copy, not this sentence, is what ships.

<!-- convert_hrtf.py: licence block begin -->
<!-- convert_hrtf.py: licence block end -->

## Not present?

Everything builds and every test runs. The host prints one line to stderr naming the path it
looked for, `RoomNode` keeps the `SphericalHeadHrtf` its constructor built, and
`IRoomHost::hrtf_source()` returns `HrtfSource::SphericalHead`, which the room panel shows as
"spherical head — front and back sound alike". The interaural delay and level difference are
right; the pinna cues that separate front from back are not there, so a cabinet placed behind
the head sounds in front of it.
