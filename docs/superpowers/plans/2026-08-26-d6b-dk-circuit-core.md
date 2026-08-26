# D6b — DK-Method Circuit Core Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** One generic, allocation-free, compile-time-sized nonlinear circuit solver — `DkCircuit<S, N, I, O>` — plus the three device models (`Triode12AX7`, `PentodeEL34`, `SiliconDiode`) that D6c and D6d instantiate, validated against analytic circuits, against the existing `DiodeClipperCircuit`, and against the LTspice reference corpus D6a committed.

**Architecture:** The solver splits into three headers under `include/SushiDSP/math/dk/`, each with one job: `DenseLu.hpp` (fixed-size LU with partial pivoting, no allocation), `DampedNewtonSolver.hpp` (the N-dimensional damped Newton iteration plus its convergence counters), and `DkCircuit.hpp` (the discretised state-space object, its matrix derivation, and the per-sample step). `DkNetlist.hpp` is the control-thread builder that turns component values into the incidence matrices; it is a plain struct of fixed-size arrays that `DkCircuit` owns, so re-derivation after a potentiometer moves needs no allocation and no reference back to the builder. Device models live in `include/SushiDSP/circuits/models/`, are header-only, stateless and free of any circuit knowledge: each is a pure `current`/`jacobian` pair in `double`. Nothing in D6b touches a product module — `Jcm800` and `TubeScreamer` are rebuilt in D6c/D6d.

**Tech Stack:** C++17 header-only (`std::array`, `<cmath>`), GoogleTest (existing `sushidsp_tests` binary), the `tests/measure/` library and `tests/measure/SpiceCsv.hpp` from D6a, `tests/benchmark/` harness.

**Spec:** `docs/superpowers/specs/2026-08-26-aaa-audio-quality-design.md` — Phase D6b (§D6b.1–D6b.4), with §D6c/§D6d read as the consumer contract and the "Acceptance summary" / "Risks" rows for Newton convergence and SPICE band-limiting.

## Global Constraints

- Build/test only through the `sd` CLI: `sd build`, `sd test` (`-s unit|regression|benchmark|all`, `-t release`). Never invoke cmake/ctest directly.
- No git worktrees; work directly in the `main` checkout (user ruling, D5b). Commit only the files your task names; never `git add -A`.
- C++17 only. Allman braces. Types `PascalCase`, functions/variables `snake_case`, members trailing `_`, constants `kPascalCase` as the tree already does (`kOversamplingFactor`, `kExponentLimit`). No abbreviations — `saturation_current`, not `is`; `thermal_voltage`, not `vt`. Local mathematical shorthands inside a single expression are acceptable only when the surrounding comment states the equation they transcribe.
- Every `.hpp`/`.cpp` (tests included) carries the 76-column boxed license header: copy it **verbatim** from `include/SushiDSP/core/PortLayout.hpp` and change only the filename line, padded so the closing `*/` stays in column 76. In the code blocks below `/* LICENSE HEADER — copy from PortLayout.hpp, filename line changed */` stands for that block.
- Doxygen `@brief`/`@param`/`@return`/`@tparam` on every public function, struct and template parameter, including test-side helpers.
- Comments only where the *why* is non-obvious. Every equation transcribed from this plan carries a one-line comment naming the equation (e.g. `// x[n] = A x[n-1] + B u + C i  (D6b plan, State update)`).
- RT safety: `DkCircuit::process_sample`, `DkCircuit::update_matrices_if_dirty`, the Newton solver and every model call must be allocation-free, lock-free, syscall-free and throw-free. Task 8 adds the `*PerformsNoHeapAllocation` traps; the existing traps must stay green after every task.
- Tests: `TEST(Unit_Dsp, …)` naming; golden-vector expectations — an analytic closed form, a value hand-computed in this plan, or an independent implementation. Never the implementation's own output.
- **Double inside, float at the boundary.** Every model, every matrix, every state and every solver iterate is `double`. The `float` cast happens only where a future `IAudioNode` reads or writes a sample buffer, which is D6c/D6d code, not D6b code.
- **No CSVs exist yet.** `docs/reference/spice/` holds schematics and `.subckt` models but no generated data (LTspice is not installed; D6a Ruling 3). Every test that consumes a CSV must `GTEST_SKIP() << "…"` with a message naming the missing path and the command that generates it, and must pass its assertions when the file is present. A skip is never a silent pass: the message is mandatory.
- `_USE_MATH_DEFINES` before any include in a `.cpp` that uses `M_PI`, with the `#ifndef M_PI` fallback, as every existing file does.
- Numbers that bind this phase: Newton residual tolerance 1e-9 relative, max 8 iterations, ≤ 4 damping halvings, ≤ 0.01 % non-converged samples over the stimulus set, zero NaN/inf; diode-clipper agreement with `DiodeClipperCircuit` ≤ 1e-6; single 12AX7 stage null vs SPICE ≤ −60 dB band-limited to 10 kHz; benchmark 2-nonlinearity / 4-state circuit at 8× ≤ 0.12 ms per 256-frame block at 48 kHz.
- Model parameters are the ones in `docs/reference/spice/models/*.sub`, **exactly**: 12AX7 μ = 100, kp = 600, kvb = 300, kg1 = 1060, kx = 1.4; Norman grid Ig0 = 1 µA, Vγ = 0.5 V, Vt_g = 0.15 V; EL34 μ = 11, kp = 60, kvb = 24, kg1 = 650, kg2 = 4500, kx = 1.35, same grid law; 1N914 Is = 2.52 nA, n = 1.752. A model whose constants drift from its `.sub` makes every SPICE null meaningless.

### Rulings taken while writing this plan (spec deviations and choices the spec left open)

1. **`DkCircuit` is a class template with `std::array` storage, not a runtime-sized struct.** The spec offers `template <int S, int N, int I, int O>`; this plan keeps it and uses `std::size_t` template parameters. Rationale: every instantiation in D6c/D6d has fixed dimensions known at compile time, the LU and the Newton loop unroll against them, and there is no path by which a heap allocation can appear. Node count, however, is *not* a template parameter (see Ruling 2), so the type name stays the four-parameter one the spec and the brief pin.
2. **Node count is a fixed internal bound, not a fifth template parameter.** `kMaximumNodes = 16`, `kMaximumResistors = 32`, `kMaximumPotentiometers = 4`, with the actual counts held as runtime members bounded by them. Storage is 16×16 `double` scratch ≈ 2 KB per instance — irrelevant next to a partitioned convolution. Alternative (a fifth template parameter) was rejected because it forces every consumer to count internal nodes, which are an implementation detail of the netlist, and it would break the `DkCircuit<S, N, I, O>` signature the brief pins. If a D6d block exceeds 16 nodes, raise the constant and re-run the benchmark; the `static_assert` in `DkNetlist::finalise` names the constant.
3. **Potentiometers are handled by re-deriving the DK matrices, not by adding Newton branches.** The spec §D6b.1 sketches "a pot enters as an extra nonlinear branch with a linear current law". That contradicts the spec's own dimensions in §D6c/§D6d (`DkCircuit<2,0,1,1>` for the TS tone stack, `DkCircuit<3,0,1,1>` for the FMV, both of which *have* pots) and it makes every knob cost a Newton row on every sample. This plan instead keeps the pot's two sections in the resistor table and re-runs the full derivation on the audio thread, allocation-free, at control-rate cadence: a per-pot dirty flag, drained by `update_matrices_if_dirty()` which the owning node calls once per block after its `SmoothedParameter`s update. Worst case (13×13 augmented system, block 5 of D6d) is roughly 4 k flops for the Gauss–Jordan inverse plus ~6 k for the eight matrix products — on the order of 3 µs, once per block, only when a knob actually moved, against a 1.5 ms budget. The Woodbury/Schur alternative is faster and much harder to get right; Priority 1 wins. Task 8's benchmark measures a knob-moving block explicitly so this ruling is backed by a number rather than by this paragraph.
4. **The multivariate solver does not live in `NewtonRaphsonSolver.hpp`.** The spec says that class "gains the `N`-dimensional variant". It is not given one. The scalar class is a stateless, header-only, zero-storage utility; the multivariate solver owns LU scratch, damping state and convergence counters, and has a different lifetime and a different test surface. Merging them would put mutable per-instance state into a class whose entire contract today is "static functions". `NewtonRaphsonSolver.hpp` is left **untouched** and `include/SushiDSP/math/dk/DampedNewtonSolver.hpp` is new. The scalar `N = 1` fast path the spec asks for is provided by `DampedNewtonSolver<1>` specialising the LU away (a single division), not by routing back to the old class.
5. **`DiodeClipperCircuit` is re-expressed, not deleted, and its header comment is corrected.** The spec says the "WDF" claim in the comment is wrong (the class has always been a scalar Newton solve on a nodal equation) and asks for the DK re-expression to prove the core. This plan fixes the comment and adds `DkCircuit<0,1,1,1>` **agreement** tests in Task 6; the existing class keeps its own implementation and its own tests so the two are genuinely independent oracles. Deleting it would leave the DK version comparing against nothing. Its removal is a D6c decision, once `TubeScreamer` no longer references it.
6. **A triode contributes two nonlinear branches, a pentode three.** Spec §D6d writes "nonlin: 1" for V1a. That count is only right if grid conduction is ignored, which is exactly the defect D6b exists to fix. `Triode12AX7` is a two-port nonlinearity — `v = (Vgk, Vpk)` → `i = (Ig, Ip)` with a 2×2 Jacobian — and `PentodeEL34` a three-port — `v = (Vg1k, Vg2k, Vpk)` → `i = (Ig1, Ig2, Ip)` with a 3×3 Jacobian. So D6d block 1 is `DkCircuit<3,2,2,1>`, not `<3,1,1,1>`, and block 5 has `N = 10`, not 4. D6d's plan must carry the corrected counts; this plan states them so the surprise lands here and not there. The CPU risk the spec flags for block 5 is correspondingly larger and is re-measured in D6d, not assumed away.
7. **The power-supply-sag coupling for D6d needs no new API.** Supply rails enter as ordinary entries of the input vector `u`, and the sag block reads the previous sample's total cathode current through `const std::array<double, N>& nonlinear_currents() const`. D6d writes the one-sample-delayed hand-off at the module level, where the delay is visible in the code that owns it. Alternative (a `set_supply` callback or a solver-level coupling between two `DkCircuit`s) was rejected: it would put circuit-topology knowledge inside the generic solver and make the delay invisible.
8. **Convergence counters are per-instance and monotonic, not per-block.** `non_converged_samples()`, `nan_fallback_samples()`, `total_samples()` and `maximum_iterations_seen()` accumulate until `reset_statistics()`. Tests compute the ratio themselves. A per-block counter would need a block boundary the solver does not know about.
9. **The diode thermal voltage is pinned at 0.025864 V** (kT/q at TNOM = 27 °C, which is what LTspice uses for the `.model D1N914` line D6a committed). `SiliconDiode` takes `saturation_current`, `ideality_factor` and `thermal_voltage` as constructor arguments with 1N914 defaults, so a germanium or LED pair in a later phase is a construction argument, not a new class.
10. **NaN policy: detect, restore, count, continue.** After each sample, if any of `v`, `i` or `x` is non-finite, the state is restored from a last-good snapshot taken at the top of the sample, `i` is zeroed, `y` is the output the restored state produces, and `nan_fallback_samples_` increments. Independently, each state is clamped to `state_limit_` (default 1e4, settable at build time) every sample; the clamp is what keeps a transient from ever reaching the range where `exp()` overflows. Both are documented in the header. `DkCircuit` therefore cannot emit a non-finite sample regardless of input, which is what "zero NaN guarantee" has to mean for a real-time node.

### Parallelism map (for the controller)

- **Task 0** (controller, inline): pre-registers all six new test files in `tests/CMakeLists.txt` with stubs, so no later task touches CMake.
- **Group A**, fully independent, run concurrently: **Task 1** (`dk/DenseLu.hpp` + `dk/DampedNewtonSolver.hpp`), **Task 2** (`SiliconDiode`), **Task 3** (`Triode12AX7`), **Task 4** (`PentodeEL34`). Disjoint files, disjoint tests.
- **Task 5** (`DkNetlist` + `DkCircuit`) needs Task 1 only.
- **Group B**, run concurrently once Task 5 lands: **Task 6** (analytic + diode-clipper validation; needs Tasks 2, 5) and **Task 7** (12AX7 stage vs SPICE, convergence statistics; needs Tasks 3, 5).
- **Task 8** (RT traps, benchmark, docs) needs everything.

