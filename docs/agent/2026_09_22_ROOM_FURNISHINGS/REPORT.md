# Report T0: absorption rows for the room module

**Status:** T0 delivered 2026-09-22, no code touched. Written by a research subagent, saved here
by the orchestrator, who did not re-derive its numbers. Task order is in `PLAN.md`, the design in
[`docs/design/ROOM_FURNISHINGS.md`](../../design/ROOM_FURNISHINGS.md).

Furniture: 9 rows asked, all have values (1 measured, 3 tabulated, 3 derived, 2 with both a
measured and a derived variant); the desk and the bookcase are weak. Finishes: 9 asked, 8 have
values, the kilim is not found.

## How to read the tables

- Bands are the octaves 125, 250, 500, 1k, 2k, 4k Hz. Where a source gives one-third octaves,
  the octave value is the arithmetic mean of the three thirds around it (ISO 20189, EN 12354-6
  section 4.2). Octave 125 is the mean of 100, 125 and 160 Hz.
- Furniture rows are an equivalent absorption area in m2. "Per item" is per piece. "Per m2
  covered" is an absorption coefficient applied to the wall or window area the curtain covers,
  not to the fabric area.
- Finish rows are absorption coefficients alpha.
- Kind: MEASURED (ISO 354, JIS A 1409 or an equivalent test on the object), TABULATED (a
  published table, quoted), DERIVED (computed by a stated formula from stated inputs).
- Sources S1 to S14 are listed at the end.

## Furniture

| Id | Row | Unit | 125 | 250 | 500 | 1k | 2k | 4k | Kind | Source |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| F1 | Single bed, spring mattress, drawers, no bedding, chamber centre | per item | 1.68 | 2.06 | 2.72 | 2.88 | 2.98 | 2.80 | MEASURED | S4, Fig. 1 curve B1 |
| F2 | Double bed with mattress | per item | 2.05 | 2.51 | 3.32 | 3.51 | 3.64 | 3.42 | DERIVED | S4 via F1 |
| F3 | Wooden desk | per item | 0.81 | 0.81 | 0.81 | 0.81 | 0.81 | 0.81 | DERIVED | S3, Formula 4 |
| F4 | Closed wooden wardrobe, door type, empty, one 0.59 x 0.515 x 1.88 m module | per module | 1.22 | 0.31 | 0.26 | 0.14 | 0.22 | 0.18 | MEASURED | S4, Fig. 3 curve T11 |
| F4b | Same size, drawer type, empty | per module | 1.06 | 0.43 | 0.35 | 0.22 | 0.29 | 0.34 | MEASURED | S4, Fig. 4 curve T21 |
| F5 | Heavy curtains, velour 18 oz/yd2 draped to half area | per m2 covered | 0.14 | 0.35 | 0.55 | 0.72 | 0.70 | 0.65 | TABULATED | S1, p. 585 |
| F5b | Heavy curtains, alternative, woven 0.4 kg/m2 folded 1:3 | per m2 covered | 0.10 | 0.40 | 0.70 | 0.90 | 0.95 | 1.00 | TABULATED | S3, Table B.1 |
| F6 | Light curtains, under 0.2 kg/m2 | per m2 covered | 0.05 | 0.06 | 0.09 | 0.12 | 0.18 | 0.22 | TABULATED | S3, Table B.1 |
| F7a | Two-seat sofa A, textile on PU foam (Sitland Cell 128) | per item | 1.58 | 1.85 | 2.55 | 2.87 | 2.73 | 2.64 | MEASURED | S5 |
| F7b | Two-seat sofa B, upholstered modular (Offecct Lucy) | per item | 2.6 | 3.5 | 3.4 | 3.6 | 3.2 | 3.8 | MEASURED | S6 |
| F7c | Two-seat sofa, suggested default, mean of F7a and F7b | per item | 2.09 | 2.67 | 2.97 | 3.24 | 2.96 | 3.22 | DERIVED | S5, S6 |
| F7d | Leather-look sofa, 2.7 m | per item | 1.05 | 1.59 | 1.67 | 1.34 | 0.92 | 0.57 | MEASURED | S4, Fig. 2 curve S1 |
| F8 | Upholstered chair, padded office or theatre | per item | 0.10 | 0.20 | 0.25 | 0.30 | 0.35 | 0.35 | TABULATED | S3, Table C.1 |
| F9 | Bookcase filled with books | per item | 0.57 | 0.57 | 0.57 | 0.57 | 0.57 | 0.57 | DERIVED | S3, Formula 4 |
| F10a | Person in a group, typical minimum | per person | 0.05 | 0.10 | 0.20 | 0.35 | 0.50 | 0.65 | TABULATED | S3, Table C.1 |
| F10b | Person seated, typical maximum | per person | 0.12 | 0.45 | 0.80 | 0.90 | 0.95 | 1.00 | TABULATED | S3, Table C.1 |
| F10c | Person standing, typical maximum | per person | 0.12 | 0.45 | 0.80 | 1.20 | 1.30 | 1.40 | TABULATED | S3, Table C.1 |
| F10d | Person "in a room", suggested default | per person | 0.19 | 0.28 | 0.37 | 0.46 | 0.46 | 0.37 | TABULATED | S2 |

