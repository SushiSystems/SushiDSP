# Room

## Remaining work

| ID | Pri | State | Title | Done when | Blocked by | Source | Opened |
| --- | --- | --- | --- | --- | --- | --- | --- |
| RW-6 | P2 | unmeasured | Measure the loudest valid scene, then decide on a guard | the loudest scene `RoomScene::is_valid` admits is measured, and the owner has ruled whether a guard belongs in the node, the rig or neither |  |  | 2026-10-19 |
| RW-14 | P2 | open | Weight the HRTF converter's diffuse average by solid angle | `tools/dsp/convert_hrtf.py` derives each direction's solid angle from the grid it finds |  |  | 2026-10-19 |
| RW-15 | P2 | open | Close the near ear's miss above 5 kHz | a higher decoder order, or a decoder that trades the far ear for the near one, brings the near ear within 3 dB above 5 kHz |  |  | 2026-10-19 |
| RW-16 | P2 | open | Run the tail's output cascade twice under a fade | a listener or microphone move blends the retiring and arriving coefficients on one line sum, two more biquads per ear in `LateReverb` |  |  | 2026-10-19 |
| RW-17 | P2 | open | Give `BandResponseFit` an analytic Jacobian | the fit converges inside a higher cap; it stops at eight steps on about 60 % of fits and costs 4.33 ms for 150 fits against the 30 Hz republish budget |  |  | 2026-10-19 |
| RW-18 | P2 | open | Settle the built room's loss below 500 Hz | a third measured room, or a low-frequency term tied to lightweight elements, sets the 125 Hz and 250 Hz figures |  | `docs/design/ROOM/README.md` §11 | 2026-10-19 |
| RW-19 | P2 | open | Rebuild the room's line allocation for long decays | spec section 6.2 sizes the delay lines for the 12 s `RoomScene::K_MAX_DECAY_SECONDS` admits, or a measurement shows the 0.525 s lines do not thin audibly |  | `docs/design/SUSHIDSP/README.md` | 2026-10-19 |
| RW-20 | P2 | open | Let furniture shadow and reflect | a placed furnishing blocks the line of sight and has an image of its own |  | `docs/design/ROOM/README.md` §9 | 2026-10-19 |
| RW-21 | P2 | open | Give the listener one direct path per cone | each cone has its own direct path to the ears, at an HRIR pair each |  |  | 2026-10-19 |
| RW-22 | P2 | open | Transcribe the remaining four material rows | brick, acoustic foam, the steel garage door and carpet on underlay join the table with a citation each |  |  | 2026-10-19 |
| RW-23 | P2 | open | Give the SM57 its measured curve | Shure's published response is a second row of `microphone_model.cpp` with its own `MicrophoneKind` |  |  | 2026-10-19 |
| RW-25 | P2 | open | Bound the raw octave step at the seam | a bound derived from the reference's own step holds the raw step across f_s |  |  | 2026-10-19 |
| RW-26 | P2 | open | Shorten the tail's lines by the direct-delay compensation | the designer is told the compensation and the tail lands with the direct sound |  |  | 2026-10-19 |

## Known issues

| ID | Origin | Symptom | Workaround | Fix | Source | Opened |
| --- | --- | --- | --- | --- | --- | --- |
| KI-6 | own | Four furnishing and covering rows carry no measurement of their own | none | none, until a per-object measurement is published |  | 2026-10-19 |
| KI-8 | own | Four of the spec's fourteen material rows are not transcribed | pick the nearest row | RW-22 |  | 2026-10-19 |
| KI-10 | own | Modal damping at a soft boundary is about 30 % wrong | none | none, the damped eigenproblem costs what FDTD does |  | 2026-10-19 |
| KI-11 | own | The tail carries nothing below the Schroeder frequency | none needed | none, the design gives the low end to the eigenmodes |  | 2026-10-19 |
| KI-13 | own | Nothing between the node and the device bounds the output | keep a limiter after the rig | RW-6 |  | 2026-10-19 |