---

### Task 0: Scaffolding (controller does this inline before dispatching Group A)

**Files:**
- Modify: `tests/CMakeLists.txt`
- Create: `tests/functional/unit/test_silicon_diode.cpp`, `test_triode_12ax7.cpp`, `test_pentode_el34.cpp`, `test_damped_newton_solver.cpp`, `test_dk_circuit.cpp`, `test_dk_triode_stage.cpp` (stubs)

- [ ] **Step 1: Register the six test files.** In `add_executable(sushidsp_tests …)`, under `# --- Circuits ---`, after `functional/unit/test_diode_clipper_circuit.cpp`:

```cmake
    functional/unit/test_silicon_diode.cpp
    functional/unit/test_triode_12ax7.cpp
    functional/unit/test_pentode_el34.cpp
    functional/unit/test_dk_circuit.cpp
    functional/unit/test_dk_triode_stage.cpp
```

and under `# --- Math ---`, after `functional/unit/test_newton_raphson_solver.cpp`:

```cmake
    functional/unit/test_damped_newton_solver.cpp
```

`SUSHIDSP_FIXTURE_DIR` and the `tests/` include root already exist (D6a Task 0); nothing else in this file changes. `docs/reference/spice/` is reached from tests through a new compile definition — add it beside the existing two:

```cmake
target_compile_definitions(sushidsp_tests PRIVATE
    SUSHIDSP_GOLDEN_DIR="${CMAKE_CURRENT_SOURCE_DIR}/golden"
    SUSHIDSP_FIXTURE_DIR="${CMAKE_CURRENT_SOURCE_DIR}/fixtures"
    SUSHIDSP_SPICE_DIR="${CMAKE_SOURCE_DIR}/docs/reference/spice"
)
```

- [ ] **Step 2: Create the six stubs**, each with the license header and a single `#include <gtest/gtest.h>` so the binary links.

- [ ] **Step 3: `sd build` succeeds; `sd test -s unit` green; commit**

```bash
git add tests/CMakeLists.txt tests/functional/unit/test_silicon_diode.cpp tests/functional/unit/test_triode_12ax7.cpp tests/functional/unit/test_pentode_el34.cpp tests/functional/unit/test_damped_newton_solver.cpp tests/functional/unit/test_dk_circuit.cpp tests/functional/unit/test_dk_triode_stage.cpp
git commit -m "D6b: scaffold DK circuit test units and the SPICE corpus path so parallel tasks never share a CMake edit"
```

---

## The DK formulation being implemented

Every task below transcribes from this section. Read it once before touching code; the letters
are the spec's (§D6b.1) and the derivation is Holters & Zölzer's nodal DK method with the
trapezoidal rule.

**Notation.** Node 0 is ground. `Nn` is the number of non-ground nodes. Incidence matrices have
one row per element and one column per non-ground node, with `+1` in the element's positive
node's column, `−1` in its negative node's column, and nothing for a terminal tied to ground:

| Matrix | Shape | Elements |
|---|---|---|
| `Nres` | `R × Nn` | resistors (pot sections included), `Gres = diag(1/R_k)` |
| `Nx` | `S × Nn` | reactive elements, `Gx = diag(2C_k/T)` for capacitors, `diag(T/(2L_k))` for inductors |
| `Nu` | `I × Nn` | input voltage sources |
| `Nnl` | `N × Nn` | nonlinear branches |
| `No` | `O × Nn` | output probes |

`T = 1/sample_rate` at the **run rate** (base rate × oversample factor). `Z = diag(z_k)` with
`z_k = +1` for a capacitor and `z_k = −1` for an inductor.

**Reactive companion (trapezoidal).** For a capacitor, `i = C dv/dt` discretised as
`i[n] + i[n−1] = (2C/T)(v[n] − v[n−1])` gives, with the state defined as
`x_k[n] := (2C/T)·v_k[n] + i_k[n]`:

```
i_k[n] = (2C/T)·v_k[n] − x_k[n−1]
x_k[n] = 2·(2C/T)·v_k[n] − x_k[n−1]
```

For an inductor, `v = L di/dt` discretised as `i[n] = i[n−1] + (T/2L)(v[n] + v[n−1])` with
`x_k[n] := (T/2L)·v_k[n] + i_k[n]` gives `i_k[n] = (T/2L)·v_k[n] + x_k[n−1]` and
`x_k[n] = 2·(T/2L)·v_k[n] + x_k[n−1]`. Both cases collapse to one line with `Z`:

```
i_x[n] = Gx·(Nx·v[n]) − Z·x[n−1]
x[n]   = 2·Gx·(Nx·v[n]) − Z·x[n−1]
```

**Nodal system.** KCL over the non-ground nodes, with the input sources carried as unknown
branch currents `i_u` so ideal sources are exact rather than large-conductance approximations:

```
S·v[n] + Nuᵀ·i_u[n] = Nxᵀ·Z·x[n−1] − Nnlᵀ·i[n]
Nu·v[n]             = u[n]
S = Nresᵀ·Gres·Nres + Nxᵀ·Gx·Nx                       (Nn × Nn)
```

which is one augmented linear system of size `(Nn + I)`:

```
Asys = [ S    Nuᵀ ]        Asys · [ v ; i_u ] = [ Nxᵀ·Z·x[n−1] − Nnlᵀ·i[n] ; u[n] ]
       [ Nu   0   ]
```

**Derivation (control rate, or on a pot change).** Let `Q = Asys⁻¹`, and let `P` be its first
`Nn` rows split as `P = [ Pleft (Nn × Nn) | Pright (Nn × I) ]`. Then
`v = Pleft·(Nxᵀ·Z·x[n−1] − Nnlᵀ·i) + Pright·u`, and the eight DK matrices are:

```
A = 2·Gx·Nx·Pleft·Nxᵀ·Z − Z        (S × S)      B = 2·Gx·Nx·Pright        (S × I)
C = −2·Gx·Nx·Pleft·Nnlᵀ            (S × N)
D = Nnl·Pleft·Nxᵀ·Z                (N × S)      E = Nnl·Pright            (N × I)
F = −Nnl·Pleft·Nnlᵀ                (N × N)
G = No·Pleft·Nxᵀ·Z                 (O × S)      H = No·Pright             (O × I)
K = −No·Pleft·Nnlᵀ                 (O × N)
```

**Per sample (real-time path).**

```
p[n] = D·x[n−1] + E·u[n]                          // nonlinear-port "open-circuit" voltages
v[n] = p[n] + F·f(v[n])                           // solved by damped Newton, warm-started
i[n] = f(v[n])
x[n] = A·x[n−1] + B·u[n] + C·i[n]
y[n] = G·x[n−1] + H·u[n] + K·i[n]
```

**Newton on the nonlinear port.** Residual and Jacobian:

```
r(v)  = v − p − F·f(v)
J(v)  = Identity − F·Jf(v)          where Jf(v)[a][b] = ∂f_a/∂v_b
solve   J·Δ = −r,   v ← v + λ·Δ
```

`λ = 1`, halved up to 4 times while `‖r(v + λΔ)‖∞ ≥ ‖r(v)‖∞`. Converged when
`‖Δ‖∞ ≤ 1e-9·(1 + ‖v‖∞)`. Warm start from the previous sample's `v`; at audio rates over an
oversampled path this is 1–3 iterations on almost every sample, which is the whole reason the
method is affordable.

**Sanity checks an implementer can run on paper.** For `S = 0`, `N = 1`, one resistor `R` from
the source node to the diode node and the diode to ground: `Asys` is 2×2, `F = −R`, `E = 1`, so
`v = u − R·i(v)` — which is exactly the equation `DiodeClipperCircuit` already solves. For an RC
lowpass (source → R → node → C → ground, output at the node) with `N = 0`: `A` reduces to the
scalar `(1 − RGc)/(1 + RGc)` with `Gc = 2C/T`, i.e. the bilinear-transform pole, and
`H = 1/(1 + R·Gc)`. Task 6 asserts both.

---

### Task 1: Fixed-size LU and the damped multivariate Newton solver

**Files:**
- Create: `include/SushiDSP/math/dk/DenseLu.hpp`
- Create: `include/SushiDSP/math/dk/DampedNewtonSolver.hpp`
- Modify: `tests/functional/unit/test_damped_newton_solver.cpp` (replace stub)

**Interfaces (consumed by Task 5):**
- `template <std::size_t N> class DenseLu` with `bool factorise(const std::array<std::array<double, N>, N>& matrix)` (in-place into member storage, partial pivoting, returns `false` on a singular pivot) and `void solve(const std::array<double, N>& right_hand_side, std::array<double, N>& solution) const`. No allocation, no exceptions.
- `template <std::size_t N> class DampedNewtonSolver` with
  `template <typename System> bool solve(System&& system, std::array<double, N>& v)`, where `System` is any callable exposing `void residual_and_jacobian(const std::array<double,N>& v, std::array<double,N>& r, std::array<std::array<double,N>,N>& jacobian)`;
  `std::uint64_t non_converged_count() const`, `std::uint64_t call_count() const`, `std::uint32_t maximum_iterations_seen() const`, `void reset_statistics()`;
  configuration `void set_maximum_iterations(std::uint32_t)` (default 8), `void set_maximum_halvings(std::uint32_t)` (default 4), `void set_relative_tolerance(double)` (default 1e-9).

- [ ] **Step 1: Write the failing tests** in `tests/functional/unit/test_damped_newton_solver.cpp`.