Row notes:

- **F1.** Bed 0.99 x 2.00 x 0.23 m, mattress 0.99 x 1.95 x 0.17 m. Digitised from the figure's
  vector curve, read error about 0.03 m2. Against a wall (B2): 1.74, 2.01, 2.53, 2.59, 2.56, 2.46.
  Without mattress (B4): 1.46, 0.65, 0.56, 0.57, 0.69, 0.89.
- **F2.** `F2 = k x F1`, `k = 2(1.6 + 2.0) / 2(0.99 + 1.95) = 1.22`, an assumed 1.6 x 2.0 m
  mattress against the measured one. The perimeter ratio is chosen because diffraction at the
  edge is what a small object gains; the area ratio, 1.66, would give 4.78 m2 at 1 kHz, more than
  the whole furniture set of the 27 m3 concrete bedroom in S7 (4.27 m2 at 800 Hz). A `k` up to
  about 1.4 still fits. No bedding, pillows or duvet was measured on any bed found.
- **F3.** `A = V^(2/3)`, `V = 1.4 x 0.7 x 0.75 = 0.735 m3`. Probably an upper bound: for the F4
  cabinet the formula gives 0.69 m2, the measurement 0.14 to 0.35 m2 from 500 Hz up. The Korean
  desk step (S8) gives 0.0131 m2 per m3 of room, so 0.3 to 0.5 m2 at 25 to 40 m3.
- **F4.** Particle board 15 mm, six shelves. The 125 Hz value is a panel resonance of this
  cabinet (mean of 2.13, 1.06 and 0.47 at 100, 125 and 160 Hz); treat it as one cabinet, not a
  rule. Scaling larger wardrobes per module is unverified. Against a wall (T12): 1.07, 0.45, 0.39,
  0.31, 0.19, 0.01.
- **F5 against F5b.** F5b is 29 percent lower at 125 Hz and 27 to 54 percent higher from 500 Hz
  up. A PTB measurement of a gathered curtain 15 cm from the wall gives 0.15, 0.71, 0.98, 1.05,
  1.10, 1.11 (S10). Wall distance and fold ratio decide; F5 is the low end. The 1 kHz values of
  F5b and F10b are ambiguous in the OCR text of S3.
- **F6.** The minimum of its range; S11 lists 0.2 kg/m2 curtains 90 mm from the wall at 0.05,
  0.06, 0.39, 0.63, 0.70, 0.73.
- **F7.** The spread between sofas is the product, not an error.
- **F8.** A padded office chair; no measured domestic armchair found, so a lower bound.
- **F9.** The weakest row: Formula 4 is for hard objects, books are not hard.

## Finishes

| Id | Row | 125 | 250 | 500 | 1k | 2k | 4k | Kind | Source |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| W1 | Smooth plaster on brick or concrete, painted | 0.01 | 0.01 | 0.01 | 0.02 | 0.02 | 0.03 | TABULATED | S3, Table B.1 "concrete, plastered brick"; S13 |
| W2 | Painted brick | 0.01 | 0.01 | 0.02 | 0.02 | 0.02 | 0.03 | TABULATED | S2 |
| W3 | Glazed ceramic tile | 0.01 | 0.01 | 0.01 | 0.01 | 0.02 | 0.02 | TABULATED | S2 |
| W4a | Parquet or laminate on concrete, EN value | 0.02 | 0.03 | 0.04 | 0.05 | 0.05 | 0.06 | TABULATED | S3, Table B.1 |
| W4b | Parquet on concrete, Everest value | 0.04 | 0.04 | 0.07 | 0.06 | 0.06 | 0.07 | TABULATED | S2 |
| W5 | Plywood or wood panel, 3/8 in on a frame | 0.28 | 0.22 | 0.17 | 0.09 | 0.10 | 0.11 | TABULATED | S1, p. 586 |
| W6 | Double-glazed PVC window | 0.12 | 0.08 | 0.05 | 0.04 | 0.03 | 0.02 | TABULATED | S3, Table B.1 |
| W7 | Wooden interior door | 0.14 | 0.10 | 0.08 | 0.08 | 0.08 | 0.08 | TABULATED | S3, Table B.1; S13 |
| W8 | Thin fitted carpet on concrete | 0.02 | 0.03 | 0.06 | 0.15 | 0.30 | 0.40 | TABULATED | S3, Table B.1 |
| W9 | Rug (kilim) on hard floor | NOT FOUND | | | | | | | |

