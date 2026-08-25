# SushiDSP documentation

The manual's index. Start at the root [`README.md`](../README.md) if you haven't; this is for
once you're in the tree and need a specific document.

## Where to go

- **[Design](design/sushidsp.md)** — the architecture, the math, the SOLID rationale, and the
  product roadmap (D0–D12). This is the authoritative spec an implementer builds from; it
  describes intent and may describe work that does not exist in the tree yet.
- **[Contributing](CONTRIBUTING.md)** — the conventions every change is held to: C++17, Allman
  braces, the license header format, the test/CTest-label layout, and how to build (`sd`, never
  `cmake` directly).
- **[Changelog](reference/CHANGELOG.md)** — what changed and when, by roadmap phase.
- **[Code of Conduct](CODE_OF_CONDUCT.md)** and **[Security Policy](SECURITY.md)**.

## Generated API reference

`sd doxygen` builds `docs/api-site/html/` from the headers under `include/SushiDSP/`, starting
from [`docs/api/mainpage.md`](api/mainpage.md). It documents what the headers say; it is not a
substitute for the design spec's reasoning.

## Implementation record

`docs/superpowers/plans/` holds the implementation plan for each roadmap phase (D0, D1, D2, …),
each with a `- [ ]` checklist of TDD steps. They are historical execution records once a phase
merges — later phases may use different tooling or conventions, and an old plan is not updated
to match; read the design spec for the current state, not an old plan.