```cpp
/* LICENSE HEADER — copy from PortLayout.hpp, filename line changed */

#include <array>
#include <cmath>
#include <cstdint>

#include <gtest/gtest.h>

#include "SushiDSP/math/dk/DampedNewtonSolver.hpp"
#include "SushiDSP/math/dk/DenseLu.hpp"

namespace
{
    /// @brief The 3x3 system used for the LU golden vectors, and its hand-solved answer.
    ///
    /// A = [[2, 1, -1], [-3, -1, 2], [-2, 1, 2]], b = [8, -11, -3].
    /// Gaussian elimination by hand gives x = [2, 3, -1] exactly:
    ///   2*2 + 1*3 - 1*(-1)  =  4 + 3 + 1 =  8
    ///  -3*2 - 1*3 + 2*(-1)  = -6 - 3 - 2 = -11
    ///  -2*2 + 1*3 + 2*(-1)  = -4 + 3 - 2 = -3
    /// det(A) = 2(-1*2 - 2*1) - 1(-3*2 - 2*(-2)) + (-1)(-3*1 - (-1)(-2)) = -8 + 2 + 5 = -1.
    constexpr double kMatrix[3][3] = { { 2.0, 1.0, -1.0 }, { -3.0, -1.0, 2.0 }, { -2.0, 1.0, 2.0 } };
}

TEST(Unit_Dsp, DenseLuSolvesAHandCheckedThreeByThreeSystem)
{
    std::array<std::array<double, 3>, 3> matrix{};
    for (std::size_t row = 0; row < 3; ++row)
    {
        for (std::size_t column = 0; column < 3; ++column)
        {
            matrix[row][column] = kMatrix[row][column];
        }
    }
    SushiDSP::dk::DenseLu<3> lu;
    ASSERT_TRUE(lu.factorise(matrix));
    std::array<double, 3> solution{};
    lu.solve({ 8.0, -11.0, -3.0 }, solution);
    EXPECT_NEAR(solution[0], 2.0, 1e-12);
    EXPECT_NEAR(solution[1], 3.0, 1e-12);
    EXPECT_NEAR(solution[2], -1.0, 1e-12);
}

TEST(Unit_Dsp, DenseLuPivotsWhenTheLeadingEntryIsZero)
{
    // [[0, 1], [1, 0]] x = [3, 5] has the exact answer x = [5, 3]; without partial pivoting the
    // first elimination step divides by zero.
    std::array<std::array<double, 2>, 2> matrix{ { { 0.0, 1.0 }, { 1.0, 0.0 } } };
    SushiDSP::dk::DenseLu<2> lu;
    ASSERT_TRUE(lu.factorise(matrix));
    std::array<double, 2> solution{};
    lu.solve({ 3.0, 5.0 }, solution);
    EXPECT_NEAR(solution[0], 5.0, 1e-12);
    EXPECT_NEAR(solution[1], 3.0, 1e-12);
}

TEST(Unit_Dsp, DenseLuReportsASingularMatrix)
{
    std::array<std::array<double, 2>, 2> matrix{ { { 1.0, 2.0 }, { 2.0, 4.0 } } };
    SushiDSP::dk::DenseLu<2> lu;
    EXPECT_FALSE(lu.factorise(matrix));
}

namespace
{
    /// @brief r(v) = [v0^2 + v1^2 - 4, v0 - v1]; the root in the first quadrant is
    /// v0 = v1 = sqrt(2) = 1.41421356237309504880.
    struct CircleAndLine
    {
        void residual_and_jacobian(const std::array<double, 2>& v, std::array<double, 2>& r,
                                   std::array<std::array<double, 2>, 2>& jacobian) const
        {
            r[0] = v[0] * v[0] + v[1] * v[1] - 4.0;
            r[1] = v[0] - v[1];
            jacobian[0][0] = 2.0 * v[0];
            jacobian[0][1] = 2.0 * v[1];
            jacobian[1][0] = 1.0;
            jacobian[1][1] = -1.0;
        }
    };

    /// @brief r(v) = exp(v) - 1 in one dimension. Undamped Newton from v = 12 overshoots to a
    /// residual of the same magnitude and stalls; damping is what makes this converge.
    struct SteepExponential
    {
        void residual_and_jacobian(const std::array<double, 1>& v, std::array<double, 1>& r,
                                   std::array<std::array<double, 1>, 1>& jacobian) const
        {
            r[0] = std::exp(v[0]) - 1.0;
            jacobian[0][0] = std::exp(v[0]);
        }
    };
}

TEST(Unit_Dsp, DampedNewtonFindsTheIntersectionOfACircleAndALine)
{
    SushiDSP::dk::DampedNewtonSolver<2> solver;
    std::array<double, 2> v{ 1.0, 1.0 };
    EXPECT_TRUE(solver.solve(CircleAndLine{}, v));
    EXPECT_NEAR(v[0], std::sqrt(2.0), 1e-10);
    EXPECT_NEAR(v[1], std::sqrt(2.0), 1e-10);
    EXPECT_EQ(solver.non_converged_count(), 0u);
    EXPECT_EQ(solver.call_count(), 1u);
    EXPECT_LE(solver.maximum_iterations_seen(), 8u);
}

TEST(Unit_Dsp, DampedNewtonSurvivesASteepStartWhereUndampedNewtonWouldStall)
{
    SushiDSP::dk::DampedNewtonSolver<1> solver;
    std::array<double, 1> v{ 12.0 };
    solver.set_maximum_iterations(60);
    EXPECT_TRUE(solver.solve(SteepExponential{}, v));
    EXPECT_NEAR(v[0], 0.0, 1e-9);
    EXPECT_EQ(solver.non_converged_count(), 0u);
}

TEST(Unit_Dsp, DampedNewtonCountsANonConvergedCallAndLeavesTheIterateFinite)
{
    SushiDSP::dk::DampedNewtonSolver<1> solver;
    solver.set_maximum_iterations(1);
    std::array<double, 1> v{ 12.0 };
    EXPECT_FALSE(solver.solve(SteepExponential{}, v));
    EXPECT_EQ(solver.non_converged_count(), 1u);
    EXPECT_TRUE(std::isfinite(v[0]));
    solver.reset_statistics();
    EXPECT_EQ(solver.non_converged_count(), 0u);
    EXPECT_EQ(solver.call_count(), 0u);
}

TEST(Unit_Dsp, DampedNewtonWarmStartConvergesInOneIterationOnASmoothTrack)
{
    // Sweeping the root slowly and warm-starting from the previous answer is the audio-rate
    // usage pattern; it must not need more than a couple of iterations per step.
    SushiDSP::dk::DampedNewtonSolver<1> solver;
    std::array<double, 1> v{ 0.0 };
    for (int step = 0; step < 100; ++step)
    {
        const double target = 0.001 * static_cast<double>(step);
        struct Shifted
        {
            double target;
            void residual_and_jacobian(const std::array<double, 1>& value, std::array<double, 1>& r,
                                       std::array<std::array<double, 1>, 1>& jacobian) const
            {
                r[0] = std::exp(value[0]) - std::exp(target);
                jacobian[0][0] = std::exp(value[0]);
            }
        };
        ASSERT_TRUE(solver.solve(Shifted{ target }, v));
        EXPECT_NEAR(v[0], target, 1e-9);
    }
    EXPECT_LE(solver.maximum_iterations_seen(), 3u);
}
```

- [ ] **Step 2: `sd build` — expected: FAIL to compile (headers missing).**

- [ ] **Step 3: Write `include/SushiDSP/math/dk/DenseLu.hpp`.** Doolittle LU with partial pivoting, storing the factors in a member `std::array<std::array<double, N>, N>` and the pivot permutation in a member `std::array<std::size_t, N>`. `factorise` returns `false` when the best pivot magnitude is below `1e-300` (a genuinely singular system; the caller decides what to do — `DkCircuit` treats it as a build-time error at `prepare()` and as a fall-back-to-previous-matrices at pot-update time). `solve` does the forward/backward substitution against the stored factors. **Specialise `N == 1`** as a single division so the diode fast path the spec asks for costs nothing.

Shape (the loops are standard; write them, do not paste a library):

```cpp
namespace SushiDSP::dk
{
    /// @brief Fixed-size LU factorisation with partial pivoting for a dense N x N system.
    ///
    /// All storage is member std::array, so factorise() and solve() are allocation-free and
    /// usable on the audio thread. The factors persist after factorise(), so one factorisation
    /// can serve several right-hand sides; DkCircuit uses that when it inverts the augmented
    /// nodal matrix column by column.
    /// @tparam N System dimension.
    template <std::size_t N>
    class DenseLu
    {
    public:
        /// @brief Factorises the matrix into the internal L/U storage.
        /// @param matrix The system matrix; not modified.
        /// @return False when a pivot is numerically zero, in which case solve() must not be called.
        bool factorise(const std::array<std::array<double, N>, N>& matrix);

        /// @brief Solves A x = b using the factors from the last successful factorise().
        /// @param right_hand_side b.
        /// @param solution x, overwritten.
        void solve(const std::array<double, N>& right_hand_side, std::array<double, N>& solution) const;

    private:
        std::array<std::array<double, N>, N> factors_{};
        std::array<std::size_t, N>           pivots_{};
    };
}
```

- [ ] **Step 4: Write `include/SushiDSP/math/dk/DampedNewtonSolver.hpp`.**

The loop, transcribing the plan's "Newton on the nonlinear port":

1. Evaluate `r` and `J` at `v`. If any entry is non-finite, stop, count a non-convergence, leave `v` at the last finite iterate, return `false`.
2. `norm = ‖r‖∞`. If `norm ≤ absolute_tolerance_` (1e-12), return `true`.
3. Factorise `J`. On singularity, perturb the diagonal by `1e-12 · (1 + |J_kk|)` once and retry; on a second failure return `false` and count it.
4. Solve `J·Δ = −r`.
5. `λ = 1`. Up to `maximum_halvings_` times: evaluate `‖r(v + λΔ)‖∞`; accept the first `λ` that strictly reduces the norm; otherwise halve. If none reduces it, accept `λ` at the last halving anyway (a step is still better than a stall) and note it by not resetting the halving budget.
6. `v ← v + λΔ`. If `‖λΔ‖∞ ≤ relative_tolerance_ · (1 + ‖v‖∞)`, return `true`.
7. After `maximum_iterations_` iterations, count a non-convergence and return `false` with the best iterate seen.

The residual re-evaluation in step 5 needs the Jacobian slot as scratch; give the class two
member `std::array` buffers for the trial residual and one for the Jacobian so a `System` that
computes both together is never asked to compute one alone. Every member is `std::array`; the
class has no constructor argument.

- [ ] **Step 5: `sd build && sd test -s unit --filter '*DenseLu*:*DampedNewton*'`** (or `sd test -s unit` if the CLI has no filter flag — check `sd test --help` once and use whichever it offers). Expected: all eight cases PASS.

- [ ] **Step 6: Commit**

```bash
git add include/SushiDSP/math/dk/DenseLu.hpp include/SushiDSP/math/dk/DampedNewtonSolver.hpp tests/functional/unit/test_damped_newton_solver.cpp
git commit -m "D6b: fixed-size LU and damped N-dimensional Newton with convergence counters"
```

---

### Task 2: `SiliconDiode`

**Files:**
- Create: `include/SushiDSP/circuits/models/SiliconDiode.hpp`
- Modify: `tests/functional/unit/test_silicon_diode.cpp` (replace stub)

**Interfaces (consumed by Tasks 5, 6 and by D6c):**
- `class SiliconDiode` with `SiliconDiode(double saturation_current_amps = 2.52e-9, double ideality_factor = 1.752, double thermal_voltage_volts = 0.025864)`, `double current(double voltage) const`, `double jacobian(double voltage) const`, and `static constexpr std::size_t kBranchCount = 1;`.

**Equations (1N914, `docs/reference/spice/models/d1n914.sub`):**

```
n·Vt = 1.752 · 0.025864 = 0.045313728 V
i(v)  = Is · (exp(v / (n·Vt)) − 1)
di/dv = (Is / (n·Vt)) · exp(v / (n·Vt))
```

The exponent argument is clamped to `±40` before `exp` (a limit `DiodeClipperCircuit` already
uses in spirit, at 60·Vt); above `v = 40·n·Vt ≈ 1.81 V` no silicon diode in a guitar circuit ever
sits, and the clamp is what stops a Newton overshoot from producing `inf`. **Clamping the
exponent alone breaks C¹** — clamp by *linear extrapolation* instead: for `v > v_max`,
`i(v) = i(v_max) + jacobian(v_max)·(v − v_max)` and `jacobian(v) = jacobian(v_max)`, and
symmetrically below `−v_max`. That is C¹ everywhere, finite everywhere, and identical to the
Shockley law inside the operating range. State this in the header comment.

- [ ] **Step 1: Write the failing tests.** The expected values below were computed at double
precision while writing this plan; the arithmetic is in the comments so a reader can re-derive
them without running anything.

