# RW-4 Choose how the alias floor closes

The row of this entry is in `docs/backlog/GENERAL.md`. This file holds what did not fit the row.

- State: decision
- Done when: the owner has chosen 16× oversampling, a band limit between the stages that lets 8× stay, or a higher-order antiderivative, and the amplifiers meet their bounds
- Where: `modules/foundation/circuits/include/SushiDSP/foundation/circuits/stages/antiderivative_antialiaser.hpp`, `tests/nodes/jcm800/test_jcm800/test_output.cpp`

Part 4's measurements for the three follow.

The amplifiers alias above their bounds at 8×. At a −12 dBFS input `measure::alias_floor()` reads
the JCM800 at −23.4 dB at preamp 1.0 against −33.8 dB, the 5150 at −26.8 dB against −30 dB, the IIC+
at −14.8 dB, the VH4 at −23.0 dB; the worst line is the 20 kHz tone's 19th harmonic folding to 3995
Hz. Two tests are skipped with the figure printed. Cause: the stages after the first receive an
already-clipped waveform whose content reaches the oversampled Nyquist, which breaks the assumption
first-order antiderivative antialiasing rests on; the transformer's tanh, the sag and the solver's
iterations are measured and ruled out.


One stage alone folds at −102.4 dB at 3×. 16× without the antialiaser read JCM800 −35.0, 5150
−19.9, IIC+ −14.3, VH4 −27.3 dB; 16× with it reads JCM800 −45.3 (−32.7 at preamp 1.0), 5150
−36.2, IIC+ −36.9, VH4 −35.3 dB at twice the CPU, which puts the 5150 and the IIC+ at 99 to 102 %
of the block at `-O2`.
