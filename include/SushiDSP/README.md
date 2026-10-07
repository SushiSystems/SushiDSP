# `include/SushiDSP` — the core library's public headers

These are the headers of the core library. Their implementation is in [`src/`](../../src/README.md),
which mirrors the folders below.

| Folder | Holds |
| --- | --- |
| `audio_format/` | Hardware sample formats, device channel selection |
| `circuits/` | Circuit blocks: tone stack, equaliser, feedback, clamp |
| `circuits/models/` | Stateless device laws: the 12AX7, the EL34, the silicon diode |
| `circuits/stages/` | Tube stages, the output transformer, supply sag, the antialiaser |
| `core/` | The node and device interfaces, `RigGraph`, the real-time slots, the buffer pool |
| `device/` | The ASIO and SDL2 devices |
| `io/` | Impulse responses and WAV files |
| `log/` | The logger, its sinks, the `{}` formatter and the `SD_LOG` macros |
| `math/` | Filters, FFT, convolution, resampling, oversampling, the scalar Newton solver |
| `math/dk/` | The nodal DK method: netlist, circuit, LU, damped Newton |

## Notes cited from the source

A comment in these headers states one invariant and names this file. The reasoning behind each
invariant follows, under the name of the file that cites it.

### `circuits/models/PentodeEL34.hpp`

`current_and_jacobian` clamps the screen voltage at `kScreenFloorVolts` before dividing by it.
Below the floor the voltage the model sees no longer varies with `v[1]`, so the whole Vg2k column
of the Jacobian is zero there. The clamp makes that column one-sided at exactly
`v[1] == kScreenFloorVolts`: the derivative reported at that point is the right-hand one, the
unclamped column, while the left-hand one is zero. A 1 mV screen supply is nonphysical, so no
operating point sits on that seam. The clamp exists only to keep `vg1k / vg2k` finite for a solver
that wanders there.

The plate and screen laws raise E1 and z to `kx - 1`. Since `kx > 1`, both powers vanish
continuously as their arguments reach zero. The two `max(0, .)` clamps are therefore C1 and not
slope discontinuities, and the single guard on each power carries its whole branch, current and
derivative alike.

### `circuits/models/SiliconDiode.hpp`

The defaults (Is = 2.52 nA, n = 1.752, Vt = 0.025864 V) match the 1N914 small-signal diode as
modelled in `docs/reference/spice/models/d1n914.sub`. `current()` and `jacobian()` implement

```
n*Vt  = ideality_factor * thermal_voltage_volts
i(v)  = Is * (exp(v / (n*Vt)) - 1)
di/dv = (Is / (n*Vt)) * exp(v / (n*Vt))
```

A Newton-Raphson solver can overshoot far past any physically meaningful diode voltage, and
`exp()` of a large argument overflows to `inf`. The exponent argument is clamped to +-40, which
puts `v_max = 40*n*Vt` near 1.81 V for the 1N914 defaults; no silicon diode in a guitar circuit
sits above that voltage. Clamping the exponent alone would break C^1 continuity at the knee, so
beyond either knee the model switches to the Shockley law's own tangent line there:
`i(v) = i(k) + jacobian(k) * (v - k)` with `jacobian(v) = jacobian(k)`, taking `k = +v_max` above
the upper knee and `k = -v_max` below the lower one. The result is C^1 everywhere, finite
everywhere, and identical to the Shockley law for `|v| <= v_max`.

The two tangents are not each other's mirror image, and must not be. The reverse tangent continues
a diode that is not conducting, so it carries -Is and a conductance of `(Is/(n*Vt))*exp(-40)`,
while the forward one carries hundreds of megamperes per volt. Mirroring the forward tangent put a
step from -2.5 nA to -593 MA at the lower knee (D6c C2).

A DK solver wants the current and its derivative at the same voltage, so `current_and_jacobian()`
carries the law and `current()` and `jacobian()` forward to it.

`current_and_antiderivative()` is piecewise with `current_and_jacobian()` by construction. The
Shockley branch integrates to `Is*(n*Vt*exp(v/(n*Vt)) - v)`, and beyond either knee the tangent
line integrates to the quadratic matched to it in value there, so the antiderivative is C^1
wherever the current is C^0. Its constant of integration is the Shockley branch's own and cancels
in any difference of two evaluations, which is the only way antiderivative antialiasing reads it
([`2026-08-26-aaa-audio-quality-design.md`](../../docs/archive/superpowers/specs/2026-08-26-aaa-audio-quality-design.md),
Model corrections D6c C2). An antiparallel pair wants both quantities at both polarities, so
pairing them costs one exponential per side where separate calls would cost two, exactly as
`current_and_jacobian()` pairs the Newton quantities.

