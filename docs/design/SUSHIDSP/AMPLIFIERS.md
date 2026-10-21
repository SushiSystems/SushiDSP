# The amplifiers: JCM800 and its family

One composite `AudioNode` (`Jcm800`) modeling the Marshall JCM800 2203 signal path with **Wave Digital Filters** for modular reactive sub-circuits and the **DK-method** for tightly-coupled blocks (the tone stack), at **8× oversampling**. Exposes `jcm800_descriptor()`.

```
DI in ─▶ input/bright cap ─▶ [V1a 12AX7] ─▶ coupling cap (tightness HPF) ─▶ [V1b 12AX7]
      ─▶ Marshall tone stack (Bass/Mid/Treble) ─▶ [V2a] ─▶ [phase inverter LTP]
      ─▶ [EL34 push-pull class-AB: grid conduction + crossover] ─▶ [output transformer:
         primary inductance + leakage + core saturation] ─▶ [power-supply sag: rectifier+RC → dynamic B+]
      (global NFB: Presence/Resonance) ─▶ downsample ─▶ [cabinet IR: partitioned convolution] ─▶ out
```

- **Triode stages (12AX7):** WDF sub-tree terminated by a nonlinear triode root; plate/grid currents from **Koren/Norman** equations, solved per oversampled sample by the shared Newton–Raphson solver (the circuit-solving core). Cathode bypass + grid-leak set level/frequency dependence; bright cap adds low-gain sparkle.
- **Tone stack:** the passive Marshall (FMV) network via DK-method (or 3-port WDF) so Bass/Mid/Treble interact exactly as the real passive network.
- **Phase inverter:** 12AX7 long-tailed pair.
- **Power section (EL34 push-pull, class AB):** pentode model with screen dependence; grid conduction clamps positive grid swing; crossover emerges from the class-AB bias point.
- **Output transformer:** primary inductance (low-end rolloff), leakage (top shaping), Jiles-Atherton-lite core saturation.
- **Power-supply sag:** rectifier + reservoir RC → dynamic B+.
- **Cabinet:** measured 4×12 IR via partitioned convolution partitioned convolution, mic-model pre-filter, swappable IR.

## As built in D4

The section above records the intended design. What D4 actually shipped is simpler in five
places, each a deliberate scope decision rather than an omission:

- **Triode stages** are not a WDF sub-tree with Koren/Norman plate and grid equations. `TriodeStage`
  solves one grid-node equation, source resistance, grid leak, and a Shockley grid-conduction
  diode, with the shared Newton–Raphson solver, and takes the plate output as an inverting linear
  gain on the grid's excursion from its bias point. The distortion character comes from grid
  conduction, not from a plate-current law.
- **Tone stack** is three cascaded RBJ biquads (low shelf, peaking, high shelf), not the passive
  FMV network via DK-method or a 3-port WDF, so the three controls do not interact the way the real
  passive network's do.
- **Phase inverter** is a single `TriodeStage` plus a sign flip, not a long-tailed pair.
- **Power section** is `PentodeStage`: the same grid-conduction model as the triode, followed by an
  explicit `tanh` plate saturation. There is no screen-grid dependence. Its node equation is
  bias-invariant by construction, so the class-AB asymmetry between the two tubes is applied as an
  input-referred grid offset on one half rather than as a difference in their bias points, a
  cathode-bias difference cancels out of the equation entirely and produces no asymmetry at all.
- **Output transformer** core saturation is a plain `tanh`, not Jiles-Atherton-lite.

The signal chain otherwise follows the diagram, including where the decimator sits: the output
transformer and the sag stage run inside the oversampled region, and only the presence shelf and
the master volume, both linear, so neither generates anything to alias, run after it. Resonance
and a true global NFB loop are not implemented; presence is a fixed high shelf. Of the advanced
parameters listed below, only the cabinet IR is settable (a constructor argument); bias, sag
amount and oversample factor are compile-time constants.

