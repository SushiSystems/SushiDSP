# KI-2 The IIC+ and the 5150 leave little headroom at the smallest device buffer

The row of this entry is in `docs/backlog/AMPLIFIERS.md`. This file holds what did not fit the row.

- Origin: own
- Where: `tests/nodes/mesa_mark2c/benchmark_mesa_mark2c.cpp`
- Cause: the valve count solved at 8×: six triodes, a long-tailed pair and four power tubes
- Recognise: the IIC+ costs 7236 ns a sample and the 5150 6286, against the JCM800's 2587; a 256-frame block costs the IIC+ 1.852 ms of a 1.5 ms budget, and a chain with it underruns at the smallest USB buffer on a slow block
- Workaround: use a larger device buffer
- Fix: RW-5