### `circuits/models/SoftTransition.hpp`

`softplus_and_logistic` returns `t` itself for `t >= 30`. `ln(1 + exp(t)) - t` is 9.24e-14 at
`t = 30` and falls from there, so the linear branch is exact to double precision and keeps
`exp(t)` away from its overflow near 709.

### `core/DenormalGuard.hpp`

Every decaying state on the audio path (the halfband delay lines, the biquads, the coupling
capacitors, the `exp()` tails of the tube residuals) drifts through the denormal range on its way
to silence. On x86 a denormal operand costs on the order of a hundred times a normal one through a
microcode assist. Flushing denormals to zero is inaudible, since they sit near 1e-38 below full
scale, and it removes a CPU spike that shows up exactly when the signal is quiet. The mode is per
thread, so a guard on the audio thread changes nothing for the rest of the process.

### `core/ICabinetHost.hpp`

`set_cabinet` builds the convolver in full on the control thread and hands it to the audio thread
as one atomic pointer swap, so `process()` never sees a partially built cabinet and never blocks.
The node retains the samples so that a later `prepare()` rebuilds the cabinet. It never resamples
them, so a `prepare()` at a different rate needs a fresh `set_cabinet()`. In every failing case
nothing is installed and nothing is retained.

### `core/RtSwapSlot.hpp` and `core/RtGraphSlot.hpp`

`RtGraphSlot` is the same mechanism as `RtSwapSlot<T>` with `RigGraph` in place of `T`; its
functions are defined in `src/core/RtGraphSlot.cpp` and the notes below cover both.

A non-RT thread builds a whole new object and publishes it. The audio thread reads the active
object with one atomic load per callback. `reclaim()`, on the non-RT thread, frees a retired object
only once the audio thread has provably moved past it.

The threading contract has two halves. `publish()` and `reclaim()` must both be called from a
single, consistent non-RT thread; callers that need either from more than one thread synchronise
those calls themselves, because the class does not. `acquire_for_audio_thread()` must be called
from a single, consistent audio thread. The pointer it returns is valid only until that same
thread's next call to `acquire_for_audio_thread()` and must not be retained across calls: a
later `reclaim()` may free the object once this thread has moved past it.

`publish()` treats `active_` as a non-owning view of the newest entry in `retired_`. Ownership of
every object ever published lives in `retired_` from the moment it is inserted, keyed by the
generation it was published at. The value `store()` replaces needs no handling: the object it
pointed at already sits in `retired_` from its own `publish()` call, and `reclaim()` is what
eventually frees it. The order of the three steps matters for two reasons.

1. Exception safety. `retired_.emplace_back()` must take ownership of the new object before
   `active_` is ever pointed at it. If `emplace_back()` throws, for instance `std::bad_alloc`
   while growing the vector, the local `unique_ptr` unwinds and frees the object it owns. Had
   `active_` already been stored, it would be left dangling and visible to the audio thread.
2. The audio thread's invariant. `active_` must be stored strictly before `publish_generation_`
   observably advances, so that reading the generation before `active_` on the audio thread can
   never race ahead of a publish still in flight.

Since only one non-RT thread publishes, there is no concurrent `publish()` to race with. The next
generation is computed locally with no atomic read-modify-write, the object moves into `retired_`
first, then `active_` is stored, and only then is the new generation number published.

`acquire_for_audio_thread()` reads the generation before `active_`. Because `publish()` stores
`active_` strictly before bumping `publish_generation_`, any object published at a generation up
to k, the value read, has already had its `active_` store land. Reading `active_` second therefore
always observes an object whose generation is at least k, and `reclaim()`, bounded to generations
below k, can never free the object this call is about to return.

In `RtSwapSlot::reclaim()` a zero watermark means `acquire_for_audio_thread()` has never returned
a non-null object. No retired entry can then be held by the audio thread, and every entry but the
active object is past.

### `device/AsioAudioDevice.hpp`

`active_instance_` is the target of the trampolines ASIO's free-function callbacks go through. The
SDK assumes one open driver process-wide. A second `AsioAudioDevice` open at the same time is a
programming error that the class documents and does not guard at run time, which is how this
codebase treats constructor-level preconditions elsewhere.

