# SDD ledger — plan: docs/superpowers/plans/2026-08-26-d5b-plugin-readiness-foundations.md
Spec: docs/superpowers/specs/2026-08-26-aaa-audio-quality-design.md (Phase D5b). BASE 6aa2989.

## Pre-flight scan
| Pair | Produces / consumes | Finding |
|---|---|---|
| T2/T4 | both edit IAudioNode.hpp, RigGraph.hpp/.cpp | sequential: T4 after T2 |
| T2/T8 | Oversampler8x::latency_samples → Jcm800::latency_samples | consistent name |
| T3/T8 | StreamingConvolution(partition, ir, len, max_block) | consistent |
| T5/T8 | RtSwapSlot<T> publish/acquire/reclaim; T8 needs has_active() — T5 brief lacks it | Ruling: T5 adds `bool has_active() const noexcept` now (cheap; avoids T8 touching core). Cost if wrong: none. |
| T6/T7 | both create/use Calibration.hpp | Ruling: T6 creates; T7 creates with identical content if absent (parallel run). |
| T4/T6,T7 | index set_parameter overload; `using IAudioNode::set_parameter` needed in nodes | T4 adds `using` to Jcm800/TubeScreamer/SineToneNode; T6/T7 keep it |
| T1,T3,T10 | all append to CMakeLists.txt + tests/CMakeLists.txt | Ruling: run in parallel in one checkout (user asked for parallel, no worktrees); each stages both CMakeLists with its own commit; an intermediate commit may reference a sibling's not-yet-committed file. Cost if wrong: a non-building intermediate commit, final tree builds. |
| T7/T9 | kMasterUnityGain 0.7 then tuned | consistent |
| T9 | re-pins snapshots once | per spec |
| T10/T8 | Ir struct — T8 creates io/Ir.hpp, T10 uses | Ruling: T10 creates io/Ir.hpp (it runs first, wave A); T8 reuses. |
Waves: A = {1,2,3,5,10} parallel; B = 4 then {6,7}; C = {8,11}; D = 9; E = 12; F = 13.