Row notes:

- **W1.** Unpainted smooth plaster on tile or brick (S1, p. 586): 0.013, 0.015, 0.02, 0.03, 0.04,
  0.05. Painted plaster on masonry (S12): 0.02 in every band.
- **W4.** Floating laminate on foam and parquet on battens are resonant: S3 parquet on joists
  0.12, 0.10, 0.06, 0.05, 0.05, 0.06.
- **W5.** Mounting decides: wood on a rigid backing (via S13) is 0.04, 0.04, 0.03, 0.03, 0.03,
  0.02.
- **W6.** The shipped "Glass, ordinary window" row is 3 times W6 at 125 Hz.
- **W7.** Flat interior doors are often hollow-core, 0.30, 0.25, 0.15, 0.10, 0.10, 0.07 (S12).
- **W8.** An ISO 354 carpet tile (S14): 0.00, 0.00, 0.05, 0.15, 0.30, 0.45.

## Room-level cross-check

All three studies use `A = 0.161 V (1/T_after - 1/T_before)`.

**Pettersson et al. (S7), 27 m3 concrete bedroom.** Furniture: double bed with extra pillows, two
bedside tables, a display case, two chairs, built-in wardrobes with clothes. Equivalent area of
the furniture per band, from the paper's tables of T20 empty and furnished:

| Band | T empty / furnished | A furniture | A / V |
| --- | --- | --- | --- |
| 100 Hz | 1.01 / 0.36 s | 7.77 m2 | 0.288 |
| 200 Hz | 1.27 / 0.34 s | 9.36 m2 | 0.347 |
| 400 Hz | 1.27 / 0.45 s | 6.24 m2 | 0.231 |
| 800 Hz | 1.46 / 0.60 s | 4.27 m2 | 0.158 |
| 1600 Hz | 1.78 / 0.60 s | 4.80 m2 | 0.178 |

At 25 to 27 m3 the Schroeder frequency is about 220 Hz; below it the area is a modal damping
figure, not an object area. From 400 Hz up the results are usable.

**Korean apartment study (S8, section 3.2).** One mean time per step, no room volume, no
per-band times. Band-averaged, per m3 of room: desk 0.0131, bed 0.0657, sofa step one 0.0650,
sofa step two 0.0811, all four 0.2249. At 30 m3 that is about 0.4, 2.0, 2.0, 2.4 and 6.8 m2. A
mean of band times is not the time of the mean absorption, so read these as orders of magnitude.

**Masovic and Oguc (S9, Table 2).** Single number, 400 to 1250 Hz, furnished minus unfurnished
rooms: up to 25 m3 6.5 m2, 25 to 50 m3 7.9 m2, 50 to 100 m3 13.2 m2; 0.20 to 0.30 m2 per m3.

**Do the rows agree with the totals?** Yes from 400 Hz up, no below 250 Hz in a small room. A
27 m3 bedroom built from F2, three F4 modules, F3 at 0.81 and one F8 sums to 6.62, 4.45, 5.16,
5.04, 5.46 and 5.12 m2 per band, 0.19 to 0.25 per m3, inside the measured 0.16 to 0.35 spread.
Against the concrete room it is 17 percent high at 800 Hz, 14 percent high at 1600 Hz and 17
percent low at 400 Hz. At 100 and 200 Hz the rows are 15 and 52 percent low; the room is below
its Schroeder frequency there, so this does not show the rows are wrong, but the module cannot
reproduce it. With the low desk value the sum drops by 0.4 m2 per band and stays inside the
spread; the data cannot choose between the two desk values. Toda's own check (S4, Fig. 5): the
summed single-item measurements equal the four items placed together up to 1.6 kHz.

## What was not found

- A measured desk per item, household or office.
- A measured bookcase per item with books.
- A measured double bed with bedding; the only bed found is the single bed without bedding.
- A kilim or any flat-woven rug on a hard floor under ISO 354. W8 is an analogue and probably an
  upper bound, which is an inference, not a measurement.
