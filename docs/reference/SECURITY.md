# Security Policy

We take the security of SushiDSP seriously. This document explains how to
report a vulnerability and what to expect in return.

## Reporting a vulnerability

**Please do not open a public GitHub issue for a security problem.** Public
disclosure before a fix is available puts every user at risk.

Instead, report it privately through one of:

- **Email (preferred):** **mustafagarip@sushisystems.io**
- GitHub's **private vulnerability reporting** ("Report a vulnerability" under
  the repository's Security tab).

### What to include

A good report lets us reproduce and assess quickly:

- A clear description of the issue and its impact.
- The affected component (e.g. the rig graph, a math kernel, the SDL2 device
  seam) and the version / commit hash.
- Step-by-step reproduction, ideally with a minimal proof of concept.
- The platform and build configuration (`SUSHIDSP_BUILD_SDL2` on/off, build
  type).
- Any suggested mitigation, if you have one.

## What to expect

- **Acknowledgement** within **72 hours**.
- An initial assessment and severity classification within **7 days**.
- Regular updates while we work on a fix, and credit in the release notes once a
  fix ships (unless you ask to remain anonymous).

Please give us a reasonable window to release a fix before any public
disclosure. We are happy to coordinate timing with you.

## Scope and threat model

A few properties are intentional design decisions rather than vulnerabilities —
please keep them in mind:

- **SushiDSP is a library, not a sandbox.** It runs inside whatever process
  links it — a standalone host, a VST3/CLAP plugin, or a game. A host or plugin
  wrapper choosing to trust arbitrary rig-preset or cabinet-IR files it loads is
  a host-level decision, not a defect here. We are interested in defects
  *inside* SushiDSP itself: memory-safety bugs in the rig graph, the math
  kernels, or the WDF/circuit solvers.
- **The real-time audio path has a hard allocation-free/lock-free contract**
  (spec §0.2). A violation of that contract inside `IAudioNode::process()` or
  anything it calls is a defect we want to hear about — it is the kind of bug
  that turns into audio dropouts or, worse, a data race, in a real DAW session.
- **No SushiRuntime, no SYCL, no GPU.** SushiDSP's `sushi_dsp` core links only
  the C++17 standard library and SIMD intrinsics; there is no separate compute
  backbone to report issues against. The only external surface is
  `sushi_dsp_sdl2` (the SDL2 device backend, `SUSHIDSP_BUILD_SDL2=ON`, PRIVATE
  to that one target) and, later, the CLAP/VST3 SDK inside the plugin wrapper.

In scope, and especially valued:

- Memory-safety bugs anywhere in `include/SushiDSP`/`src/` (the rig graph, the
  buffer pool, any math kernel, the WDF/Newton-Raphson solver core).
- A real-time-path violation: an allocation, a lock, a syscall, or a thrown
  exception reachable from `IAudioNode::process()`.
- Parsing/deserialization flaws in any rig-preset or cabinet-IR format once
  those exist (none do yet — the format is `{descriptor.id, {param_id →
  value}}`, spec §7.2).
- A data race between the control thread and the audio thread around the rig
  graph's compile/swap or the `SpscRing<T>` command path.

## Supported versions

SushiDSP is pre-1.0 and under active development. Security fixes are applied to
the latest `main`; there is no long-term support branch yet. Always test against
the current `main` before reporting.

## Contact

For anything that isn't sensitive, a GitHub issue or Discussion is fine. For
security matters, use the private channels above or email
**mustafagarip@sushisystems.io**.