```cpp
/* LICENSE HEADER — copy from PortLayout.hpp, filename line changed */

#include <cmath>

#include <gtest/gtest.h>

#include "SushiDSP/circuits/models/SiliconDiode.hpp"

TEST(Unit_Dsp, SiliconDiodeMatchesShockleyAtThreeOperatingPoints)
{
    // 1N914: Is = 2.52e-9 A, n = 1.752, Vt = 0.025864 V, so n*Vt = 0.045313728 V exactly.
    //
    //  v = 0.4 V : v/(nVt) = 8.82734...  exp = 6.81817404536e3
    //              i  = 2.52e-9 * (6818.17404536 - 1)      = 1.71792785943e-5 A
    //              di = (2.52e-9 / 0.045313728) * 6818.174 = 3.79174244818e-4 S
    //  v = 0.6 V : v/(nVt) = 13.2410...  exp = 5.6299187171e5
    //              i  = 2.52e-9 * (562991.87171 - 1)       = 1.41873699671e-3 A
    //              di = (2.52e-9 / 0.045313728) * 562991.9 = 3.13092649696e-2 S
    //  v = 0.7 V : v/(nVt) = 15.4479...  exp = 5.11586582342e6
    //              i  = 2.52e-9 * (5115865.82342 - 1)      = 1.2891979355e-2 A
    //              di = (2.52e-9 / 0.045313728) * 5115866  = 2.84504993167e-1 S
    const SushiDSP::SiliconDiode diode;
    EXPECT_NEAR(diode.current(0.4), 1.71792785943e-5, 1e-16);
    EXPECT_NEAR(diode.jacobian(0.4), 3.79174244818e-4, 1e-15);
    EXPECT_NEAR(diode.current(0.6), 1.41873699671e-3, 1e-14);
    EXPECT_NEAR(diode.jacobian(0.6), 3.13092649696e-2, 1e-13);
    EXPECT_NEAR(diode.current(0.7), 1.2891979355e-2, 1e-13);
    EXPECT_NEAR(diode.jacobian(0.7), 2.84504993167e-1, 1e-12);
}

TEST(Unit_Dsp, SiliconDiodeReverseCurrentApproachesMinusSaturationCurrent)
{
    // v = -0.5 V: exp(-11.0342) = 1.61404223972e-5, i = 2.52e-9 * (1.614e-5 - 1)
    //           = -2.51995932614e-9 A, di = (2.52e-9/0.045313728)*1.614e-5 = 8.97605786067e-13 S
    const SushiDSP::SiliconDiode diode;
    EXPECT_NEAR(diode.current(-0.5), -2.51995932614e-9, 1e-20);
    EXPECT_NEAR(diode.jacobian(-0.5), 8.97605786067e-13, 1e-24);
    EXPECT_NEAR(diode.current(0.0), 0.0, 1e-24);
}

TEST(Unit_Dsp, SiliconDiodeJacobianMatchesACentredDifference)
{
    const SushiDSP::SiliconDiode diode;
    for (const double voltage : { -0.3, 0.0, 0.3, 0.55, 0.65 })
    {
        const double step = 1e-6;
        const double numerical = (diode.current(voltage + step) - diode.current(voltage - step)) / (2.0 * step);
        EXPECT_NEAR(diode.jacobian(voltage), numerical, 1e-6 * std::abs(diode.jacobian(voltage)) + 1e-14) << voltage;
    }
}

TEST(Unit_Dsp, SiliconDiodeStaysFiniteAndCOneAcrossTheLinearExtrapolation)
{
    const SushiDSP::SiliconDiode diode;
    for (const double voltage : { -60.0, -2.0, 2.0, 60.0, 1000.0 })
    {
        EXPECT_TRUE(std::isfinite(diode.current(voltage))) << voltage;
        EXPECT_TRUE(std::isfinite(diode.jacobian(voltage))) << voltage;
    }
    // The extrapolation knee sits at v_max = 40 * n * Vt = 1.81254912 V; the derivative must be
    // continuous across it.
    const double knee = 40.0 * 1.752 * 0.025864;
    EXPECT_NEAR(diode.jacobian(knee - 1e-7), diode.jacobian(knee + 1e-7), 1e-3);
    EXPECT_NEAR(diode.current(knee + 0.5), diode.current(knee) + 0.5 * diode.jacobian(knee), 1e-9);
}

TEST(Unit_Dsp, SiliconDiodeHonoursANonDefaultIdealityFactor)
{
    // n = 1 makes n*Vt = 0.025864; at v = 0.6 the exponent is 23.1983..., exp = 1.19426e10,
    // so the current is far larger than the 1N914's at the same voltage.
    const SushiDSP::SiliconDiode ideal(2.52e-9, 1.0, 0.025864);
    const SushiDSP::SiliconDiode part;
    EXPECT_GT(ideal.current(0.6), 100.0 * part.current(0.6));
}
```

- [ ] **Step 2: `sd build` — expected FAIL. Step 3: write the header.** Members
`saturation_current_`, `ideality_factor_`, `thermal_voltage_`, and the precomputed
`inverse_scaled_thermal_voltage_ = 1.0 / (ideality_factor · thermal_voltage)` plus the knee
values, so `current()` is one `exp` and two multiplies. Doxygen naming the `.sub` file the
constants come from.

- [ ] **Step 4: `sd test -s unit` green. Commit**

```bash
git add include/SushiDSP/circuits/models/SiliconDiode.hpp tests/functional/unit/test_silicon_diode.cpp
git commit -m "D6b: SiliconDiode (Shockley with ideality n), 1N914 parameters matching d1n914.sub"
```

---

### Task 3: `Triode12AX7`

**Files:**
- Create: `include/SushiDSP/circuits/models/Triode12AX7.hpp`
- Modify: `tests/functional/unit/test_triode_12ax7.cpp` (replace stub)

**Interfaces (consumed by Tasks 5, 7 and by D6d):**

```cpp
class Triode12AX7
{
public:
    static constexpr std::size_t kBranchCount = 2;   // branch 0 = grid, branch 1 = plate

    /// v[0] = Vgk, v[1] = Vpk;  i[0] = Ig, i[1] = Ip.
    void current(const std::array<double, 2>& v, std::array<double, 2>& i) const;
    void jacobian(const std::array<double, 2>& v, std::array<std::array<double, 2>, 2>& j) const;
};
```

`j[a][b] = ∂i_a/∂v_b`. Because `Ig` depends only on `Vgk`, `j[0][1]` is exactly `0.0`.

**Equations (`docs/reference/spice/models/12ax7_koren.sub`), μ = 100, kp = 600, kvb = 300, kg1 = 1060, kx = 1.4, Ig0 = 1 µA, Vγ = 0.5 V, Vt_g = 0.15 V:**

```
root  = sqrt(kvb + Vpk²)
t     = kp · (1/μ + Vgk / root)
s     = softplus(t) = ln(1 + exp(t))                  // log1p(exp(t)) for t < 30, else t
sigma = 1 / (1 + exp(−t))                             // d(softplus)/dt
E1    = Vpk · s / kp
Ip    = (E1 ≤ 0) ? 0 : 2 · E1^kx / kg1                // the (1 + sgn(E1)) factor, written branch-free
Ig    = Ig0 · softplus((Vgk − Vgamma) / Vt_g)

∂E1/∂Vgk = Vpk · sigma / root
∂E1/∂Vpk = s / kp − Vpk² · Vgk · sigma / root³
∂Ip/∂x   = (E1 ≤ 0) ? 0 : (2 · kx · E1^(kx−1) / kg1) · ∂E1/∂x
∂Ig/∂Vgk = (Ig0 / Vt_g) · sigma((Vgk − Vgamma) / Vt_g)
∂Ig/∂Vpk = 0
```

C¹ at `E1 = 0`: `kx = 1.4 > 1`, so `E1^(kx−1) → 0` and both `Ip` and its derivative vanish
continuously. The `softplus`/`sigma` pair is C^∞. Write `softplus` and `sigma` as file-local
helpers in an anonymous namespace **shared with Task 4's pentode**; put them in
`include/SushiDSP/circuits/models/SoftTransition.hpp` (created by whichever of Tasks 3 and 4
lands first — the controller assigns it to **Task 3** so Task 4 never creates it) as
`namespace SushiDSP::models { inline double softplus(double); inline double logistic(double); }`,
each with Doxygen and the overflow guard.

- [ ] **Step 1: Write the failing tests.** All values below were computed at double precision
while writing this plan; the intermediate `t` and `E1` are given so a reader can check the chain.

```cpp
/* LICENSE HEADER — copy from PortLayout.hpp, filename line changed */

#include <array>
#include <cmath>

#include <gtest/gtest.h>

#include "SushiDSP/circuits/models/Triode12AX7.hpp"

TEST(Unit_Dsp, Triode12AX7PlateCurrentMatchesKorenAtThreeOperatingPoints)
{
    // mu = 100, kp = 600, kvb = 300, kg1 = 1060, kx = 1.4.
    //
    // (1) Vpk = 250, Vgk = -1.5  (the classic 12AX7 quiescent point)
    //     root = sqrt(300 + 62500) = 250.599281...
    //     t    = 600*(0.01 - 1.5/250.599281) = 2.4086090199
    //     s    = ln(1 + exp(t)) = 2.4948121...   E1 = 250*s/600 = 1.03947164822
    //     Ip   = 2 * 1.03947164822^1.4 / 1060   = 1.99187388685e-3 A   (1.99 mA - right for a 12AX7)
    //     dIp/dVgk = 2.45547036171e-3 S,  dIp/dVpk = 2.58169362129e-5 S
    //     (gm = 2.46 mA/V and rp = 1/2.58e-5 = 38.7 kOhm, both within a few percent of the
    //      datasheet's 1.6 mA/V / 62.5 kOhm at this point - Koren's fit runs hot here, which is
    //      the model's known behaviour, not a transcription error.)
    //
    // (2) Vpk = 100, Vgk = -2.0  (deep toward cutoff)
    //     t = 600*(0.01 - 2/100.0/*sqrt(300+10000)=101.4889*/ )  -> t = -5.82395133797
    //     s = 2.95195... e-3,  E1 = 100*s/600 = 4.91923696815e-4
    //     Ip = 2 * 4.91923696815e-4^1.4 / 1060 = 4.40941496484e-8 A
    //     dIp/dVgk = 3.6441889239e-7 S,  dIp/dVpk = 7.69341309294e-9 S
    //
    // (3) Vpk = 300, Vgk = 0  (grid at the cathode; the stage is fully on)
    //     root = sqrt(300 + 90000) = 300.499584..., t = 600 * 0.01 = 6 exactly
    //     s = ln(1 + exp(6)) = 6.00247568514,  E1 = 300*s/600 = 3.00123784257
    //     Ip = 2 * 3.00123784257^1.4 / 1060 = 8.78910615165e-3 A
    //     dIp/dVgk = 4.08295444883e-3 S,  dIp/dVpk = 4.10158287077e-5 S
    const SushiDSP::Triode12AX7 triode;
    struct Point { double vgk, vpk, ip, dip_dvgk, dip_dvpk; };
    const Point points[3] = {
        { -1.5, 250.0, 1.99187388685e-3, 2.45547036171e-3, 2.58169362129e-5 },
        { -2.0, 100.0, 4.40941496484e-8, 3.6441889239e-7,  7.69341309294e-9 },
        {  0.0, 300.0, 8.78910615165e-3, 4.08295444883e-3, 4.10158287077e-5 },
    };
    for (const Point& point : points)
    {
        std::array<double, 2> currents{};
        std::array<std::array<double, 2>, 2> jacobian{};
        triode.current({ point.vgk, point.vpk }, currents);
        triode.jacobian({ point.vgk, point.vpk }, jacobian);
        EXPECT_NEAR(currents[1], point.ip, 1e-11 + 1e-9 * point.ip) << point.vpk;
        EXPECT_NEAR(jacobian[1][0], point.dip_dvgk, 1e-11 + 1e-9 * point.dip_dvgk) << point.vpk;
        EXPECT_NEAR(jacobian[1][1], point.dip_dvpk, 1e-13 + 1e-9 * point.dip_dvpk) << point.vpk;
        EXPECT_DOUBLE_EQ(jacobian[0][1], 0.0);
    }
}

TEST(Unit_Dsp, Triode12AX7GridCurrentFollowsNormansSoftLaw)
{
    // Ig = 1e-6 * ln(1 + exp((Vgk - 0.5)/0.15)), dIg/dVgk = (1e-6/0.15) * logistic(same argument).
    //
    //   Vgk = -1.5 : argument = -13.3333333333, ln(1+exp) = 1.61959548077e-6
    //                Ig = 1.61959548077e-12 A,  dIg = 1.07972944615e-11 S  (effectively cut off)
    //   Vgk =  0.5 : argument = 0, ln(2) = 0.693147180560
    //                Ig = 6.9314718056e-7 A,   dIg = (1e-6/0.15)*0.5 = 3.33333333333e-6 S
    //   Vgk =  1.0 : argument = 3.33333333333, ln(1+exp) = 3.36838574941
    //                Ig = 3.36838574941e-6 A,  dIg = (1e-6/0.15)*0.965651... = 6.43703202889e-6 S
    const SushiDSP::Triode12AX7 triode;
    struct Point { double vgk, ig, dig; };
    const Point points[3] = {
        { -1.5, 1.61959548077e-12, 1.07972944615e-11 },
        {  0.5, 6.9314718056e-7,   3.33333333333e-6  },
        {  1.0, 3.36838574941e-6,  6.43703202889e-6  },
    };
    for (const Point& point : points)
    {
        std::array<double, 2> currents{};
        std::array<std::array<double, 2>, 2> jacobian{};
        triode.current({ point.vgk, 250.0 }, currents);
        triode.jacobian({ point.vgk, 250.0 }, jacobian);
        EXPECT_NEAR(currents[0], point.ig, 1e-18 + 1e-9 * point.ig) << point.vgk;
        EXPECT_NEAR(jacobian[0][0], point.dig, 1e-18 + 1e-9 * point.dig) << point.vgk;
    }
}

TEST(Unit_Dsp, Triode12AX7JacobianMatchesCentredDifferencesEverywhere)
{
    const SushiDSP::Triode12AX7 triode;
    for (const double vgk : { -8.0, -3.0, -1.0, 0.0, 1.0 })
    {
        for (const double vpk : { 5.0, 50.0, 150.0, 300.0, 400.0 })
        {
            std::array<std::array<double, 2>, 2> jacobian{};
            triode.jacobian({ vgk, vpk }, jacobian);
            for (std::size_t input = 0; input < 2; ++input)
            {
                const double step = 1e-6;
                std::array<double, 2> up{ vgk, vpk };
                std::array<double, 2> down{ vgk, vpk };
                up[input] += step;
                down[input] -= step;
                std::array<double, 2> current_up{}, current_down{};
                triode.current(up, current_up);
                triode.current(down, current_down);
                for (std::size_t output = 0; output < 2; ++output)
                {
                    const double numerical = (current_up[output] - current_down[output]) / (2.0 * step);
                    EXPECT_NEAR(jacobian[output][input], numerical,
                                1e-5 * std::abs(jacobian[output][input]) + 1e-12)
                        << vgk << " " << vpk << " " << output << " " << input;
                }
            }
        }
    }
}

TEST(Unit_Dsp, Triode12AX7IsCutOffAndCOneAtNonPositivePlateVoltage)
{
    const SushiDSP::Triode12AX7 triode;
    std::array<double, 2> currents{};
    std::array<std::array<double, 2>, 2> jacobian{};
    triode.current({ -2.0, -10.0 }, currents);
    triode.jacobian({ -2.0, -10.0 }, jacobian);
    EXPECT_DOUBLE_EQ(currents[1], 0.0);
    EXPECT_DOUBLE_EQ(jacobian[1][0], 0.0);
    EXPECT_DOUBLE_EQ(jacobian[1][1], 0.0);
    // Approaching Vpk = 0 from above, plate current and both of its derivatives go to zero, so
    // the piecewise definition is C1 and not a slope discontinuity like the D5b clamp was.
    for (const double vpk : { 1e-3, 1e-4, 1e-5 })
    {
        triode.current({ -2.0, vpk }, currents);
        triode.jacobian({ -2.0, vpk }, jacobian);
        EXPECT_LT(currents[1], 1e-9);
        EXPECT_LT(std::abs(jacobian[1][0]), 1e-6);
    }
}

TEST(Unit_Dsp, Triode12AX7StaysFiniteUnderAbsurdInputs)
{
    const SushiDSP::Triode12AX7 triode;
    for (const double vgk : { -1e4, -50.0, 50.0, 1e4 })
    {
        for (const double vpk : { -1e4, 0.0, 1e4 })
        {
            std::array<double, 2> currents{};
            std::array<std::array<double, 2>, 2> jacobian{};
            triode.current({ vgk, vpk }, currents);
            triode.jacobian({ vgk, vpk }, jacobian);
            EXPECT_TRUE(std::isfinite(currents[0]) && std::isfinite(currents[1])) << vgk << " " << vpk;
            EXPECT_TRUE(std::isfinite(jacobian[0][0]) && std::isfinite(jacobian[1][1])) << vgk << " " << vpk;
        }
    }
}
```