- A measured domestic armchair.
- The Korean bedroom's volume and per-band times.
- Diaz and Pedrero, Applied Acoustics 66 (2005) 945-956, and Building Acoustics 14 (2007),
  doi 10.1260/095745607781497894: paywalled.
- Everest and Pohlmann, 5th ed. 2009, Appendix: the book is lend-only. The 4th edition (2001) was
  read in full text and confirms F5, the unpainted variant of W1, W5, the carpet rows, the glass
  row and the five rows shipped from Everest. W2, W3, W4b and F10d rest on a website reprint (S2)
  whose other rows match the 4th edition.
- EN 12354-6 Annexes B and C in English: paywalled. The tables come from the identical Russian
  adoption GOST R EN 12354-6-2012 as OCR text. W1, W6 and W7 are corroborated by S13. W4a, W8,
  F5b, F6, F8 and F10a to c are single-source OCR; check F5b and F10b at 1 kHz against the
  standard before they ship.
- Cox and D'Antonio Appendix A and Vorlander, Auralization, appendix: not opened. S11 and S12
  serve only as cross-checks.

### Side finding

`modules/nodes/room/src/Materials.cpp` cites Everest for "Concrete, unpainted" with 0.01, 0.01,
0.02, 0.02, 0.02, 0.03. The 4th edition appendix gives 0.01, 0.01, 0.015, 0.02, 0.02, 0.02 for a
concrete floor. The other five shipped rows match the book. Not edited.

## Sources

Read on 2026-09-22.

- **S1** F. A. Everest, The Master Handbook of Acoustics, 4th ed., McGraw-Hill 2001. Appendix
  pp. 585-587; Table 9-2, p. 203.
  https://www.burosch.de/images/PDF/AudioPDF/the-master-handbook-of-acoustics.pdf
- **S2** Reprint of the Everest table in the 5th-edition layout (brick, tile, parquet, people in
  a room). https://www.asiproaudio.com/acoustic_IOI/101_13.htm
- **S3** EN 12354-6:2003. Formula 4 and section 4.2 from the English preview
  https://cdn.standards.iteh.ai/samples/3841/b8f48ee303d64756a67c82938c32e58a/SIST-EN-12354-6-2004.pdf;
  Annex B Table B.1 and Annex C Table C.1 from the Russian adoption GOST R EN 12354-6-2012, OCR
  text, http://stroysvoimirukami.ru/gost-r-en-12354-6-2012/
- **S4** Y. Tsuchiya, "Sound absorption characteristics of furniture and examples of their use"
  (Japanese), Toda Corporation Technical Research Report, JIS A 1409 chamber, year not printed.
  https://www.toda.co.jp/lucubration/pdf/v615.pdf. F1, F4, F4b and F7d are read from vector
  curves and meaned per octave; check: the summed items at 500 Hz read 5.03 m2, the total curve
  reads 5.03.
- **S5** Z Lab Srl, report 054-2018-CR, two-seater sofa Cell 128, Sitland, UNI EN ISO 354:2003.
  https://www.sitland.com/site/assets/files/4417/054-2018-cr_eng.pdf
- **S6** Offecct, Lucy sofa 2-seater, ISO 354 and SS 25269, report 18-106-M8.
  https://flokkplatformprod.blob.core.windows.net/catalog/offecct-lucy/Offecct-absorption-area-Lucy-sofa-2-seater_EN.pdf
- **S7** M. Pettersson, F. Ljunggren, C. Simmons, Building and Environment 223 (2022) 109480,
  Appendix A. https://www.sciencedirect.com/science/article/pii/S0360132322007119
- **S8** J. H. Jeong, B. K. Lee, J. Y. Jeon, Trans. Korean Soc. Noise Vib. Eng. 25(4) (2015)
  291-298. https://doi.org/10.5050/ksnve.2015.25.4.291
- **S9** D. Masovic, M. Oguc, Telfor Journal 5(2) (2013) 145-150.
  https://journal.telfor.rs/published/vol5no2/vol5no2_a12.pdf
- **S10** Acoustic Index, PTB reference measurements (ISO 354).
  https://acousticindex.com/en/sound-absorption-coefficient-database
- **S11** Pyroomacoustics materials database (compiled, no primary source per row).
  https://pyroomacoustics.readthedocs.io/en/stable/pyroomacoustics.materials.database.html
- **S12** akustik.ua absorption data sheet (compiled).
  https://www.acoustic.ua/st/web_absorption_data_eng.pdf
- **S13** LogAcustica, sound absorption coefficients with a source per row.
  https://logacustica.com/en/sound-absorption-coefficients/
- **S14** TFI Aachen report 23-000748-02, carpet tile, EN ISO 354.