**Parameters (`NodeDescriptor`):** Preamp, Master, Bass, Middle, Treble, Presence; advanced: bias, sag_amount, cabinet_ir, oversample_factor. All smoothed, click-free. **Cost target:** Newton–Raphson × 8× × ~6 nonlinear stages, **< 1.0 ms per 256-block on one core**.

## As built in D5b

- **Parameters** are eight, not six: Preamp, Master (dB), Bass, Middle, Treble, Presence,
  Input trim (dB), Output (dB). Master, Input trim and Output are calibrated in decibels via
  `Calibration::decibels_to_gain`; the rest stay normalized `[0, 1]`. Every parameter is a
  `SmoothedParameter` target set by `Jcm800::set_parameter(std::uint16_t, float)`, index-based,
  RT-safe, and applied with a per-block `skip(frames)` step rather than per-sample ramping.
  Defaults are calibrated so a −12 dBFS input lands the amp's output at roughly −6 dBFS.
- **Cabinet** runs on `StreamingConvolution` (partitioned convolution/the resamplers's partition, chosen by
  `StreamingConvolution::partition_size_for`, 128 frames at or below 50 kHz, 256 above) behind
  an `RtSwapSlot<StreamingConvolution>`, so `set_cabinet(Ir)` can install a new impulse response
  from the control thread at any time, at any host block size, with `reclaim_cabinets()` freeing
  the previous engine once the audio thread has moved past it. `latency_samples()` reports the
  oversampler's latency plus the active cabinet's partition size, and moves after `set_cabinet()`
 , a host should re-read it whenever the cabinet changes.
- **Output limiter removed.** The soft-knee limiter added after D4 is gone; the output stage is
  a plain clamp to full scale. Calibration (input trim + the −6 dBFS default operating point)
  is what keeps normal playing levels off the clamp, not a limiter.

## As built in D6a

- **One 8× region, not two.** The sag stage and `JcmToneStack` now run inside the same
  oversampled region as V1a/V1b/V2a/PI, instead of the chain being split into two oversampled
  regions with a base-rate gap between them. **This raises the reported latency, it does not
  halve it:** at D5b each `Oversampler8x` round trip cost 12 base-rate samples and the amp ran
  two regions, for 24 in total; one region built from the new 115/27/15-tap halfbands costs 65,
  so the amp goes 24 → 65 base-rate samples, about 0.85 ms more latency at 48 kHz, and a
  host-visible behaviour change.
- **Where the alias floor actually came from.** The D5b baseline is −3.8 dB at `preamp = 10`
  (two regions, 15-tap stages). The new halfbands alone, still with two regions, take it to
  −32.0 dB. Collapsing to a single region then *regresses* it to −20.4 dB: the intermediate
  decimate/interpolate pair that the collapse removes was itself filtering some of what the
  measurement sees, so the single-region route on its own is worse than the two-region one.
  The Miller poles below are what recover it and go past the target, to −43.4 dB.
- **Three Miller-effect grid poles.** V1b, V2a and the phase inverter each gain a physically
  derived one-pole lowpass (`Biquad::set_one_pole_low_pass`, prewarped bilinear) on their grid
  node, modeling the 12AX7's Miller capacitance seen through a 100 kΩ source: `C_in = C_gk`
  (1.6 pF) `+ C_gp` (1.7 pF) `· (1 + |A|) + 10 pF` stray (a stated assumption, the stray term is
  not derived from a real JCM800 layout, and is the knob to turn if the amp should sound
  brighter). At the stage gains involved this puts the corner at 13 804 Hz (V1b, |A| = 60),
  13 804 Hz (V2a, |A| = 60) and 19 576 Hz (PI, |A| = 40). Adding these three poles is what takes
  the measured alias floor the rest of the way, from −20.4 dB to −43.4 dB at `preamp = 10`, past
  the D6a acceptance target of ≥ 30 dB improvement over the D5b baseline of −3.8 dB. Their audible
  cost is a whole-amp treble rolloff (preamp 0.6, knobs centred): −0.06 dB at 1 kHz, −1.33 dB at
  5 kHz, −4.64 dB at 10 kHz, −8.76 dB at 15 kHz. The bias-mismatch asymmetry re-measures at
  2.17 % second harmonic active vs. 2.93 % zeroed, up from 1.77 % / 2.45 % before D6a: the
  absolute figures moved because the drive reaching the power stage changed, while the
  mismatch's own effect, it removes about 0.76 pp of second harmonic, is unchanged.
