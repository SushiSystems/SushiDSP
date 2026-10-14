# bras_cr2_reference.csv

Measured T30 of a second real room: the CR2 seminar room at RWTH Aachen, 145 m3, empty, with a
linoleum floor, a painted ribbed concrete ceiling, plaster and concrete walls and 9.75 m2 of
windows. It comes from the Benchmark for Room Acoustical Simulation (BRAS, Aspöck, Brinkmann,
Ackermann, Weinzierl and Vorländer, TU Berlin DepositOnce, doi 10.14279/depositonce-6726.3,
licence CC-BY-SA-4.0). The BRAS model's surface areas are 34.93 m2 plaster, 56.94 m2 concrete,
9.75 m2 windows, 49.29 m2 floor and 51.62 m2 ceiling, total 202.53 m2, and its volume is
146.1 m3 (`3_surface_descriptions.zip`, `mat_CR2.txt`).

One row per octave band: the mean T30 over the ten dodecahedron responses
(`CR2_RIR_LS{1,2}_MP{1..5}_Dodecahedron.wav`), their standard deviation and the smallest
dynamic range above the noise any of them had in that band. Each response is analysed with the
method of `tools/dsp/motus_reference.py`, whose docstring states every parameter.

To reproduce, fetch `1_scene_descriptions-CR2.zip` (616 MB) from the DepositOnce item, extract
`RIRs/wav/*Dodecahedron.wav` and run `python tools/dsp/bras_reference.py --room CR2 --rir
CR2_RIR_LS*_MP*_Dodecahedron.wav --csv tests/nodes/room/fixtures/built_room_loss/bras_cr2_reference.csv`. The script needs
`numpy` and `scipy`.
