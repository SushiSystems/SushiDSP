# `modules/foundation/circuits`

Circuit blocks: tone stacks, the equaliser, feedback, the clamp, the tube stages and the device models.

## Owns

- `modules/foundation/circuits/include/SushiDSP/circuits/audio_taper_pot.hpp`
- `modules/foundation/circuits/include/SushiDSP/circuits/fmv_tone_stack.hpp`
- `modules/foundation/circuits/include/SushiDSP/circuits/graphic_equalizer.hpp`
- `modules/foundation/circuits/include/SushiDSP/circuits/output_safety_clamp.hpp`
- `modules/foundation/circuits/include/SushiDSP/circuits/power_amp_feedback.hpp`
- `modules/foundation/circuits/include/SushiDSP/circuits/models/pentode_el34.hpp`
- `modules/foundation/circuits/include/SushiDSP/circuits/models/silicon_diode.hpp`
- `modules/foundation/circuits/include/SushiDSP/circuits/models/soft_transition.hpp`
- `modules/foundation/circuits/include/SushiDSP/circuits/models/triode12_ax7.hpp`
- `modules/foundation/circuits/include/SushiDSP/circuits/stages/antiderivative_antialiaser.hpp`
- `modules/foundation/circuits/include/SushiDSP/circuits/stages/grid_conduction_law.hpp`
- `modules/foundation/circuits/include/SushiDSP/circuits/stages/long_tailed_pair.hpp`
- `modules/foundation/circuits/include/SushiDSP/circuits/stages/miller_input_capacitance.hpp`
- `modules/foundation/circuits/include/SushiDSP/circuits/stages/output_transformer_stage.hpp`
- `modules/foundation/circuits/include/SushiDSP/circuits/stages/pentode_stage.hpp`
- `modules/foundation/circuits/include/SushiDSP/circuits/stages/phase_inverter_presets.hpp`
- `modules/foundation/circuits/include/SushiDSP/circuits/stages/plate_swing.hpp`
- `modules/foundation/circuits/include/SushiDSP/circuits/stages/power_supply_sag_stage.hpp`
- `modules/foundation/circuits/include/SushiDSP/circuits/stages/power_tube_presets.hpp`
- `modules/foundation/circuits/include/SushiDSP/circuits/stages/resonance_control.hpp`
- `modules/foundation/circuits/include/SushiDSP/circuits/stages/triode_stage.hpp`

## Depends on

`math` (same tier).

## Notes cited from the source

A comment in this module's files states one invariant and names this file. The reasoning
behind each invariant follows, under the name of the file that cites it.

### `modules/foundation/circuits/include/SushiDSP/circuits/models/pentode_el34.hpp`

`current_and_jacobian` clamps the screen voltage at `K_SCREEN_FLOOR_VOLTS` before dividing by it.
Below the floor the voltage the model sees no longer varies with `v[1]`, so the whole Vg2k column
of the Jacobian is zero there. The clamp makes that column one-sided at exactly
`v[1] == K_SCREEN_FLOOR_VOLTS`: the derivative reported at that point is the right-hand one, the
unclamped column, while the left-hand one is zero. A 1 mV screen supply is nonphysical, so no
operating point sits on that seam. The clamp exists only to keep `vg1k / vg2k` finite for a solver
that wanders there.

The plate and screen laws raise E1 and z to `kx - 1`. Since `kx > 1`, both powers vanish
continuously as their arguments reach zero. The two `max(0, .)` clamps are therefore C1 and not
slope discontinuities, and the single guard on each power carries its whole branch, current and
derivative alike.

### `modules/foundation/circuits/include/SushiDSP/circuits/models/silicon_diode.hpp`

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

### `modules/foundation/circuits/include/SushiDSP/circuits/models/soft_transition.hpp`

`softplus_and_logistic` returns `t` itself for `t >= 30`. `ln(1 + exp(t)) - t` is 9.24e-14 at
`t = 30` and falls from there, so the linear branch is exact to double precision and keeps
`exp(t)` away from its overflow near 709.

### `modules/foundation/circuits/source/fmv_tone_stack.cpp`

The treble pot's a-to-wiper section grows as the knob closes, so its DK position is one minus the
panel position. The bass and middle controls are rheostats: the wiper carries the section's
current, and the dead wiper-to-b section stamps nothing because its two terminals are one node.

### `modules/foundation/circuits/source/stages/pentode_stage.cpp` and `modules/foundation/circuits/source/stages/triode_stage.cpp`

Both stages solve the same grid node. With every branch current leaving the node and Vg referenced
to the cathode, Kirchhoff's current law reads

```
(Vg - Vk - Vin) / Rs + (Vg - Vk) / Rg + IG0 * softplus((Vg - VGAMMA) / VTG) = 0
```

The residual is convex and increasing in Vg, so Newton converges from either side. The two grid
bounds handed to the solver mark where the plate ceiling has already saturated.

`root_stays_on_bound` skips the solve when the warm start sits on a bound and would stay there.
Newton's first step from a bound leaves the iterate on it whenever the residual there has the sign
that points past the bound. The conduction current is never negative, which settles the cutoff
bound from the linear part alone, and at the conduction bound softplus equals its argument.