### `log/`

One brick per header. `LogLevel`, `LogCategory` and `LogRecord` are the values a line is made
of. `LogArgument` prints one value and `LogFormatter` walks the `{}` markers. `ILogSink` is the
interface a line leaves through, and `StderrLogSink`, `AsyncLogSink` and `CapturingLogSink`
implement it. `LogLineQueue` is the bounded queue inside `AsyncLogSink`. `Logger` owns the
current sink and the runtime minimum level, `DefaultLogger` builds the logger of the process,
`ScopedLogSink` installs a sink for a scope, and `Log.hpp` defines the macros.

A call reads `SD_LOG_WARN(K_ROOM_LOG_CATEGORY, "format {}", value)`. Each module that logs
declares its one category in `<module>_log_category.hpp` beside its own headers. The formatter
takes `{}` and nothing else: a value that needs a width, a precision or hex is turned into a
string before the call. It prints a bool, a `char`, any integer, a `float` or `double` with six
significant digits, a C string, a `std::string` and a `std::string_view`. A message longer than
`kMaxLogMessageLength`, 1024 characters, is cut there.

A level below `SD_LOG_MIN_LEVEL` compiles to `((void)0)` and its arguments are never evaluated.
The constant is `SD_LOG_LEVEL_INFO` when `NDEBUG` is defined and `SD_LOG_LEVEL_TRACE` otherwise,
and a translation unit may define it before including `Log.hpp`. A level that is compiled in but
below `Logger::minimum_level()` is skipped at run time, again without evaluating its arguments.
The logger of the process starts at `LogLevel::Info` with a `StderrLogSink`, so a line logged
before an application installs a sink still reaches stderr.

### `log/Logger.hpp` and `log/AsyncLogSink.hpp`

The audio thread never logs. `Logger::log` formats on the caller's stack and takes a mutex for
the length of one `shared_ptr` copy, which is cheap and still not something a real-time callback
may do. A failure on the audio thread is counted in an atomic, the way
`TrackRecorder::dropped_blocks()` counts a refused block, and the control thread logs the count.

An application installs an `AsyncLogSink` over a `StderrLogSink` when it starts, through a
`ScopedLogSink` that flushes it on the way out of `main`. After its constructor has allocated
the queue and started the worker, `AsyncLogSink::write` copies the line into a slot of the queue
and returns. It takes no lock and allocates nothing. When the queue is full the line is dropped
and counted, and the worker writes one `WARN` under the `log` category that says how many lines
were lost. The wake-up `write` sends takes no lock either, so it can arrive just before the
worker goes to sleep and be missed; the worker's wait ends after 50 ms at the latest and the
line is written then. `flush()` returns once every line queued before the call has been written
and the sink below has been flushed.

Tests install a `CapturingLogSink`, which stores each line before `write` returns, so the order
they read is the order the code logged in.

### `math/Biquad.hpp`

`set_first_order_high_pass` gives the shape a coupling capacitor into a grid leak has. It stays
stable at corners far below where the second-order `set_high_pass` does: its pole sits on the real
axis, where a float ulp moves the radius by an ulp and cannot collapse a conjugate pair onto the
unit circle.

### `math/HalfbandCoefficients.hpp`

`tools/dsp/design_halfband.py` writes this file; a hand edit is lost at the next run. The
committed tables came from scipy 1.18.1 and numpy 2.5.2 with `remez` at `grid_density=32`, checked
on a verification grid of 65536 points.

| Stage | Runs at | Taps | Band edges | Measured stopband | Measured passband ripple |
| --- | --- | --- | --- | --- | --- |
| base to 2x | 2 f_base | 115 | passband 0.45 f_base, stopband 0.55 f_base | -90.12 dB | 0.00027 dB |
| 2x to 4x | 4 f_base | 27 | passes 0.55 f_base, rejects its image at 1.45 f_base | -101.60 dB | 0.00007 dB |
| 4x to 8x | 8 f_base | 15 | passes 0.55 f_base, rejects its image at 3.45 f_base | -110.69 dB | 0.00003 dB |

The comments in the committed file differ from what the generator writes. On 2026-10-05 the file
block, the `// Generated by` line and the `@brief` above each tap table were written by hand, and
`design_halfband.py` (lines 74 and 85) still emits the older `// GENERATED` header and a two-line
note per table. The tap values are the generator's own. A regeneration brings the old comments
back until the generator emits the committed text.