- Since DA0 the four stage classes live in `modules/foundation/circuits/include/SushiDSP/foundation/circuits/stages/`, owned by the
  core, so any amplifier module can use them; `modules/nodes/jcm800` keeps only `JcmToneStack` and
  `Jcm800`.

## As built in DA4

`TriodeStage` gained a seven-argument constructor that gives its plate a two-sided `tanh` ceiling
set by how far the real plate can swing toward the supply and toward the bottom of its load line
(a `PlateSwing`), and on that path the grid-conduction diode references the cathode rather than
the bias point, so a stage's clean swing matches its bias voltage the way the real grid does. The
six-argument constructor is unchanged and stays bit-identical. With a physical ceiling in place,
all four amplifier modules dropped the level-staging devices (JCM800's five scalars, the 5150's
seven `k*LevelStaging` constants and its derived power-grid lift, the VH4's four staging
constants, the IIC+'s per-stage normalisation) that kept every following grid off its
solver bound; each dossier's own dividers now run at face value, in volts, from the first grid to
the power tubes. Two circuit facts came with that change, applied by every module: the phase
inverter is a long-tailed pair, so its grid-cathode window is the input divided by the tail
degeneration `1 + (R_tail ∥ Z_shunt) / Z_k` of the spec's section 4
(the triode plate swing design spec in the version history), not the full swing a
common-cathode stage would see; and `PentodeStage`'s grid is not volt-true, so the link from a preamp stage's volts into its
grid carries a derived `kPowerTubeGridScale` bridge until `PentodeStage` is rebuilt in DA5. Every
coupling capacitor moved to `Biquad::set_first_order_high_pass`, the numerically stable one-pole
shape for a physical RC link, in place of the second-order `set_high_pass`, which is unsafe below
a `cutoff_hz / sample_rate` of `1e-4`. Because the dossiers' own dividers run this hot, "no grid on
its bound" is not achievable at these gain structures; the acceptance criterion each module's
README documents instead is a clean end at minimum gain, non-decreasing THD across the knob, and
a −6 dBFS ± 1 dB default peak. The JCM800's alias floor regressed to −6.7 dB against the −33.8 dB
bound as a result and is carried as a known issue (`docs/reference/KNOWN_ISSUES.md`). The
2026-09-07 diagnosis measured the source as the grid-conduction knee rather than the plate
ceiling or the level; the DA5 spec below carries the repair.

## As built in DA5, part 1, the grid law

`TriodeStage`'s grid-conduction diode is the Norman law `Ig = IG0 · softplus((Vgk − VGAMMA) / VTG)`
with IG0 1 µA, VGAMMA 0.5 V and VTG 0.15 V, read from `Triode12AX7` through a `GridConductionLaw`
struct (`K_TRIODE12_AX7_GRID_CONDUCTION`) so the stage and the DK model cannot drift apart; the
room-temperature Shockley knee (Is 1 nA, Vt 25.85 mV) and the six-argument bias-referenced
constructor are gone. The solver's grid bounds sit where the plate `tanh` has saturated in float,
so no fence holds a grid short of its ceiling: the V5A stage's DC transfer reaches its full
90.74 V, where the 160·Vt fence held it at 74.3 V. Alone in an 8× region the same stage folds at
−81.2 dB at 3× its window and −53.7 dB at 12× (−37.3 and −13.7 dB before), which
`TriodeStage.TriodeStageAliasFloorAtThreeAndTwelveTimesItsWindow` pins under −60 and −40 dB.
All four amplifiers pin their unity constant at 82 Hz, the low E (JCM800 0.3105599, 5150
0.2400876, IIC+ 0.3343019, VH4 0.3543949); the 5150's inverter runs at its solved point
(Vgk −1.174528 V, gain 63.51, degeneration 1.9145, swing 113.7 / 79.6 V) and its PRE GAIN is an
exponential audio taper, 0.10 at noon. Whole-amplifier alias floors at −12 dBFS moved from +26.0
to −17.2 dB (5150 default), +9.1 to −6.9 dB (IIC+), −6.7 to −15.1 dB (JCM800 at preamp 1.0) and
−10.6 to −11.6 dB (VH4); the JCM800 at preamp 0.5 moved from −18.4 to −11.2 dB. None is under
the −33.8 dB bound, which the spec's antialiasing part (decision 2, held for a 16× measurement)
and the volt-true `PentodeStage` (decision 3) still own, with the JCM800's V1b gain and inverter
bias (decision 4).

## As built in DA5, part 2, the volt-true power stage and the 2203's corrections

`PentodeStage` takes the six arguments `TriodeStage` takes, in real volts: its grid conducts on
the Norman law of `PentodeEL34` (`K_PENTODE_EL34_GRID_CONDUCTION`), referenced to the cathode, so an
EL34 biased at −38 V has 38 V of clean grid swing where the bias-referenced stage clamped at
+0.2 model volts; its plate is bounded by a `PlateSwing` of its own through `PlateSwing::bound`,
the two-sided tanh both stages share. `PowerTubePreset` carries a law and a swing in place of the
Shockley pair and the saturation drive, and every preset stands on a datasheet row: `K_EL34` is the
Philips EL34 sheet's class B push-pull point at −38 V, 3.4 kΩ and 2 × 30 mA (swing 25.5 / 280.3 V,
gain 6.88), the VH4's is the same sheet's 500 V row at Diezel's 35 mA, the 5150's and the IIC+'s
6L6GCs are the RCA sheet's class AB₁ row, and the IIC+'s outer pair is Mesa's 25 W split as a
class A stage. The bias-invariance the D4 section above records does not hold here: a cathode-bias
difference between two halves moves their handover, and the modules keep the input-referred offset
they had, converted through the deleted grid scales (JCM800 0.24 V, 5150 0.4752 V). The JCM800's
gains are the Koren Jacobian at each idle point (V1a 55.43, V1b 7.774 unbypassed, V2a 67.00), its
inverter runs with the grid leaks on the 470/10k junction as the 2203 wires them (Vgk −1.346 V,
gain 61.93, degeneration 1.889, swing 117.4 / 89.5 V; the VH4's stand-in copies the four), and the
`kPowerTubeGridScale` bridges are gone from all four modules. Alone in an 8× region the EL34 stage
folds at −88.1 dB at 8 V and −55.1 dB at its window (−49.7 dB at one model volt before). Whole
amplifiers at −12 dBFS: JCM800 −11.2 to −16.3 dB at preamp 0.5 (−18.4 before part 1), −15.1 to
−13.4 dB at preamp 1.0, −24.6 to −48.0 dB at preamp 0.02; 5150 −17.2 to −11.4 dB at its default
and −6.0 to −35.4 dB at pre gain 0.02; IIC+ −6.9 to −0.1 dB; VH4 −11.6 to −13.3 dB. The 5150 and
IIC+ defaults read the same with the power stage made linear, so what shows there is the preamp's
fold, which the old clamp had masked by re-squaring the output; the antialiasing part (decision 2)
owns it. Unity constants at 82 Hz: JCM800 0.2534701, 5150 0.1730211, IIC+ 0.2402319, VH4 0.2365060.

## As built in DA5, part 3, the transformer's secondary and the 16× measurement

`OutputTransformerStage`'s output carries no mean: a one-pole running mean of the saturated signal,
tracked in double a decade under the rolloff's corner, is subtracted after the tanh, because a
secondary sees d(flux)/dt and the tanh had been putting back 0.021 of full scale of the DC its own
rolloff removed whenever the pair delivered a square with unequal halves (the VH4 at bass 0 read
0.0152 of output DC; it reads 0.0010, the ±1 clamp's own). The four unity constants at 82 Hz are
re-pinned on it: JCM800 0.2500494, 5150 0.1710120, IIC+ 0.2349335, VH4 0.2319680. 16× was measured
through probes and not shipped (spec §14): alias floors at −12 dBFS move from −16.3 to −35.0 dB
(JCM800 default), −13.4 to −25.1 (JCM800 preamp 1.0), −11.4 to −19.9 (5150), −0.1 to −14.3 (IIC+)
and −13.3 to −27.3 (VH4) at 1.9 to 2.0× the CPU, the 5150 alone taking 5.19 of the 5.33 ms block;
the clean settings' floor at either rate is `Biquad`'s float rounding (−48.0 dB at 8×, −78.0 with
double state), not a fold.

## As built in DA5, part 4, the antiderivative of the composite stage, and the double biquad

`TriodeStage` and `PentodeStage` return the output of an `AntiderivativeAntialiaser`: first-order
ADAA of the composite grid-and-plate transfer, evaluated in the grid's coordinate as
(G(u1) − G(u0)) / (g(u1) − g(u0)), with g(u) = A u + K softplus((u − c) / w) the input that puts the
grid at u and G = A · P + (K / w) · R, P the plate tanh's closed-form ln cosh and R the product of
tanh and logistic, which has no elementary antiderivative and is tabulated over the knee's ±24
transition widths (spec §15). `Biquad` carries coefficients and state in double behind its float
interface. Measured apart and together at −12 dBFS: ADAA alone moves the hot defaults 8.7 to
15.4 dB (JCM800 −16.3 to −25.0, preamp 1.0 −13.4 to −23.3, 5150 −11.4 to −26.8, IIC+ −0.1 to
−14.8, VH4 −13.3 to −23.0 dB), the double biquad alone moves the JCM800's preamp 0.02 from
−48.0 to −78.0 dB and nothing else; one stage alone reaches −102.4 dB at 3x its window. What
remains is the later stages' fold of an already-clipped input, which a first-order antiderivative
cannot see, so dependencies's −33.8 dB stays a known issue. The cost at the build's `-O0` is 1.7 to 2.1x
(5150 4.23 and IIC+ 4.29 of the 5.33 ms block), at `-O2` 1.4 to 2.0x. All eight goldens move,
`tube_screamer` and `dd3` for the first time in this phase.

## Amplifier family

The DA phase (plans the high gain amps part1 plan in the version history and
the high gain amps part2 plan in the version history) grew the
single JCM800 into four sibling amplifier modules sharing the stage classes above.

| Module | Channel | Dossier | Module path | Status |
| --- | --- | --- | --- | --- |
| JCM800 | Marshall JCM800 2203, one channel |, | `modules/nodes/jcm800/` | White-box, see "As built" above. DA4: plate-swing chain. |
| Peavey 5150 | Block-letter 1992 5150, lead channel | the peavey 5150 design dossier research note in the version history | `modules/nodes/peavey5150/` | White-box against the dossier's schematic set. DA4: plate-swing chain. |
| Mesa/Boogie Mark IIC+ | Lead mode | the mesa mark iic plus design dossier research note in the version history | `modules/nodes/mesa_mark2c/` | White-box against the dossier's schematic set. DA4: plate-swing chain. |
| Diezel VH4 | Channel 3 ("Mega") | the diezel vh4 design dossier research note in the version history | `modules/nodes/diezel_vh4/` | Documented approximation: the gain chain is a reverse-engineering drawing, the tone stack's interconnection is a reconstruction, and the power section is built from prose alone. DA4: plate-swing chain. |

Each module's own README lists exactly where its model departs from its dossier.
