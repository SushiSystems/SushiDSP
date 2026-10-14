# tests/foundation/math/dk

## `test_damped_newton_solver.cpp`

### `DenseLuPivotsWhenTheLeadingEntryIsZero`

`// Without partial pivoting the first step divides by zero.`

[[0, 1], [1, 0]] x = [3, 5] has the exact answer x = [5, 3]; without partial pivoting the first
elimination step divides by zero.

### `DenseLuSolvesTheLeadingBlockOfAnOversizedArray`

`// Only the leading 3x3 block may be touched.`

The same hand-checked 3x3 system, stored in the top-left corner of a 5x5 array whose remaining
rows are the zeros a circuit leaves behind when it uses fewer ports than the compile-time
maximum. Only the leading 3x3 block may be touched.

### `DampedNewtonWithHalvingDisabledFailsOnTheDivergentStart`

`// With no halving, every iteration takes the full step.`

The same start with the halving budget set to zero: every iteration is forced to take the full
Newton step, which is exactly the divergence damping exists to prevent. This pins
set_maximum_halvings() to observable behaviour, and it also pins the stall guard: out at |v| \~
1e168 the Jacobian 1/(1 + v^2) underflows, the perturbed solve returns a step that is negligible
next to |v|, and a step-size-only convergence test would report that runaway point as a solution
while the residual is still atan -> pi/2.

### `DampedNewtonWarmStartConvergesInOneIterationOnASmoothTrack`

`// A warm start needs at most a couple of iterations.`

Sweeping the root slowly and warm-starting from the previous answer is the audio-rate usage
pattern; it must not need more than a couple of iterations per step.

### `DampedNewtonReturnsTheBestIterateWhenItFailsToConverge`

`// A failed solve returns its lowest-residual iterate.`

One iteration from the divergent start with halving disabled: the only step available is the
full Newton step, which lands at |atan| larger than where it started. The step is still taken -
that is how the iteration escapes a flat region - but solve() returning false must hand back the
lowest-residual iterate it saw, as its @param v contract says.

### `DenseLuTwoByTwoSolvesTheSameSystemsAsTheGeneralTemplate`

`// [[4, 3], [6, 3]] x = [10, 12] has the answer x = [1, 2].`

[[4, 3], [6, 3]] x = [10, 12] has the exact answer x = [1, 2]:

```text
4*1 + 3*2 = 10,  6*1 + 3*2 = 12.   det = 4*3 - 3*6 = -6.
```

### `DampedNewtonLeavesItsLastEvaluationAtTheIterateItReturns`

`// The default budget of 8 iterations is too tight here.`

Each damped step moves the iterate by roughly -1 while it is far from the root (step = -r/r' =
-(1 - exp(0.25 - v))), so the true worst case is v = 12.0: about 14-15 iterations of that
near-unit step before the quadratic tail takes over. The v = -5.0 start first overshoots past
the root, since r' = exp(-5) is tiny there, but the line search clips that step back into the
unit-step regime, landing near v = 7 before it needs 10+ more of them. The default budget of 8
is too tight for either start.
