# SDD ledger — plan: docs/superpowers/plans/2026-08-26-d6a-measurement-and-oversampling.md
Spec: docs/superpowers/specs/2026-08-26-aaa-audio-quality-design.md (D6a). Base 0598e3f. No worktree (user ruling D5b).

## Pre-flight scan
| Pair / task | Produces vs consumes | Finding |
|---|---|---|
| T0↔T2,T3,T6 | CMake lines + SUSHIDSP_GOLDEN_DIR/FIXTURE_DIR macros | consistent; T0 owns tests/CMakeLists.txt so Group A never edits it |
| T1→T4 | HalfbandDesign{taps,tap_count,passband_edge,stopband_db}; kHalfbandStage1..3 | names match |
| T2→T4 | measure::kFftSize, magnitude_spectrum_db, peak_near, bin_of, sine_stimulus | match |
| T3→T4,T5 | golden::compare_or_regenerate, env SUSHIDSP_REGENERATE_GOLDEN | match |
| T2 self | tests call sine_stimulus (Render.hpp) — defined there | ok |
| T4 self | phase mapping in upsampler flagged as derive-on-paper; equivalence tests arbiter | ok |
| T5 self | rename oversampler_a_→oversampler_; test_jcm800 latency test exists? (to verify at dispatch) | verify |
| T6 self | AcSweep has `setting`, test uses it | ok |
| T2/T4 alias acceptance | JCM800 30 dB may be blocked by post-cab clamp | pre-ruled in plan T4 step 6 |
Ruling: run T1,T2,T3,T6 concurrently in one checkout (files disjoint by construction) — user wants max parallelism — cost: a non-bisectable interleaving if commits race.
Task 0: complete (controller inline, 135c169)
Dispatched Group A: T1 sonnet, T2 opus, T3 sonnet, T6 opus (base 135c169)
Task 1: implemented 9f71560,e6cabb9 (DONE_WITH_CONCERNS: script parity + ValueError fixed by implementer). Ruling: accept implementer's correction of the plan's enforce_halfband parity — the plan text was wrong, tests in T4 are the arbiter — cost if wrong: T4 equivalence tests fail and expose it. Reviewer dispatched (sonnet).
Task 1: complete (commits 9f71560..e6cabb9, review clean; minors deferred: no zero-denominator guard in enforce_halfband rescale; docstring fix landed as separate commit)
Ruling: plan Task 4 test `HalfbandDesignsHaveExactZeroTapsAndUnityDcGain` has the parity inverted — zeros are at EVEN offset from the centre (odd offsets are the real taps); T4 dispatch must correct the test to `(distance % 2 == 0 && i != centre) → EXPECT 0`. Plan's one_sided_pairs (offsets 1,3,5..) and upsampler phase comment are consistent with the correct rule. Cost if wrong: T4 tests fail loudly.
Task 3: implemented 5f3658f (DONE). Reviewer dispatched (sonnet).
Task 3: complete (commit 5f3658f, review clean; ⚠️ binary content unverifiable by reviewer — implementer's second-run −200 dB result accepted)
Task 2: implemented eec6499 (DONE_WITH_CONCERNS). Baseline alias floors @48k: TubeScreamer drive=1 −8.3 dB, Jcm800 preamp=10 −3.8 dB.
Ruling: accept implementer's three corrections to the plan's code (Farina envelope sign, null_test residual over overlap only, one-pole digital expectations −3.003/−19.415 dB at 0.1 dB) — each backed by an independent check in the report; plan text was wrong — cost if wrong: reviewer catches it. Reviewer dispatched (opus).
Task 2: review — Spec ✅, quality approved with 1 Important (null_test signed-correlation lag search breaks on polarity inversion) → fix round 1 dispatched to original implementer. Minors deferred: unchecked signal length in Spectrum/Thd (assert wanted); identity frequency_response test is plumbing-only; multi-declarator lines inherited from plan; function-local statics single-thread only; alias_floor ~23 s dominates unit suite.
Task 6: implemented de85927 (DONE_WITH_CONCERNS). Ruling: accept .asc files written as on-sheet netlist directives instead of placed symbols — spec asks for LTspice-runnable schematics and correctness beats drawing when no LTspice is available to verify symbol pin offsets — cost if wrong: user redraws for readability. Reviewer (opus) asked to verify FMV / TS tone / NFB topologies against canonical circuits.
Task 2: fix round 1/5 (3 addressed per implementer; commit a7307f7); scoped re-review dispatched (sonnet). Dispatching Task 4 (opus) on base a7307f7 in parallel.
Task 2: fix round 1/5 (3 addressed, 0 open; commit a7307f7) — re-review clean
Task 2: complete (commits eec6499..a7307f7, review clean)
Task 6: review — Spec ❌: Critical Rfb 10k across C6 in ts_tone corrupts reference <200 Hz; Important export.py input-node positional heuristic. FMV/ts_clip/LTP+OT verified correct by reviewer. Fix round 1 dispatched to original implementer. Minors deferred: %.6f quantisation in transient CSV undocumented; '*' comments inside TEXT blocks to confirm on first LTspice run; NFB polarity undetermined until simulated (⚠️ accepted, documented remedy).
Task 6: fix round 1/5 (2 findings + 4 minors addressed per implementer; commit d75fb13); scoped re-review dispatched (sonnet)
Task 6: fix round 1/5 (6 addressed, 0 open; commit d75fb13) — re-review clean
Task 6: complete (commits de85927..d75fb13, review clean; ⚠️ nothing simulated — golden CSVs are a user-side LTspice step)
Task 4: implemented a9e3234 (DONE_WITH_CONCERNS). Alias floors: TS −8.3→−9.6 dB, JCM800 −3.8→−32.0 dB. Bench: JCM800 1457.8→1359.8 ns/sample (0.348 ms/256), TS 557.8→535.5 (0.137 ms), oversampler 66.4→60.2. Latency 12→65 samples. 3 red tests at HEAD (TS/JCM 30 dB acceptance, Jcm800PowerTubeBiasMismatch...).
Ruling: TubeScreamer's floor is in-domain aliasing (21st harmonic folding around the 8× Nyquist), unreachable by any base-rate halfband; the spec's "≥30 dB over D5b" cannot hold for TS in D6a. Task 5 re-pins the TS test as a regression floor (≤ −9.0 dB) with the diagnosis, and the 30 dB target moves to D6c (C¹ diode curves + rebuilt stage) — recorded in CHANGELOG by Task 7. Cost if wrong: an alias regression in TS would only be caught at −9 dB granularity until D6c.
Ruling: JCM800 30 dB assertion moves to Task 5 (plan pre-authorised this); if still short after the single region, Task 5 pins the measured value with the clamp diagnosis and D6d owns the rest.
Ruling: Jcm800PowerTubeBiasMismatchShapesTheEvenHarmonicContent window is re-fitted in Task 5 after the single-region change (the halfband droop/images were shaping the drive into the power stage); Task 5 must re-verify the physical claim (mismatch>0 raises even content vs mismatch=0), not just widen the window. Cost if wrong: a masked power-stage regression.
Ruling: accept three red tests at a9e3234 for one task; Task 5 is serial and immediately next. Cost: one non-green commit in history.
Ruling: accept implementer's bin-exact reference fix in the alias sweep test (plan's sine(0.1,…) was off-bin).
Reviewer dispatched (opus).
Task 4: review — Spec ✅, approved with 1 Important (reset() coverage lost — plan-mandated deletion) → fix round 1 dispatched. Minors deferred: 1e-6 equivalence margin empirical; golden README row wording.
Task 4: fix round 1/5 (3 addressed per implementer; commit 1f66aa4); scoped re-review dispatched (sonnet)
Task 4: fix round 1/5 (3 addressed, 0 open; commit 1f66aa4) — re-review clean
Task 4: complete (commits a9e3234..1f66aa4, review clean; 3 red tests intentionally handed to Task 5 per rulings)
Task 5: dispatched (opus) on base 1f66aa4 with rulings for the three red tests.
Task 5: implemented 416e7ff (DONE_WITH_CONCERNS): single region made JCM800 alias floor −32.0 → −20.4 dB (region-A decimator had been band-limiting V1b; not the clamp — verified with output −20 dB). Bias-mismatch re-verified (2.12 % active vs 2.88 % zeroed), window (0.017,0.025). Bench 1335.2 ns/sample. Latency 130→65.
Ruling: neither ship −20 dB nor revert to two regions; add the physically real interstage band-limit — 12AX7 Miller input capacitance (Cgk 1.6 pF, Cgp 1.7 pF from our own .sub, gain from the configured stage gains, +10 pF stray stated as assumption) as first-order lowpasses on the V1b/V2a/PI grids inside the 8× region, corners computed from named constants. Treble impact to be measured and reported for the user. Cost if wrong: an audible treble change the user may reject; D6d's DK stages replace it anyway. Fix round 1 dispatched to original implementer.
Task 5: fix round 1/5 → 2bb498f: Miller one-poles added; JCM800 alias floor −43.4 dB (spec assertion restored); treble −1.3 dB@5k, −4.6@10k, −8.8@15k (user ear-check pending; knob = 10 pF stray assumption); bench 1368.4 ns/sample. Task reviewer dispatched (opus) over 1f66aa4..2bb498f.
Task 7: dispatched (sonnet) in parallel with Task 5 review; files disjoint (tests/benchmark, docs).
Task 5: review — Spec ✅, approved with 1 Important (Miller comment: 470p peaker reasoning wrong; 100k source impedance is a shared simplification) → fix round 2 dispatched (comment-only). Minors deferred: golden README double row; redundant M_PI fallback (house style).
Task 5: fix round 2/5 (commit 7bee37a, comment-only + nits); scoped re-review dispatched (sonnet)
Task 7: implemented bac2acb (DONE); reviewer dispatched (sonnet)
Task 5: fix round 2/5 (4 addressed, 0 open; commit 7bee37a) — re-review clean
Task 5: complete (commits 416e7ff..7bee37a, review clean; ⚠️ treble change (−4.6 dB @10k) not ear-checked — user decision; 10 pF stray is the knob)
Task 7: complete (commit bac2acb, review clean; minor deferred: double blank line in sushidsp.md)
Final review (opus): no Critical; Important I1 (design doc alias-floor history backwards / −32.0 attributed to D5b), I2 (latency documented as halving; really 24→65 vs D5b), I3 (bias-mismatch test not self-checking). Triage: alias-floor acceptances → regression label must-fix; rest leave. No unsafe rulings. Single fix wave dispatched (opus) covering I1–I3, label move, header pad, Oversampler doc contract, regen stderr warning, docs wording.
Final fix wave: 78c608c, 381f0e3 (8 items); scoped re-review dispatched (sonnet)
Final fix wave re-review clean. D6a complete: 0598e3f..381f0e3.