### `math/NewtonRaphsonSolver.hpp`

`solve_joint` is a template and not a `std::function` on purpose. Every caller sits on the audio
thread and builds a fresh capturing lambda per sample, and a capture list wider than the standard
library's small-buffer optimisation (16 bytes on libstdc++) would make `std::function`
heap-allocate inside `process()`. A template instantiates the callable inline, with no allocation
on any platform.

Warm-starting from the previous sample's solution is what makes two to four iterations enough at
audio rates. Circuits with exponential residuals, the diode models among them, must bound the
trial point. Without bounds a single overshoot evaluates `exp()` past the float range and leaves
`inf` or `nan` in the warm-start state for good. For the same reason a non-finite step ends the
iteration on the last good iterate.

A step that lands exactly on the current iterate is a fixed point in float. Every further
iteration would recompute the same value, so stopping there changes nothing but the cost.
Warm-started audio-rate solves reach this on most samples well before `max_iterations`.

`solve()` exists for residuals whose derivative shares no work with the residual itself.

### `math/ResamplerDesign.hpp`

`tools/dsp/design_resampler.py` writes this file together with `src/math/ResamplerKernel.hpp`; a
hand edit is lost at the next run. The committed design came from scipy 1.18.1 and numpy 2.5.2,
checked on a verification grid of 524288 points. It measures a passband ripple of 0.0001 dB over
[0, 0.4], a stopband of -100.48 dB over [0.5, 256.0], a polyphase DC sum spread of 5.62e-06 and a
linear-interpolation bound of 1.1471e-06.

`HALF_WIDTH` is 34. The plan called for 32, and the DR1+DR2 plan's Task 1.1 ruling grew it to
clear -100 dB with margin
([`2026-09-05-dr1-dr2-resampler-and-dd3.md`](../../docs/archive/superpowers/plans/2026-09-05-dr1-dr2-resampler-and-dd3.md)).
The latency floor is 0.7083 ms per stage at 48.0 kHz.

The comments in the committed file differ from what the generator writes. On 2026-10-05 the file
block and the `// Generated by` line were written by hand, and the `@brief` of
`kResamplerPassbandEdge` lost the word "above", which pointed at the measurement comment that
moved here. `design_resampler.py` (lines 137 to 139 and 213) still emits the older text, for this
file and for `src/math/ResamplerKernel.hpp`. The constants are the generator's own. A regeneration
brings the old comments back until the generator emits the committed text.

### `math/dk/DampedNewtonSolver.hpp`

The system must evaluate the residual and the Jacobian together. The solver never asks for one
alone, so a system may share the exponentials between them. Every buffer is a member `std::array`,
which is why `solve()` is safe on the audio thread. The statistics are counters the host can poll
to see how hard the circuit is working. Warm-starting `v` from the previous sample is the intended
use.

A step that stopped moving without reducing the residual is a stall, not convergence, and the
solver keeps iterating. That distinction stops a Jacobian that underflows far from the root from
being reported as a solution.

On success the last call `solve()` made to `System::residual_and_jacobian` was made at the iterate
it leaves in `v`: the tolerance return happens with the residual belonging to the current `v`, and
every step return sets `v` to the trial point the line search last evaluated. A system that caches
per-evaluation quantities may therefore reuse them after a `true` return. On failure it may not,
because `fail()` restores the lowest-residual iterate seen, which is generally not the last one
evaluated. `test_damped_newton_solver.cpp` pins this.

### `math/dk/DenseLu.hpp`

All storage is member `std::array`. `DkCircuit` relies on the factors persisting when it inverts
the augmented nodal matrix column by column. The size-aware overloads let a caller hold one
compile-time-maximum array and factorise only the leading block it filled.

### `math/dk/DkCircuit.hpp`

`DkCircuit` implements the nodal DK method (Holters and Zoelzer) with the trapezoidal rule, as
transcribed in the
[D6b plan](../../docs/archive/superpowers/plans/2026-08-26-d6b-dk-circuit-core.md). `prepare()`
runs the derivation once. After that each sample costs one Newton solve on the N-dimensional
nonlinear port plus the five small matrix products of the state-space update. Potentiometers are
re-derived and not carried as Newton branches (D6b plan, ruling 3), which is what makes a knob
cost nothing per sample.

