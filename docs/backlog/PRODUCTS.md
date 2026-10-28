# Products

## Remaining work

| ID | Pri | State | Title | Done when | Blocked by | Source | Opened |
| --- | --- | --- | --- | --- | --- | --- | --- |
| RW-40 | P2 | blocked | Calibrate the power amplifier's feedback depth | the depth that sweeps presence leaves the loop stable; `K_TAP_TO_FEEDBACK_VOLTS` 14.0 is a holding value | RW-41 |  | 2026-10-19 |
| RW-41 | P2 | blocked | Measure the Mesa Mark IIC+'s output transformer | the transformer's two corners, its core drive and both sag figures are measured | external: a bench measurement of a real chassis, or one published | `docs/agent/2026_09_20_V0_1_0_RELEASE/PLAN.md` | 2026-10-19 |
| RW-42 | P2 | blocked | Find a Diezel VH4 power-amp schematic | the phase inverter and power section are the VH4's | external: a published schematic |  | 2026-10-19 |
| RW-43 | P2 | blocked | Answer the Mark IIC+ questions only a real amplifier answers | the dossier's twelve §8 bench items are measured | external: a Mark IIC+ on a bench | `modules/nodes/mesa_mark2c/README.md` | 2026-10-19 |
| RW-44 | P2 | blocked | Read the IIC+'s effects send and return network | the network is modelled and the three skipped tests of `test_behaviour.cpp` assert again (dossier questions 19 and 20) | external: a schematic that gives the link's values |  | 2026-10-19 |

## Known issues

| ID | Origin | Symptom | Workaround | Fix | Source | Opened |
| --- | --- | --- | --- | --- | --- | --- |