- [ ] **Step 2: `sd build` — expected FAIL. Step 3: write `SoftTransition.hpp`** with
`softplus(t) = t < 30.0 ? std::log1p(std::exp(t)) : t` (the two branches agree to better than
1e-13 at the switch: `ln(1+e³⁰) − 30 ≈ 9.4e-14`) and
`logistic(t) = 1.0 / (1.0 + std::exp(-t))` guarded for large negative `t`.

- [ ] **Step 4: Write `Triode12AX7.hpp`,** transcribing the equation block above line for line,
each line carrying the plan's symbol name in a comment. Parameters are `static constexpr double`
members named `kMu`, `kKp`, `kKvb`, `kKg1`, `kKx`, `kGridCurrentScale`, `kGridThresholdVolts`,
`kGridTransitionVolts`, each with a comment naming `12ax7_koren.sub`. The class is stateless and
default-constructible; `current` and `jacobian` are `const`.

Note for the implementer: `current` and `jacobian` recompute the same `softplus`/`logistic` pair.
That duplication is deliberate — `DkCircuit` always calls them together and a future profile may
justify a joint `current_and_jacobian`, but two clean functions matching the spec's stated
interface come first. If Task 8's benchmark misses the 0.12 ms budget, add the joint form then,
with the number that justified it in the commit message.

- [ ] **Step 5: `sd test -s unit` green. Commit**

```bash
git add include/SushiDSP/circuits/models/SoftTransition.hpp include/SushiDSP/circuits/models/Triode12AX7.hpp tests/functional/unit/test_triode_12ax7.cpp
git commit -m "D6b: Triode12AX7 (Koren plate current + Norman grid current), parameters matching 12ax7_koren.sub"
```

---

### Task 4: `PentodeEL34`

**Files:**
- Create: `include/SushiDSP/circuits/models/PentodeEL34.hpp`
- Modify: `tests/functional/unit/test_pentode_el34.cpp` (replace stub)

**Depends on** `SoftTransition.hpp` from Task 3. Run Task 4 after Task 3's Step 3 lands, or —
if the controller dispatches both at once — have Task 4 declare the dependency and wait; do **not**
create a second copy of `softplus`.

**Interfaces:**

```cpp
class PentodeEL34
{
public:
    static constexpr std::size_t kBranchCount = 3;   // 0 = control grid, 1 = screen, 2 = plate

    /// v[0] = Vg1k, v[1] = Vg2k, v[2] = Vpk;  i[0] = Ig1, i[1] = Ig2, i[2] = Ip.
    void current(const std::array<double, 3>& v, std::array<double, 3>& i) const;
    void jacobian(const std::array<double, 3>& v, std::array<std::array<double, 3>, 3>& j) const;
};
```

**Equations (`el34_koren.sub`), μ = 11, kp = 60, kvb = 24, kg1 = 650, kg2 = 4500, kx = 1.35, grid law as the triode's:**

```
u_arg  = kp · (1/μ + Vg1k / Vg2k)                   // Vg2k floored at 1e-3 V to keep this finite
s      = softplus(u_arg)
sigma  = logistic(u_arg)
E1     = Vg2k · s / kp
knee   = atan(Vpk / kvb)
Ip     = (E1 ≤ 0) ? 0 : 2 · E1^kx / kg1 · knee
z      = Vg2k / μ + Vg1k
Ig2    = (z ≤ 0) ? 0 : z^kx / kg2
Ig1    = Ig0 · softplus((Vg1k − Vgamma) / Vt_g)

∂E1/∂Vg1k = sigma
∂E1/∂Vg2k = s / kp − sigma · Vg1k / Vg2k
∂Ip/∂Vg1k = (2 · kx · E1^(kx−1) / kg1) · knee · ∂E1/∂Vg1k
∂Ip/∂Vg2k = (2 · kx · E1^(kx−1) / kg1) · knee · ∂E1/∂Vg2k
∂Ip/∂Vpk  = (2 · E1^kx / kg1) · (1/kvb) / (1 + (Vpk/kvb)²)
∂Ig2/∂Vg1k = (z ≤ 0) ? 0 : kx · z^(kx−1) / kg2
∂Ig2/∂Vg2k = ∂Ig2/∂Vg1k / μ
∂Ig2/∂Vpk  = 0
∂Ig1/∂Vg1k = (Ig0 / Vt_g) · logistic((Vg1k − Vgamma) / Vt_g);  the other two entries are 0
```

`atan(Vpk/kvb)` is what makes the plate current knee over instead of continuing to rise — it is
the pentode's whole character and where class-AB crossover comes from. `kx = 1.35 > 1` makes both
`Ip` and `Ig2` C¹ across their `max(0, ·)` kinks.

- [ ] **Step 1: Write the failing tests.** Values computed at double precision while writing this
plan.

```cpp
TEST(Unit_Dsp, PentodeEL34MatchesKorenAtThreeOperatingPoints)
{
    // mu = 11, kp = 60, kvb = 24, kg1 = 650, kg2 = 4500, kx = 1.35.
    //
    // (1) Vpk = 400, Vg2k = 400, Vg1k = -37  (a JCM800 idle point at -37 V fixed bias)
    //     u_arg = 60*(1/11 - 37/400) = -0.0954545454545
    //     s = ln(1+exp(u_arg)) = 0.646558...   E1 = 400*s/60 = 4.31038947984
    //     knee = atan(400/24) = 1.510796...
    //     Ip  = 2 * 4.31038947984^1.35 / 650 * knee = 3.34149261301e-2 A   (33.4 mA idle)
    //     dIp/dVg1k = 4.98316907313e-3 S,  dIp/dVg2k = 5.73718514954e-4 S,
    //     dIp/dVpk  = 3.30555615549e-6 S   (a ~300 kOhm plate resistance: pentode, as expected)
    //     z = 400/11 - 37 = -0.636363636364 < 0, so Ig2 = 0 and its whole row is 0 - the screen
    //     draws nothing at idle, which is what the max(0, .) clamp is for.
    //
    // (2) Vpk = 250, Vg2k = 300, Vg1k = -20
    //     u_arg = 60*(1/11 - 20/300) = 1.45454545455,  s = 1.66440636413
    //     E1 = 300*s/60 = 8.32203182063,  knee = atan(250/24) = 1.47517...
    //     Ip = 7.92947981393e-2 A,  dIp/dVg1k = 1.04281593601e-2 S,
    //     dIp/dVg2k = 1.05203721563e-3 S, dIp/dVpk = 2.04537709632e-5 S
    //     z = 300/11 - 20 = 7.27272727273,  Ig2 = 7.27272727273^1.35 / 4500 = 3.23652408856e-3 A
    //     dIg2/dVg1k = 1.35 * 7.2727^0.35 / 4500 = 6.00779783938e-4 S
    //     dIg2/dVg2k = 6.00779783938e-4 / 11    = 5.46163439944e-5 S
    //
    // (3) Vpk = 100, Vg2k = 400, Vg1k = -5   (hard into conduction, plate pulled down)
    //     u_arg = 60*(1/11 - 5/400) = 4.70454545455,  s = 4.71355874131
    //     E1 = 400*s/60 = 31.4237249421,  knee = atan(100/24) = 1.33705...
    //     Ip = 4.31496731895e-1 A,  dIp/dVg1k = 1.83712705886e-2 S,
    //     dIp/dVg2k = 1.6859423525e-3 S,  dIp/dVpk = 7.33338162266e-4 S
    //     z = 400/11 - 5 = 31.3636363636,  Ig2 = 2.3278936712e-2 A,
    //     dIg2/dVg1k = 1.0020064063e-3 S,  dIg2/dVg2k = 9.10914914819e-5 S
    const SushiDSP::PentodeEL34 pentode;
    struct Point
    {
        double vg1k, vg2k, vpk;
        double ip, dip_dvg1k, dip_dvg2k, dip_dvpk;
        double ig2, dig2_dvg1k, dig2_dvg2k;
    };
    const Point points[3] = {
        { -37.0, 400.0, 400.0, 3.34149261301e-2, 4.98316907313e-3, 5.73718514954e-4, 3.30555615549e-6,
          0.0, 0.0, 0.0 },
        { -20.0, 300.0, 250.0, 7.92947981393e-2, 1.04281593601e-2, 1.05203721563e-3, 2.04537709632e-5,
          3.23652408856e-3, 6.00779783938e-4, 5.46163439944e-5 },
        {  -5.0, 400.0, 100.0, 4.31496731895e-1, 1.83712705886e-2, 1.6859423525e-3, 7.33338162266e-4,
          2.3278936712e-2, 1.0020064063e-3, 9.10914914819e-5 },
    };
    for (const Point& point : points)
    {
        const std::array<double, 3> voltages{ point.vg1k, point.vg2k, point.vpk };
        std::array<double, 3> currents{};
        std::array<std::array<double, 3>, 3> jacobian{};
        pentode.current(voltages, currents);
        pentode.jacobian(voltages, jacobian);
        EXPECT_NEAR(currents[2], point.ip, 1e-12 + 1e-9 * point.ip) << point.vg1k;
        EXPECT_NEAR(currents[1], point.ig2, 1e-12 + 1e-9 * point.ig2) << point.vg1k;
        EXPECT_NEAR(jacobian[2][0], point.dip_dvg1k, 1e-12 + 1e-9 * point.dip_dvg1k) << point.vg1k;
        EXPECT_NEAR(jacobian[2][1], point.dip_dvg2k, 1e-12 + 1e-9 * point.dip_dvg2k) << point.vg1k;
        EXPECT_NEAR(jacobian[2][2], point.dip_dvpk, 1e-14 + 1e-9 * point.dip_dvpk) << point.vg1k;
        EXPECT_NEAR(jacobian[1][0], point.dig2_dvg1k, 1e-12 + 1e-9 * point.dig2_dvg1k) << point.vg1k;
        EXPECT_NEAR(jacobian[1][1], point.dig2_dvg2k, 1e-13 + 1e-9 * point.dig2_dvg2k) << point.vg1k;
        EXPECT_DOUBLE_EQ(jacobian[1][2], 0.0);
        EXPECT_DOUBLE_EQ(jacobian[0][1], 0.0);
        EXPECT_DOUBLE_EQ(jacobian[0][2], 0.0);
    }
}
```

