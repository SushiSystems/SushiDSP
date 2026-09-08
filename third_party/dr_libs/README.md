# dr_libs

`dr_mp3.h`, taken verbatim from tag `mp3-0.7.3` of
[mackron/dr_libs](https://github.com/mackron/dr_libs), fetched from
`https://raw.githubusercontent.com/mackron/dr_libs/mp3-0.7.3/dr_mp3.h`.

It is one header with no build system and no transitive dependency, so it is vendored as a
file rather than fetched: the checkout compiles it, nothing configures it.

## Who uses it

`apps/host_gui` only, through `apps/host_gui/Mp3Decoder.cpp`, which is the single translation
unit that defines `DR_MP3_IMPLEMENTATION`. The core (`include/SushiDSP`, `src/`) and every
product module stay on the standard library alone, and `WavFile` remains the tree's only
decoder — the host converts an mp3 to a WAV before anything downstream sees it.

## Licence

Public domain (Unlicense) or MIT No Attribution, the user's choice; `LICENSE.txt` carries both
texts as they appear at the foot of the header. The decoder is derived from
[lieff/minimp3](https://github.com/lieff/minimp3), released under CC0.

## Updating

Fetch the new tag's `dr_mp3.h` over this one, refresh `LICENSE.txt` from its footer, and change
the tag named above. The header is never edited by hand; a local fix belongs in
`Mp3Decoder.cpp`.
