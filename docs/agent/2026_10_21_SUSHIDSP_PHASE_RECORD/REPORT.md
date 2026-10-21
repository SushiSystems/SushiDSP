# SushiDSP: the phase record

**Status:** Shipped — 2026-10-21

The dates and commits of the phases of [the SushiDSP design](../../design/SUSHIDSP/README.md),
which took them out when it took the design format.

## Decisions

- The repository decision (SushiDSP stands alone) was the owner's on 2026-08-25.
- The WDF adaptor tree was deferred indefinitely by the D5b amendment, user-confirmed 2026-08-26;
  the decision table is in `docs/archive/superpowers/specs/2026-08-26-aaa-audio-quality-design.md`.

## DS

| Phase | Landed | Commits |
| --- | --- | --- |
| DS0 | 2026-09-07 | `394a605` |
| DS1 | 2026-09-07 | `99796e6`, `bc774fa`, `02a9a1a` |
| DS2 | 2026-09-07 | `39c1a02`, `18eb040` |
| DS3 | 2026-09-07 | `394a605` (attribution, see `docs/reference/KNOWN_ISSUES.md`) |
| DS4 | 2026-09-07 | `af6a9ed`, `da342fc`, `afa0627` |

## DT

| Phase | Landed | Commits |
| --- | --- | --- |
| DT0 | 2026-09-08 | `247cd35` |
| DT1 | 2026-09-08 | `a4a696a`, `9076224` |
| DT2 | 2026-09-08 | `2c0d616`, `58a2f43`, `2e3c789`, `87d976c`, `6c809f4` |
| DT3 | 2026-09-08 | `dbc2706`, `32daeaa`, `f43224b` |
| DT4 | 2026-09-08 | `3d625ca`, `ce4b638`, `8085575` |
| DT5 | 2026-09-08 | `4d36860`, `f240e98` |
| DT6 | 2026-09-08 | `3a9faf1`, `55503b0`, `21cfcf7`, `16ac806` |

## DE

| Phase | Landed | Commits |
| --- | --- | --- |
| DE0 | 2026-09-08 and 09 | `f150623`, `bd34d8d`, `8e37b08`, `9915597`, `4e0d855`, `80f5c84`, `43eb93f` |
| DE1 | 2026-09-09 | `54aa3b5`, `1faae17`, `853081d`, `b077a47` |
| DE2 | 2026-09-09 | `268ba6b`, `cff30df`, `8c95dde`, `d20aa95` |
| DE3 | 2026-09-09 | `3394fe4`, `a18aca8`, `5ce46b1`, `1b72f81`, `60b8941`, `ba33504` |
| DE4 | 2026-09-08 and 09 | `3878cc1`, `ba86613`, `95954af` |
| DE5 | 2026-09-08 and 09 | `29e22d3`, `725b8a3`, `f2ae692`, `d38754f`, `549f07a`, `bfac578`, `11aeff0` |
| DE6 | 2026-09-09 | `db55eba` |
| DE7 | not opened | the JCM800 with a room measured 0.893 ms per 256-frame block (`78a798f`) |

DE8 is deferred. DS5 waits until a user asks for it.