plus, in the same file and following Task 3's shapes exactly:

- `PentodeEL34JacobianMatchesCentredDifferencesEverywhere` — the 3×3 sweep over
  `Vg1k ∈ {−45, −30, −15, −5, 0}`, `Vg2k ∈ {50, 200, 400}`, `Vpk ∈ {20, 100, 250, 450}`, same
  1e-6 step, same 1e-5 relative tolerance. Skip the comparison at points within 1e-3 of the
  `z = 0` kink, where a centred difference straddles the kink and is not the derivative; note
  that in a comment.
- `PentodeEL34ScreenCurrentIsCOneAcrossItsKink` — sweep `Vg1k` through `z = 0` (at
  `Vg1k = −Vg2k/μ`) and assert `Ig2` and `∂Ig2/∂Vg1k` both approach zero.
- `PentodeEL34PlateCurrentKneesOverWithPlateVoltage` — golden-vector from `atan`: the ratio
  `Ip(Vpk = 400) / Ip(Vpk = 100)` at fixed grids equals `atan(400/24) / atan(100/24)`
  `= 1.51079585976 / 1.33705315408 = 1.12994...` to 1e-9. This is the pentode property that
  distinguishes it from a triode and it must be exact, not approximate.
- `PentodeEL34StaysFiniteUnderAbsurdInputs` — including `Vg2k = 0` and `Vg2k < 0`, which the
  1e-3 V floor must survive.

- [ ] **Step 2: `sd build` FAIL → Step 3: write the header** with `static constexpr` parameters
named as in Task 3 plus `kKg2`, each commented with `el34_koren.sub`.

- [ ] **Step 4: `sd test -s unit` green. Commit**

```bash
git add include/SushiDSP/circuits/models/PentodeEL34.hpp tests/functional/unit/test_pentode_el34.cpp
git commit -m "D6b: PentodeEL34 (Koren pentode with screen current), parameters matching el34_koren.sub"
```

---

### Task 5: `DkNetlist` and `DkCircuit`

**Files:**
- Create: `include/SushiDSP/math/dk/DkNetlist.hpp`
- Create: `include/SushiDSP/math/dk/DkCircuit.hpp`

**Depends on:** Task 1. Adds no tests of its own — Tasks 6 and 7 are its test suite, which is why
it is a single task and not test-driven in isolation. Its Step 6 is a compile-and-smoke gate.

**Interfaces (consumed by Tasks 6, 7, 8 and by every D6c/D6d block):**

```cpp
namespace SushiDSP::dk
{
    inline constexpr std::size_t kMaximumNodes = 16;
    inline constexpr std::size_t kMaximumResistors = 32;
    inline constexpr std::size_t kMaximumPotentiometers = 4;

    /// @brief Control-thread description of a circuit: node indices and component values.
    /// Node 0 is ground. This is a plain aggregate of fixed-size arrays so DkCircuit can own a
    /// copy and re-derive its matrices on the audio thread without touching the builder.
    template <std::size_t S, std::size_t N, std::size_t I, std::size_t O>
    struct DkNetlist
    {
        std::size_t node_count = 0;                 ///< non-ground node count, <= kMaximumNodes

        void resistor(std::size_t a, std::size_t b, double ohms);
        void capacitor(std::size_t a, std::size_t b, double farads);
        void inductor(std::size_t a, std::size_t b, double henries);
        void voltage_source(std::size_t a, std::size_t b);
        std::size_t potentiometer(std::size_t a, std::size_t wiper, std::size_t b, double ohms);
        void nonlinear(std::size_t a, std::size_t b);
        void output(std::size_t a, std::size_t b);
    };

    /// @brief One discretised nonlinear circuit block, solved by damped Newton per sample.
    template <std::size_t S, std::size_t N, std::size_t I, std::size_t O>
    class DkCircuit
    {
    public:
        bool prepare(const DkNetlist<S, N, I, O>& netlist, double sample_rate);

        template <typename NonlinearSystem>
        void process_sample(NonlinearSystem& nonlinear,
                            const std::array<double, I>& inputs,
                            std::array<double, O>& outputs);

        void set_potentiometer(std::size_t index, double position);
        bool update_matrices_if_dirty();

        void reset();
        const std::array<double, N>& nonlinear_currents() const;
        const std::array<double, N>& nonlinear_voltages() const;
        const std::array<double, S>& states() const;

        std::uint64_t non_converged_samples() const;
        std::uint64_t nan_fallback_samples() const;
        std::uint64_t total_samples() const;
        std::uint32_t maximum_iterations_seen() const;
        void          reset_statistics();
        void          set_state_limit(double limit);
    };
}
```

`NonlinearSystem` is the caller-supplied glue between the branch vector and the device models.
It exposes exactly two methods, and D6c/D6d each write one small struct per block:

```cpp
void currents(const std::array<double, N>& v, std::array<double, N>& i) const;
void jacobian(const std::array<double, N>& v, std::array<std::array<double, N>, N>& j) const;
```

Off-diagonal blocks between independent devices are zero and the glue struct simply does not
write them — `DkCircuit` zeroes the Jacobian buffer before each call. Passing it by reference
rather than storing it keeps `DkCircuit` free of any model dependency, so `dk/` stays in
`SushiDSP/math/` with no include of `SushiDSP/circuits/`. That layering is the point.

- [ ] **Step 1: Write `DkNetlist.hpp`.**

Storage: `std::array<std::array<double, kMaximumNodes>, kMaximumResistors> resistor_incidence_`
is wasteful and slow to derive from; instead store, for each element, the pair
`(positive_node, negative_node)` and the value, and build the incidence products directly from
those pairs in the derivation (a stamp loop, not a dense matrix multiply). Concretely:

```cpp
struct Branch
{
    std::size_t positive_node;   ///< 0 means ground
    std::size_t negative_node;
    double      value;           ///< ohms, farads or henries as the table demands
};
```

with `std::array<Branch, kMaximumResistors> resistors_`, `std::array<Branch, S> reactive_` and a
parallel `std::array<double, S> reactive_sign_` holding `Z` (+1 capacitor, −1 inductor),
`std::array<Branch, I> sources_`, `std::array<Branch, N> nonlinear_`, `std::array<Branch, O> outputs_`,
plus counters. `potentiometer(a, wiper, b, ohms)` appends **two** resistor branches
(`a–wiper` and `wiper–b`), records their indices and `ohms` in
`std::array<PotentiometerEntry, kMaximumPotentiometers>`, and returns the pot's index for
`set_potentiometer`. Each `add` asserts (`assert`, control thread only) that the counter has room
and that node indices are `< kMaximumNodes`; `DkCircuit::prepare` returns `false` on any violation
rather than asserting, because `prepare` is reachable from a preset load.

Doxygen on every method naming which sign convention the caller must use (`a` is the positive
terminal; for a nonlinear branch that means `v = V(a) − V(b)` is what the model receives, and a
positive model current flows from `a` to `b` through the device).

- [ ] **Step 2: Write `DkCircuit.hpp` — storage and `prepare()`.**

