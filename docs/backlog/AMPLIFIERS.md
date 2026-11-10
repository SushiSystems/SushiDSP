# Amplifiers

## Remaining work

| ID | Pri | State | Title | Done when | Blocked by | Source | Opened |
| --- | --- | --- | --- | --- | --- | --- | --- |
| RW-87 | P2 | open | Bring the JCM800 alias floor down to its promised -33.8 dB | The JCM800 alias floor reads at or below -33.8 dB, measured -23.4 dB |  |  | 2026-11-10 |
| RW-88 | P2 | open | Bring the Peavey 5150 alias floor down to its promised -30 dB | The Peavey 5150 alias floor reads at or below -30 dB, measured -26.8 dB |  |  | 2026-11-10 |
| RW-89 | P3 | open | Derive the gain of the Diezel VH4 power block | The 73.114 dB mix-bus gain is derived from the power stage, transformer and sag, and the characterization test goes |  |  | 2026-11-10 |

## Known issues

| ID | Origin | Symptom | Workaround | Fix | Source | Opened |
| --- | --- | --- | --- | --- | --- | --- |
| KI-2 | own | The IIC+ and the 5150 leave little headroom at the smallest device buffer | use a larger device buffer | RW-5 |  | 2026-10-19 |