The caller supplies the glue between the branch vector and its device models as a
`NonlinearSystem` with one method, `currents_and_jacobian`, which fills both outputs from a single
evaluation. Newton never wants one without the other, and the exponentials a model shares between
them are most of the per-sample cost. The Jacobian buffer is zeroed before every call, so a glue
struct for independent devices leaves the off-diagonal blocks alone. Nothing in the header
includes a circuit model: `DkCircuit` stays in `SushiDSP/math` with no dependency on
`SushiDSP/circuits`.

`process_sample()`, `update_matrices_if_dirty()` and everything they reach allocate nothing, take
no lock, make no syscall and throw nothing. Every state is clamped each sample and a non-finite
sample restores the previous state, so the block cannot emit a non-finite output whatever is fed
to it (D6b plan, ruling 10). Warm-starting Newton from the previous sample's port voltages keeps
it at one to three iterations on an oversampled path.

`HasCurrentsForUpdate` exists so that a port can stamp something other than f(v) at the converged
operating point without `DkCircuit` knowing what. The port is handed the open-circuit voltage p,
the solved port voltage v and the port impedance matrix F, which together are the whole of the
memoryless map from p to i that the block's linear part sees. Antiderivative antialiasing is the
reason the hook exists
([`2026-08-26-aaa-audio-quality-design.md`](../../docs/archive/superpowers/specs/2026-08-26-aaa-audio-quality-design.md),
Model corrections D6c C2).

The reactive companion conductance `Gx` follows the D6b plan's "Reactive companion" section.

A failed solve restores the lowest-residual iterate, which is generally not where the models were
last evaluated, so `nonlinear_current_` has to be brought back into step with
`nonlinear_voltage_`. On a converged solve the solver's own contract says the two already agree.
The convergence gate keeps failures under 0.01 % of samples, so the extra evaluation is off the
critical path.

`derive()` is cheap enough to run whenever a knob moved. At the worst D6d shape
(`node_count_ + I = 13`, S = 4, N = 10, O = 1) the LU factorisation is about 13^3/3 = 730
multiply-adds and the thirteen unit-vector solves about 13 * 13^2 = 2200. The nine matrix products
are row and column differences and not dense multiplies, so together they are under a thousand
more. That is roughly 4 kflop, on the order of three microseconds, once per block and only when a
knob moved. Ruling 3 re-derives for that reason: against a 1.5 ms block budget a Woodbury update
would save noise, and the rank-two update is far harder to keep correct.

A nullor's row in the augmented system is its nullator (V(p) - V(q) = 0, right-hand side zero) and
its column is its norator (an unknown current into the driven pair). Unlike a source, the two are
different node pairs, which is exactly why an op-amp cannot be spelled with `voltage_source()`
([D6c plan](../../docs/archive/superpowers/plans/2026-09-05-d6c-tubescreamer-on-dk.md), ruling 1).

Every nullor puts a zero on the diagonal: its row touches only the sensed nodes and its column
only the driven ones. `DenseLu`'s partial pivoting takes the largest magnitude in each column and
not the diagonal entry, so those zeros cost it nothing. It fails only when a whole column below
the diagonal is zero, which is the rank deficiency an op-amp outside a feedback loop leaves
behind.

After the inversion, Pleft is still `inverse_[0..nodes)[0..nodes)` and Pright still
`inverse_[0..nodes)[nodes..nodes+I)`. The nullor rows and columns sit after the sources and their
right-hand side is zero, so they shape the inverse without ever being read out of it.

### `math/dk/DkNetlist.hpp`

`DkNetlist` is a plain aggregate of fixed-size arrays, so `DkCircuit` can own a copy and re-derive
its matrices on the audio thread without touching the builder. Elements are stored as node pairs
and not as dense incidence matrices: every product in the DK derivation is a row or column
difference, which a stamp loop over these pairs computes directly.

The add methods assert on overflow or an out-of-range node, because they run on the control thread
while a circuit is written by hand. `DkCircuit::prepare` re-validates and returns false, because
it is also reachable from a preset load.

In `nullor()` the sensed pair is a nullator and the driven pair a norator. Together they are the
infinite-gain limit of a voltage-controlled voltage source, which is what the LTspice reference
blocks model with an E source of gain 1e6. A nullor is only well posed inside a negative-feedback
loop: if the driven pair cannot reach the sensed pair through the rest of the circuit, the
augmented nodal system is singular and `DkCircuit::prepare` returns false.