Members: a copy of the netlist; `double sample_rate_`; the eight matrices as
`std::array<std::array<double, ·>, ·>`; `std::array<double, S> state_`, `state_backup_`;
`std::array<double, N> nonlinear_voltage_`, `nonlinear_current_`, `open_circuit_voltage_`;
`DampedNewtonSolver<N> solver_`; the derivation scratch
`std::array<std::array<double, kMaximumNodes + I>, kMaximumNodes + I> system_`,
`inverse_`, a `DenseLu<kMaximumNodes + I>` (note: templated on the **maximum**, and the runtime
size is passed to a size-aware `factorise(matrix, size)` overload — add that overload in Task 1's
header if it is not already there; the controller should fold this into Task 1's Step 3 rather
than let Task 5 edit Task 1's file); `std::array<double, kMaximumPotentiometers> pot_position_`;
`bool matrices_dirty_`; the four counters; `double state_limit_ = 1e4;`.

`prepare(netlist, sample_rate)`:

1. Validate counts and node indices; return `false` on failure.
2. Store the netlist and `sample_rate`; compute `Gx[k] = 2·C_k·sample_rate` for capacitors and
   `Gx[k] = 1/(2·L_k·sample_rate)` for inductors (both are the `T`-forms above with `T = 1/fs`).
3. Set every pot position to 0.5, mark dirty.
4. Call `derive()`; return its result.
5. `reset()`.

`derive()` — private, allocation-free, called from `prepare()` and from
`update_matrices_if_dirty()`:

1. Zero `system_` over `(node_count_ + I)²`.
2. Stamp resistors: for each resistor branch with conductance `g = 1/R`, add `+g` at
   `(a,a)` and `(b,b)` and `−g` at `(a,b)` and `(b,a)`, skipping any index that is ground.
   Pot sections use `R = max(pot_total · position, kMinimumPotOhms)` and
   `max(pot_total · (1 − position), kMinimumPotOhms)` with `kMinimumPotOhms = 1.0`; the floor is
   what keeps a knob at an end stop from making `S` singular, and 1 Ω is far below any pot's
   wiper resistance so it changes nothing audible. Document that.
3. Stamp reactive elements the same way with conductance `Gx[k]`.
4. Stamp sources: row `node_count_ + s` gets `+1`/`−1` at the source's nodes and the mirrored
   entries in the corresponding column.
5. Invert `system_` into `inverse_` by factorising once and solving `node_count_ + I` unit
   right-hand sides. On a singular factorisation, leave the previous matrices untouched and
   return `false` (the pot path then keeps the last good response instead of emitting garbage;
   `prepare()` propagates the `false` to the caller).
6. Read `Pleft` = `inverse_[0..node_count_)[0..node_count_)` and
   `Pright` = `inverse_[0..node_count_)[node_count_..node_count_+I)`.
7. Form the eight matrices exactly as the formulation section writes them, using the branch
   tables as sparse incidence operators. `Nx·M` is "for each state `k`, row `k` of the result is
   `M[a_k] − M[b_k]` with ground rows read as zero"; `M·Nxᵀ` is the column-wise mirror. Every
   product in the list is one of those two patterns plus a diagonal scale, so no dense
   multiply is needed and there is no temporary larger than `kMaximumNodes` wide.
8. Clear `matrices_dirty_`; return `true`.

Write a comment above `derive()` giving the flop count for the worst D6d block
(`node_count_ + I = 13`) and the ruling-3 justification for re-deriving rather than updating.

- [ ] **Step 3: Write `process_sample()`.**

```
snapshot state into state_backup_
p = D·x + E·u                                          // open_circuit_voltage_
if N > 0:
    converged = solver_.solve(adapter, nonlinear_voltage_)     // warm-started from last sample
    if !converged: non_converged_samples_++
    nonlinear.currents(nonlinear_voltage_, nonlinear_current_)
else:
    nonlinear_current_ is empty
y = G·x + H·u + K·i
x = A·x + B·u + C·i
clamp each x to +-state_limit_
if any of x, y, i, v is non-finite:
    restore x from state_backup_, zero i and v, recompute y = G·x + H·u, nan_fallback_samples_++
total_samples_++
```

The `adapter` passed to the solver is a small local struct capturing `nonlinear`,
`open_circuit_voltage_` and the `F` matrix, whose `residual_and_jacobian` computes
`r = v − p − F·f(v)` and `J = Identity − F·Jf(v)`. Build it as a stack object per sample — it
holds references only, so it is a few pointers and no allocation. Add a comment saying so, since
"allocates a struct per sample" is exactly the kind of thing a reviewer will flag.

**The `N == 0` case must compile.** Guard the Newton block with `if constexpr (N > 0)`; the
`std::array<double, 0>` members are legal and empty. The TS tone stack and the FMV depend on
this path.

- [ ] **Step 4: Write `set_potentiometer` / `update_matrices_if_dirty`.**

`set_potentiometer(index, position)` clamps `position` to `[0, 1]`, and sets `matrices_dirty_`
**only if the stored value actually changed** — an unchanged knob must cost nothing, which is
what makes calling this every block from a `SmoothedParameter` free.
`update_matrices_if_dirty()` returns `false` immediately when not dirty, otherwise calls
`derive()`. It is RT-safe. Doxygen on both stating the cadence contract: *the owning node calls
`update_matrices_if_dirty()` once per block, after its smoothers advance and before the sample
loop; it must never be called from inside the sample loop.*

- [ ] **Step 5: Write `reset()`** — zero `state_`, `nonlinear_voltage_`, `nonlinear_current_`;
`reset_statistics()`. Do **not** re-derive: a reset is a state clear, not a topology change.

- [ ] **Step 6: Smoke gate.** Add nothing to the test files (Tasks 6/7 own them); instead verify
the headers compile by adding a temporary translation unit outside the repo, or by temporarily
including both headers at the top of `tests/functional/unit/test_dk_circuit.cpp` with an
`instantiate` function that constructs `DkCircuit<0,1,1,1>`, `DkCircuit<2,0,1,1>` and
`DkCircuit<4,2,2,1>` and calls every public method once. That instantiation is genuinely useful
— **keep it** in the file as `TEST(Unit_Dsp, DkCircuitInstantiatesEveryShapeD6cAndD6dNeed)` with
`EXPECT_TRUE(circuit.prepare(...))` on a trivial netlist for each shape. `sd build && sd test -s unit`.

- [ ] **Step 7: Commit**

```bash
git add include/SushiDSP/math/dk/DkNetlist.hpp include/SushiDSP/math/dk/DkCircuit.hpp tests/functional/unit/test_dk_circuit.cpp
git commit -m "D6b: DkCircuit nodal state-space solver and DkNetlist builder, with pot re-derivation and NaN fallback"
```

---

### Task 6: Validation against analytic circuits and against `DiodeClipperCircuit`

**Files:**
- Modify: `tests/functional/unit/test_dk_circuit.cpp`
- Modify: `include/SushiDSP/circuits/DiodeClipperCircuit.hpp` (comment only)

**Depends on:** Tasks 2 and 5.

- [ ] **Step 1: Fix the false claim in `DiodeClipperCircuit.hpp`.** Replace "A minimal one-stage
WDF nonlinear circuit" with a description of what the class actually does — a scalar Newton solve
of `v = Vs − Rs·(Is·(exp(v/Vt) − exp(−v/Vt)))`, the nodal equation for a series resistor into an
antiparallel diode pair — and add: "D6b re-expresses the same circuit as `DkCircuit<0,1,1,1>`;
the two implementations are independent and `test_dk_circuit.cpp` asserts they agree to 1e-6.
Kept as a second oracle; removal is a D6c decision once `TubeScreamer` no longer references it."
Nothing else in the file changes.

- [ ] **Step 2: RC lowpass against the analytic bilinear transform.**

```cpp
TEST(Unit_Dsp, DkCircuitRcLowpassMatchesTheBilinearTransformExactly)
{
    // Node 1 = source, node 2 = output. R from 1 to 2, C from 2 to ground, probe node 2.
    // Trapezoidal DK must reproduce the bilinear transform of 1/(1 + sRC) exactly:
    //   Gc = 2C/T,  H(z) = (1 + z^-1) / ((1 + R*Gc) + (1 - R*Gc) z^-1)
    // With R = 1 kOhm, C = 100 nF, fs = 48000: Gc = 2*1e-7*48000 = 9.6e-3 S, R*Gc = 9.6,
    // so the pole is (9.6 - 1)/(9.6 + 1) = 8.6/10.6 = 0.811320754717 and the DC gain is
    // (1 + 1)/((1 + 9.6) + (1 - 9.6)) = 2/2 = 1 exactly. The -3 dB point of the analogue
    // prototype is 1/(2*pi*R*C) = 1591.549431 Hz.
    constexpr double kResistance = 1000.0;
    constexpr double kCapacitance = 100e-9;
    constexpr double kSampleRate = 48000.0;

    SushiDSP::dk::DkNetlist<1, 0, 1, 1> netlist;
    netlist.node_count = 2;
    netlist.voltage_source(1, 0);
    netlist.resistor(1, 2, kResistance);
    netlist.capacitor(2, 0, kCapacitance);
    netlist.output(2, 0);

    SushiDSP::dk::DkCircuit<1, 0, 1, 1> circuit;
    ASSERT_TRUE(circuit.prepare(netlist, kSampleRate));

    // Reference: the bilinear-transform difference equation, written out independently.
    const double conductance = 2.0 * kCapacitance * kSampleRate;
    const double a0 = 1.0 + kResistance * conductance;
    const double a1 = 1.0 - kResistance * conductance;
    double reference_input = 0.0, reference_output = 0.0;

    struct NoNonlinearity
    {
        void currents(const std::array<double, 0>&, std::array<double, 0>&) const {}
        void jacobian(const std::array<double, 0>&, std::array<std::array<double, 0>, 0>&) const {}
    } nonlinear;

    for (int sample = 0; sample < 4096; ++sample)
    {
        const double input = std::sin(2.0 * M_PI * 500.0 * sample / kSampleRate)
                           + 0.3 * std::sin(2.0 * M_PI * 7000.0 * sample / kSampleRate);
        std::array<double, 1> outputs{};
        circuit.process_sample(nonlinear, { input }, outputs);
        reference_output = (input + reference_input - a1 * reference_output) / a0;
        reference_input = input;
        ASSERT_NEAR(outputs[0], reference_output, 1e-12) << sample;
    }
    EXPECT_EQ(circuit.nan_fallback_samples(), 0u);
}
```

- [ ] **Step 3: Series RLC against its analytic resonance.** Source → R → L → C → ground, probe
across `C`. Assert (a) the measured magnitude peak of the DK block sits at the damped resonance
`f = (1/2π)·sqrt(1/(LC) − R²/(4L²))` within 0.5 %, using `tests/measure/`'s
`frequency_response` on a thin `IAudioNode` adapter, and (b) with `R = 100 Ω`, `L = 10 mH`,
`C = 100 nF`: `1/(LC) = 1e9`, `R²/(4L²) = 10000/(4·1e-4) = 2.5e7`, so
`sqrt(9.75e8) = 31224.9900...` rad/s and `f = 4969.6 Hz`; the undamped `f0` is
`1/(2π·sqrt(1e-3·1e-7))` — write both numbers in the comment and assert against the damped one.
Use a run rate of 384 kHz so the trapezoidal warping at 5 kHz is negligible (the bilinear
frequency warping at `f/fs = 0.013` is under 0.01 %); state that in the comment, because
asserting an analogue resonance against a discretised circuit at 48 kHz would fail for a
legitimate reason and someone would then loosen the tolerance.

- [ ] **Step 4: Diode clipper — DK vs the existing solver.**

```cpp
TEST(Unit_Dsp, DkCircuitDiodeClipperAgreesWithTheExistingScalarSolver)
{
    // DiodeClipperCircuit(Rs, Is, Vt) solves  v = Vs - Rs*Is*(exp(v/Vt) - exp(-v/Vt)).
    // The DK version is node 1 = source, node 2 = the diode node, Rs from 1 to 2, and TWO
    // antiparallel SiliconDiode branches from 2 to ground - a pair, not a single symmetric
    // element, so D6c can make it asymmetric later. Using SiliconDiode with n = 1 and the same
    // Vt reproduces the scalar model's law exactly except for its -1 offsets, which cancel
    // between the two antiparallel branches.
    constexpr double kSourceResistance = 2200.0;
    constexpr double kSaturationCurrent = 2.52e-9;
    constexpr double kThermalVoltage = 0.025864;
    ...
}
```

The DK netlist is `DkCircuit<0, 2, 1, 1>` with branches `(2 → 0)` and `(0 → 2)`; the
`NonlinearSystem` glue holds one `SiliconDiode` and fills a diagonal 2×2 Jacobian. Drive both
with the same 2048-sample signal — a 220 Hz sine ramped from 0.05 V to 8 V amplitude, so the
sweep crosses the whole knee — and assert `EXPECT_NEAR(dk_output, scalar_output, 1e-6)` per
sample. If the two disagree by more than 1e-6, the likely cause is the `−1` offsets and the
antiparallel cancellation argument above; verify it algebraically before touching a tolerance.
Also assert `non_converged_samples() == 0` and `nan_fallback_samples() == 0`.

- [ ] **Step 5: Potentiometer re-derivation.**

```cpp
TEST(Unit_Dsp, DkCircuitPotentiometerChangesTheResponseAndCostsNothingWhenUnchanged)
{
    // A 10 kOhm pot as a voltage divider into a 100 nF cap: at position p the DC gain from the
    // source to the wiper is exactly p (an ideal source drives the top of the pot), so a DC
    // input of 1 V settles to p at the output. Positions 0.25, 0.5, 0.9 give 0.25, 0.5, 0.9.
    ...
    EXPECT_FALSE(circuit.update_matrices_if_dirty());     // nothing moved
    circuit.set_potentiometer(0, 0.5);
    EXPECT_FALSE(circuit.update_matrices_if_dirty());     // same value, still not dirty
    circuit.set_potentiometer(0, 0.9);
    EXPECT_TRUE(circuit.update_matrices_if_dirty());
}
```

Also assert both end stops (`0.0` and `1.0`) still `prepare`/derive successfully and produce
finite output — that is the `kMinimumPotOhms` floor's test.

- [ ] **Step 6: NaN and state-limit policy.**

```cpp
TEST(Unit_Dsp, DkCircuitNeverEmitsANonFiniteSampleAndCountsItsFallbacks)
{
    // A nonlinearity that deliberately returns inf on one sample must not poison the state.
    ...
    EXPECT_TRUE(std::isfinite(outputs[0]));
    EXPECT_EQ(circuit.nan_fallback_samples(), 1u);
    // and the sample after the poisoned one is back on the normal trajectory
}
```

Plus a case driving the RC circuit with a 1e30 V input and asserting the output stays finite and
the state stops at `state_limit_`.

- [ ] **Step 7: `sd build && sd test -s unit` all green. Commit**

```bash
git add tests/functional/unit/test_dk_circuit.cpp include/SushiDSP/circuits/DiodeClipperCircuit.hpp
git commit -m "D6b: DkCircuit validated against analytic RC/RLC, against DiodeClipperCircuit within 1e-6, and on pot re-derivation; corrected the false WDF claim in DiodeClipperCircuit"
```

---

### Task 7: Common-cathode 12AX7 stage vs the SPICE reference, and convergence statistics

**Files:**
- Modify: `tests/functional/unit/test_dk_triode_stage.cpp` (replace stub)

**Depends on:** Tasks 3 and 5. Uses `tests/measure/SpiceCsv.hpp` and `tests/measure/NullTest.hpp`
from D6a, and `SUSHIDSP_SPICE_DIR` from Task 0.

**The circuit is `docs/reference/spice/jcm_v1a/README.md`'s netlist, node for node** — that is
deliberate: this test is simultaneously D6b's SPICE acceptance and D6d block 1's prototype, so
D6d inherits a circuit that is already known to null.

| Node | 1 | 2 | 3 | 4 | 5 | 6 | 7 |
|---|---|---|---|---|---|---|---|
| name | src | g | p | k | pin | out | B |

Elements: `Rgleak 1 MΩ` (src–gnd), `Rgs 68 kΩ` (src–g), `Rp 100 kΩ` (B–p), `Rk 2.7 kΩ` (k–gnd),
`Ck 0.68 µF` (k–gnd), `Cc 0.022 µF` (p–pin), `Cbr 470 pF` (pin–out), pot `1 MΩ` (pin–out–gnd,
wiper = out), nonlinear branches `(g → k)` and `(p → k)` for `Ig` and `Ip`, sources `V1` (src–gnd)
and `Vplate = 360 V` (B–gnd), output probe `(out–gnd)`. That is `DkCircuit<3, 2, 2, 1>` with
`node_count = 7`.

**Bias check first.** Before any null test, assert the operating point: with `V1 = 0` and
`Vplate = 360 V`, run 200 ms of silence at 384 kHz and read the settled node voltages. Kirchhoff
plus the Koren law give `Ip·Rk = −Vgk` and `Vpk = 360 − Ip·(Rp + Rk)`; solving that pair with the
Task 3 model gives a cathode voltage near 1.3–1.5 V and a plate near 200–230 V. Assert
`0.8 V < Vk < 2.2 V` and `150 V < Vp < 280 V` — a band wide enough to survive a legitimate model
detail and narrow enough that a sign error in a stamp fails it instantly. **The implementer must
compute the exact fixed point by hand (two-variable Newton on paper or in a scratch script) and
put the numbers in the test comment**, then keep the assertion band as stated. A settled
operating point that lands outside the band is a real bug, not a tolerance problem.

- [ ] **Step 1: Write the bias test and the convergence test** (both run with no CSV present, so
they gate immediately).

```cpp
TEST(Unit_Dsp, DkTriodeStageNewtonConvergesOnNinetyNinePointNineNinePercentOfSamples)
{
    // Spec D6b.4: <= 0.01 % non-converged, zero NaN, over the full stimulus set. The set is a
    // 2-second sweep 20 Hz -> 20 kHz at -12 dBFS scaled to volts by kFullScaleVolts, plus a
    // 1 kHz burst at 0.05 V, 0.5 V and 5 V (well past grid conduction), all at 384 kHz.
    ...
    const double ratio = 100.0 * static_cast<double>(circuit.non_converged_samples())
                       / static_cast<double>(circuit.total_samples());
    std::printf("[ measure ] jcm_v1a DK: %.4f %% non-converged, max %u iterations, %llu NaN fallbacks\n",
                ratio, circuit.maximum_iterations_seen(),
                static_cast<unsigned long long>(circuit.nan_fallback_samples()));
    EXPECT_LE(ratio, 0.01);
    EXPECT_EQ(circuit.nan_fallback_samples(), 0u);
}
```

Copy the printed line into the commit message; D6d compares against it.

- [ ] **Step 2: Write the SPICE null test with the skip contract.**

```cpp
namespace
{
    /// @brief Path of a generated LTspice CSV; empty CSVs mean "not generated yet".
    std::string spice_path(const char* block, const char* kind)
    {
        return std::string(SUSHIDSP_SPICE_DIR) + "/" + block + "/" + block + "_" + kind + ".csv";
    }
}

TEST(Unit_Dsp, DkTriodeStageNullsAgainstTheLtspiceTransientReference)
{
    const std::string path = spice_path("jcm_v1a", "tran");
    const auto reference = SushiDSP::measure::load_transient_csv(path);
    if (reference.input.empty())
    {
        GTEST_SKIP() << "No LTspice reference at " << path
                     << ". Generate it with: python tools/spice/export.py --block jcm_v1a "
                     << "(needs LTspice installed; see docs/reference/spice/README.md), then commit the CSV.";
    }
    ...
}
```

When the CSV *is* present: for each of the three `setting` groups in the file, drive the DK
circuit with `reference.input` at `reference.sample_rate` (192 kHz per D6a's export contract),
discard the first 5 ms as settling, low-pass both signals at 10 kHz before comparing (spec's
Risks section: LTspice's integrator at 192 k and ours disagree in the top octave, so the null is
evaluated band-limited), and assert
`EXPECT_LE(SushiDSP::measure::null_test(reference_band_limited, dk_band_limited, 256), -60.0)`.
The band-limiting filter is a 4th-order Butterworth built from two `Biquad`s at 10 kHz, applied
identically to both signals so it cannot flatter either one; say so in a comment.

Add the same shape for the AC sweep: `load_ac_csv`, skip if empty, otherwise compare the DK
block's `frequency_response` (measured at −40 dBFS so the stage is small-signal, which is what an
LTspice `.ac` analysis linearises around) to the CSV within ±0.5 dB from 20 Hz to 10 kHz, per
`setting`. The DK block is wrapped in a throw-away `IAudioNode` adapter local to this file that
casts `float ↔ double` at its boundary — that adapter is the first instance of the D6c/D6d node
pattern, so write it cleanly and comment it as the template the product modules will follow.

- [ ] **Step 3: `sd build && sd test -s unit`** — expected: bias and convergence cases PASS, the
two CSV cases SKIP with their messages visible in the output. Confirm the skip messages actually
print (`sd test` should surface them; if not, run the binary directly once to check) — a skip
nobody reads is the same as a deleted test.

- [ ] **Step 4: Commit**

```bash
git add tests/functional/unit/test_dk_triode_stage.cpp
git commit -m "D6b: common-cathode 12AX7 stage on DkCircuit - bias point, Newton convergence <x> %, SPICE nulls pending an LTspice run"
```

---

### Task 8: RT-safety traps, benchmark, documentation

**Files:**
- Modify: `tests/functional/unit/test_rt_safety.cpp`
- Modify: `tests/benchmark/main.cpp`
- Modify: `docs/design/sushidsp.md` (§4.5), `docs/reference/CHANGELOG.md`, `docs/README.md`

**Depends on:** everything.

- [ ] **Step 1: RT traps.** Following the existing `operator new` trap pattern in the file, add:

```cpp
TEST(Unit_Dsp, DkCircuitProcessPerformsNoHeapAllocation)
TEST(Unit_Dsp, DkCircuitPotentiometerUpdatePerformsNoHeapAllocation)
TEST(Unit_Dsp, DampedNewtonSolverPerformsNoHeapAllocation)
```

Each constructs and `prepare`s **outside** the trap window (prepare is control thread and is
allowed to be slow, though in fact it allocates nothing either), then arms the trap and runs
256 samples / one `update_matrices_if_dirty()` after a real position change / 256 solves. The
pot case is the one that matters: it is the only place where an audio-thread code path re-runs
a derivation, and Ruling 3 stands or falls on it. Assert `g_trapped_allocation_count == 0`.

- [ ] **Step 2: Benchmark case.** In `tests/benchmark/main.cpp` add a case named
`"DkCircuit 2-nonlin 4-state @8x"`: the `jcm_v1a` circuit from Task 7 (2 nonlinear branches,
3 states) plus one extra series RC to reach 4 states, prepared at `48000 × 8 = 384000` Hz, driven
through a 256-frame block of the existing 220 Hz tone with the oversampling loop unrolled inline
(2048 solver calls per block). Print it with a budget of **0.12** ms:

```cpp
print(measure_dk_circuit(), 0.12);
```

Add a second case, `"DkCircuit pot update"`, timing 1 block where a pot moves on every block, so
Ruling 3's re-derivation cost is a committed number rather than an estimate.

Run `sd test -t release -s benchmark`. Put both numbers in the commit message. **If the 0.12 ms
budget is missed**, do not loosen it: report DONE_WITH_CONCERNS with the measured figure and the
profile, and name the two levers this plan already identified — a joint
`current_and_jacobian` on the models (Task 3, Step 4 note) and specialising `DenseLu<2>` — so
D6c/D6d can decide with data.

- [ ] **Step 3: Documentation.**
  - `docs/design/sushidsp.md` §4.5: rewrite the solver-layering paragraph as "As built in D6b" —
    DK is the circuit solver; `Wdf.hpp` remains the two elements it always was and is deferred
    indefinitely (spec "Decisions taken"); the layering is
    `math/dk/{DenseLu, DampedNewtonSolver, DkNetlist, DkCircuit}` with
    `circuits/models/{SiliconDiode, Triode12AX7, PentodeEL34}` above it and no dependency from
    `math/` on `circuits/`. State Ruling 6's corrected branch counts (a triode is two branches,
    a pentode three) explicitly, since §D6d's numbers in the spec are wrong and someone will
    otherwise trust them.
  - `docs/reference/CHANGELOG.md`: a D6b section listing the new headers, the measured Newton
    convergence figure from Task 7, the two benchmark numbers, and the fact that no product
    module changed yet.
  - `docs/README.md`: index entries for `include/SushiDSP/math/dk/` and
    `include/SushiDSP/circuits/models/`.