## Execution
Task 2: minor (deferred): RigGraph.hpp latency doc comment self-contradictory; doc lacks "chain only, over-counts parallel branches" caveat; no taps() test for even/sub-3 request.
Task 2: complete (commits 6aa2989..90bda4c, review clean)
Task 10: Ruling: brief's resampler cutoff min(1, in/out) was inverted; implementer used min(1, out/in) (anti-alias on downsample) — plan bug, spec intent upheld — cost if wrong: none (upsample path identical).
Task 3: Ruling: reviewer Important "commit 4f74927 not self-buildable (swept siblings' CMake/test_rt_safety lines)" — accepted as the known cost of the pre-flight parallel-in-one-checkout ruling; final tree builds; no fix dispatched. Cost if wrong: one non-bisectable commit.
Task 3: minor (deferred): test_streaming_convolution.cpp header filename line 75 cols; ctor Doxygen lacks @brief; no direct reset() test.
Task 3: complete (commits 90bda4c..4f74927, review clean)
Task 10: fix round 1/5 (3 addressed, 0 open — header width, downsample alias test, max_length doc; commits 7afd066..2841e06)
Task 10: minor (deferred): 1e-12 magnitude floor inflates gain on deep in-band notch (IrLoader.cpp ~:96); next_power_of_two uint32 overflow unreachable today; bessel_i0 per tap (load-time); IrLoader::load file path only indirectly tested.
Task 10: complete (commits 4f74927..2841e06, review clean after 1 fix round)
Task 1: minor (deferred): skip() float pow near-1 base rounding (test passes within tolerance).
Task 1: complete (commits 2841e06..784ce61, review clean)
Task 5: minor (deferred): reclaim() `!= active_` predicate dead (inherited); contract silent on slot outliving audio thread; one test subset of another. Future: unify RtGraphSlot onto RtSwapSlot<RigGraph>.
Task 5: complete (commits 784ce61..58279c5, review clean)
Task 4: Ruling: no `using IAudioNode::set_parameter;` added in T4 (reviewer confirmed C++17 hiding analysis); T6/T7 briefs carry the `using` requirement with their index overrides. Cost if wrong: an ambiguity error at compile time, not a runtime fault.
Task 4: minor (deferred): push_parameter_change false return unobserved outside tests (T12 host should log/count drops); RigController race remains until T12.
Task 4: complete (commits 58279c5..951ff31, review clean)
Task 6: Ruling: TS regression snapshot re-pinned against the brief — accepted: reviewer independently verified the old snapshot encoded the 0.1/block ramp law (0.9^10) and the new smoothing is the same affine ramp at 50.6193 ms; new test sets knobs before prepare(). Cost if wrong: a silent tone change in the TS level path (bench + THD tests unchanged argue against).
Task 7: Ruling: JCM800 regression snapshot re-pinned against the brief — accepted: the preamp slew law change (10%/block → 20 ms one-pole) makes the old block-10 pins unreachable; pre-D5b control with no ramp in flight agrees exactly. Regression-test comment mis-attributes the cause to tone smoothing — deferred minor for Task 9 (which re-pins again) to fix. Cost if wrong: a hidden preamp-path tone change.
Task 7: Ruling: tone_stack_is_dirty() semantics = |target − applied| (brief's "false after one block" inconsistent with 20 ms ramp); test asserts dirty→dirty→clean.
Task 7: minor (deferred): regression comment cause wording; tone-change test can't distinguish target-store from coefficient-write (sample-0 comparison suggested).
Task 7: complete (commits 86b8ae0..9f34075, review clean)
Task 11: complete (commits 9f34075..8a5928a, review clean; manual ASIO check not runnable headless — user to verify)
Task 6: fix round 1/5 (2 addressed, 0 open — smoothing test now discriminating, dead loop bound removed; commits 86b8ae0..3e73c8f)
Task 6: minor (deferred): duplicate range literals between clamp constants and descriptor table; smoothing-test comment numbers approximate.
Task 6: complete (commits 951ff31..86b8ae0 + 3e73c8f, review clean after 1 fix round)
Task 8: Ruling: fix round may touch RtSwapSlot.hpp (add `active_for_publisher()`) and the one-line IAudioNode::latency_samples() doc ("may change when a node's control-thread setter swaps a latency-bearing stage") — smaller than a follow-up task. Cost if wrong: none functionally.
Task 8: Ruling: set_cabinet() retaining IR samples for re-prepare — approved (RtSwapSlot has no unpublish).
Task 8: fix round 1/5 (5 addressed, 0 open — pre-prepare guard, active_for_publisher(), latency doc, max_block_, RT swap test; commits ccf7d3b..13412a2)
Task 8: minor (deferred): IAudioNode::reset() needs a thread-affinity contract (publisher thread); reclaim_cabinets() has no production caller until T12.
Task 8: complete (commits 3e73c8f..13412a2, review clean after 1 fix round)
Task 9: Ruling: reviewer Important (1) "limiter commit dcbb28c on main will move the calibration test" — invalid: dcbb28c is an ancestor of this branch and Task 8 removed that limiter; nothing pending. Cost if wrong: none.
Task 9: Ruling: kMasterUnityGain landed at 3.58 (brief guessed 2.2) — behavioural tests are the authority; accepted. Cost if wrong: default loudness only.
Task 9: minor (deferred): calibration test comment should state cabinet-free is a valid proxy (IrLoader band-normalises); missing Calibration.hpp include in test_jcm800.cpp; duplicated test body; post-clamp hard clip aliases — future soft-knee inside the last oversampled region (D6d).
Task 9: complete (commits 4884073..dcff669, review clean)
Task 13: Ruling: reviewer Important "WDF deferral provenance fabricated" — invalid: the D5b spec has a "Decisions taken (user-confirmed 2026-08-26)" table (line 56) with the WDF row (line 62), confirmed via the brainstorming Q&A; the "decision table" wording is accurate. No fix dispatched. Cost if wrong: none.
Task 13: complete (commits 3950fe0..4680afe, review clean after ruling)
Task 12: fix round 1/5 (2 addressed, 0 open — selection remap + strcmp; commits 4680afe..273b9eb)
Task 12: minor (deferred): asset copy uses CMAKE_SOURCE_DIR + configure-time EXISTS; static cabinet_input buffer keyed by slot index; executable_directory() from argv[0]; bundled IR file absent (user-supplied).
Task 12: complete (commits dcff669..3950fe0 + 273b9eb, review clean after 1 fix round)
All 13 tasks complete. Final whole-branch review dispatched on 6aa2989..273b9eb.
Final review: mergeable after 4 Important (TS latency_samples missing; smoothing test tolerance ±53%; IrLoader band-normalise + load() untested; IAudioNode::reset() thread contract) + must-fix minors (RigGraph latency doc, Calibration include, TS range literals). One fix wave dispatched.
Final review: Ruling: T5 "`!= active_` predicate dead" minor re-worded to "redundant, kept as defence in depth" per final reviewer.
Final fix wave: complete (commits 273b9eb..3037c10, re-review clean). Deferred: fixed temp filename in IrLoader WAV test; duplicated PCM16 writer; M1/M2/M4/M5/M7 of final review.