- [ ] **Step 4: `sd test -s all` green; `sd doxygen` produces no new warnings for the new headers. Commit**

```bash
git add tests/functional/unit/test_rt_safety.cpp tests/benchmark docs
git commit -m "D6b: RT-safety traps for the DK path, benchmark cases (<a> ms/256 at 8x, budget 0.12), design doc and changelog"
```

---

## No Placeholders

Every code block above is either complete and compilable as written, or is explicitly marked as a
shape whose body the step describes in prose (Task 5's `derive()` steps, Task 6 Steps 3/5/6, Task
7 Step 2). Nothing is stubbed "for later". `<x>` and `<a>` in commit messages are values produced
by the step immediately above them. The three sets of hand-computed model values in Tasks 2, 3
and 4 are the real expectations, computed at double precision while writing this plan, with the
intermediate quantities in the comments so an implementer can re-derive them without running the
code they are testing. The SPICE-consuming assertions in Task 7 are written to run, and skip only
because the CSVs do not exist yet (D6a Ruling 3) — the skip carries the command that produces
them.

## Self-review

- **Spec coverage.** §D6b.1 formulation → the "DK formulation" section, transcribed into Task 5;
  trapezoidal discretisation at the run rate → `prepare(netlist, sample_rate)` where the caller
  passes base × oversample; variable-resistor extension → Task 5 Step 4 (by Ruling 3, as
  re-derivation rather than as an extra Newton branch). §D6b.2 fixed-size `std::array` storage,
  double state, `DkNetlist` builder, LU + damped Newton + counters + N=1 fast path →
  Tasks 1 and 5; the `DiodeClipperCircuit` WDF-comment correction and DK re-expression →
  Task 6 Steps 1 and 4. §D6b.3 all three models with `current`/`jacobian` in double, C¹, exact
  `.sub` constants, three hand-computed operating points each → Tasks 2, 3, 4. §D6b.4 RC/RLC
  analytic → Task 6 Steps 2–3; diode clipper within 1e-6 → Task 6 Step 4; 12AX7 vs SPICE →
  Task 7; convergence ≤ 0.01 % and zero NaN → Task 7 Step 1 and Task 6 Step 6; RT traps and the
  0.12 ms benchmark → Task 8.
- **Consumer contract for D6c/D6d.** `DkCircuit<S,N,I,O>` shapes used downstream are all
  instantiated in Task 5 Step 6; `nonlinear_currents()` is the sag hand-off (Ruling 7); supplies
  are input-vector entries; the `IAudioNode` float↔double adapter pattern is written once in
  Task 7 Step 2 for D6c/D6d to copy.
- **Type consistency.** `DenseLu<N>::factorise` gains the size-aware overload in Task 1 (folded
  in by the controller) so Task 5 never edits Task 1's file. `softplus`/`logistic` live in
  `SoftTransition.hpp`, created by Task 3 and consumed by Task 4 — the only Group-A ordering
  constraint, and it is called out in Task 4's header.
- **Where the risk is.** Task 5 is the largest single task and has no tests of its own; it is
  gated entirely by Tasks 6 and 7, which are dispatched immediately after. If Task 5's
  derivation is wrong, Task 6 Step 2 fails first and most legibly (the RC pole is a single number
  to compare against), so run that step before the others. Dispatch Task 5 on the most capable
  model available.
- **Known open questions**, carried to the controller rather than guessed at: the EL34 idle
  current the Koren fit produces at −37 V (33.4 mA) is at the high end of a real 2203's idle and
  may indicate the default bias in spec §D6d needs moving; and whether `kMaximumNodes = 16`
  suffices for D6d block 5 is not verifiable until that block's netlist is written.
