# The grid knee, antialiasing and the volt-true power stage

**Status:** Parts 1 to 4 built, 2026-09-08: all seven decisions taken and implemented (sections 12 to 15); decision 2 is ADAA at 8x on the composite stage function, with `Biquad` in double, section 15. The −33.8 dB acceptance of section 11 is not met and stays a known issue. Evidence in `.superpowers/sdd/amp-gain-staging/diagnosis.md`, `.superpowers/sdd/amp-gain-staging/da5-grid-law-report.md`, `.superpowers/sdd/amp-gain-staging/da5-part2-report.md`, `.superpowers/sdd/amp-gain-staging/da5-part3-report.md` and `.superpowers/sdd/amp-gain-staging/da5-part4-report.md`. Succeeds DA4 (`2026-09-07-triode-plate-swing-design.md`) as the DA5 spec.

## 1. Goal

Every amplifier's grid saturates the way the repository's own 12AX7 model says a grid saturates,
the stage that saturates does not fold its harmonics into the audio band, and the power tubes
clip where their grids conduct. With that in place the alias floor of the JCM800 at preamp 1.0
returns under the -33.8 dB bound, the 5150 stops putting its worst alias 26 dB above the note at
its default setting, and the four amplifiers' default settings are equally loud at the low E.

## 2. Why

The diagnosis measured one `TriodeStage` (5150 V5A constants) alone in an 8x region: -117.6 dB
with the grid inside its window, -37.3 dB at 3x past it, -13.7 dB at 12x. The same stage with its
plate ceiling fully engaged and its grid inside the window (the V2A cold clipper) measures
-109 dB, so the DA4 plate tanh is not the source. Replacing the Shockley knee (Is 1 nA, Vt
25.85 mV) with the Norman law of `Triode12AX7` (IG0 1 uA, VGAMMA 0.5 V, VTG 0.15 V) moves the 3x
figure to -81.2 dB and the 12x figure to -46.5 dB; a Shockley knee at the cathode's temperature
(Vt 0.09 V) moves them to -61.7 and -22.0 dB. Removing the solver's 160 Vt cutoff fence moves
neither. The knee is the corner.

The drive is the circuit's. The dossier's dividers put 5800 (75 dB) between V1B's grid and V5A's,
V1A and its peaker 42 more, and Peavey's published lead sensitivity (-80 dBV at pre gain 5)
falls out of that arithmetic to within 2 dB: V5A's 2.0 V window is crossed at 82 uV. A guitar is
60 dB above that. So a stage on this amplifier lives 10 to 3500x past its window, no knee alone
holds the fold under the bound there, and the antialiasing is load-bearing in its own right.

`PentodeStage` references its diode to the bias point and clamps the grid at +0.2 model volts,
+1.6 V real on the EL34 and +3.2 V on the 6L6GC, against windows of 20 V and 55 V. Alone in an
8x region it aliases at -49.7 dB with 1.0 model volt of drive. The 5150's pair puts 4.7 to 7.2
model volts on it above about 1 mV of input.

## 3. The change to `TriodeStage`

The seven-argument path takes its grid law from the model the dossiers and the SPICE corpus were
computed with, so the module and its references agree by construction. Two candidates, one to be
chosen (section 8, decision 1):

- **Norman, as in `12ax7_koren.sub`:** `Ig = IG0 * softplus((Vgk - VGAMMA) / VTG)` with IG0 1 uA,
  VGAMMA 0.5 V, VTG 0.15 V, evaluated through `models::softplus_and_logistic` as `Triode12AX7`
  already does. Above 0.5 V the grid current is a 150 kOhm slope, so the grid clamps softly and
  the plate tanh does the clipping. Measured: -81.2 dB at 3x, -46.5 dB at 12x.
- **Shockley at the cathode's temperature:** the existing law with Vt = kT/q at 1050 K, 0.090 V,
  and Is left at 1 nA. A firm clamp with a knee 3.5x wider than today's. Measured: -61.7 dB at
  3x, -22.0 dB at 12x. A real 12AX7 passes about 0.3 mA at Vgk = +1 V, so this clamp is nearer
  the tube than the Norman law's 1 uA scale; the Norman law is nearer the reference corpus.

Either way the constructor's `saturation_current_amps, thermal_voltage_volts` pair becomes the
chosen law's two constants, named for what they are, and the six-argument path is deleted (DA4
section 5 already scheduled that). `peak_grid_excursion_volts()` and the `PlateSwing` ceiling
stay as they are.

The cutoff fence (`kNegativeExponentLimit`) stays while the bias-referenced path exists: it costs
nothing in the alias metric and it is the guard that path needs. Once the six-argument path is
deleted, the fence widens to the plate swing's own reach, so the tanh ceiling is the cutoff; the
DC transfer then reaches its full headroom (90.7 V, where the fence holds it at 74.3 V today).

## 4. Antialiasing

The stage is memoryless per sample once the Newton solve converges: plate output is a function
`f(vin)` of the input alone. That admits first-order antiderivative antialiasing on the composite
function, `y[n] = (F(x[n]) - F(x[n-1])) / (x[n] - x[n-1])`, with the antiderivative `F` tabulated
at construction over the grid's reachable range (the input is bounded by the previous stage's
plate swing, so the table's span is known per stage) and the small-difference branch falling back
to `f` at the midpoint. `ts_clip` already carries the project's ADAA (measured -55.9 dB against
-32.4 dB plain on its 13th harmonic), so the shape exists.

The alternative is a 16x region: one more `HalfbandUpsampler2x` / `HalfbandDownsampler2x` pair
on `kHalfbandStage3`, twice the per-sample cost of every stage, and about 6 dB on a hard corner
whose harmonics fall as 1/k. The diagnosis did not measure a 16x chain; the estimate is the
fold's arithmetic, and decision 2 chooses between the two or asks for that measurement first.

## 5. The change to `PentodeStage`

`PentodeStage` becomes volt-true, as DA4 section 5 scheduled: its input is the grid voltage in
volts, its grid diode is referenced to the cathode (conduction at Vgk = 0, `|cathode_bias_volts|`
above rest, as the seven-argument `TriodeStage` does), its plate is bounded by a `PlateSwing` of
its own from the dossier's B+ and the load line, and `PlateSwing`'s tanh moves to a shared helper
both stages call. `kPowerTubeGridScale` is deleted from all four modules. The presets need a gain
and two headroom figures in real volts; the 5150, IIC+ and VH4 dossiers give B+, the bias and the
reflected load, and `kEl34` says it was fitted to goldens without an operating point, so the
2203's preset is the one number the maintainer supplies or approves (decision 3).

## 6. Module corrections the diagnosis found

Derived from the repository's Koren model; each changes what an amplifier sounds like and each
waits on section 8.

- **JCM800 V1b gain.** `kTriodeGain = 60` serves V1a, V1b and V2a. On the operating points their
  own bias constants were solved at, the model gives 55.4 (V1a), 7.77 (V1b, 10 kOhm unbypassed;
  the `jcm_v1b_cold` deck's AC sweep reads +16.89 dB, 7.0) and 67.0 (V2a). V1b is 17.8 dB high
  and V2a's 1.25 V window receives 7.7x the circuit's drive. Correction: one gain constant per
  stage, as the 5150 has. Decision 4.
- **JCM800 phase inverter bias.** The `jcm_ltp` deck returns both grid leaks to ground; the 2203
  returns them to the 470 / 10 k tail junction (the deck's own README names that node as the
  feedback injection point). Re-solved: Ia 1.432 mA, Vk 1.346 V, Va 212.6 V, headroom to supply
  147.4 V (the module: 15.38 V), full single-triode gain 61.9 (22.4), cathode impedance 1311 Ohm
  and degeneration 1.889 (1.744). With 15 V of headroom the inverter clips toward the supply at
  0.7 V of grid drive, which is where the JCM800's 15.9 % THD at preamp 0.02 comes from. The
  deck is corrected first and re-run, then the module follows it. Decision 4.
- **5150 phase inverter bias.** The chosen -2.0 V becomes the solved point with the grid leaks
  on node X: Ia 1.2495 mA, Vgk -1.175 V, Va 188.4 V, headroom 113.7 V, gain 63.5. Decision 5.
- **Unity-constant frequency.** The JCM800 pins its -6 dBFS default at 82 Hz, the 5150 at 1 kHz;
  the 5150's resonance shelf lifts everything below 330 Hz, so at 82 Hz its default is at the
  output clamp and 10 dB louder RMS than the JCM800. All four amplifiers pin at the same
  frequency; the JCM800's 82 Hz is the sibling rule's answer. Decision 6.
- **Pre-gain taper.** `n * sqrt(n)` is 8 to 11 dB hotter than an audio taper over the middle of
  the knob. A taper constant is a choice; recorded, not proposed. Decision 7.

## 7. What changes about the sound

With the knee at the physics and the fold removed, a stage past its window clips the way its
tube does and adds nothing below the fundamental that was not in the harmonic series. The 5150
and the IIC+ lose the fizz the maintainer hears; both stay amplifiers with no clean setting at a
guitar level, because that is the circuit. The pre-gain knob still saturates the THD metric; what
it changes is which stages clip and how the harmonic series is shaped, which the hard knee
erased and the physical one restores. The power tubes stop clipping their positive half 22 to
25 dB early, so the power section compresses instead of squaring and the output crest factor
rises from the 1.5 to 4.3 dB measured today. Pinning all four unity constants at 82 Hz brings
the 5150 down 6 dB at 1 kHz and 10 dB RMS at the low E, level with the JCM800. If decision 4 is
taken, the JCM800 becomes the cold-clipper amplifier the 2203 is: V1b asymmetric, V2a driven
7.7x less, the inverter with 147 V of headroom, and its THD at preamp 0.02 falls from 15.9 %
toward the 0.19 % its clean-end test already reads at -40 dBFS.

## 8. Decisions the maintainer owns

1. Grid law: Norman (the corpus) or Shockley at 1050 K (the tube). Section 3.
2. Antialiasing: ADAA on the composite stage function, or 16x, or measure 16x first. Section 4.
3. `PentodeStage` volt-true constants for the EL34 in the 2203. Section 5.
4. Whether the JCM800's V1b gain and inverter bias are corrected now or held. Section 6.
5. The 5150's inverter bias, -2.0 V chosen or -1.175 V solved. Section 6.
6. The unity-constant frequency, 82 Hz proposed. Section 6.
7. The pre-gain taper. Section 6.

## 9. Golden files this moves

Any change under sections 3, 4 or 5 moves `jcm800_48k/96k`, `peavey5150_48k/96k`,
`mesa_mark2c_48k/96k` and `diezel_vh4_48k/96k`, since all four run every triode through
`TriodeStage` and every power tube through `PentodeStage`. Section 6's items move the named
amplifier's pair. `tube_screamer_48k/96k` and `dd3_48k/96k` do not move; neither uses either
stage. Each re-pin gets a `tests/golden/README.md` row naming this spec. No golden is regenerated
by this design; the maintainer regenerates after each decision lands.

## 10. Out of scope

The IIC+'s missing V2A to V2B effects-loop network (dossier open questions 19 and 20) stays its
own known issue; this spec makes the IIC+ less fizzy and does not give it a clean setting. The
full DK load-line solve (D6d) remains the eventual replacement for the tanh ceiling and, with the
Koren grid law in place, would share its grid physics with this spec rather than change it.

## 11. Acceptance

- One `TriodeStage` with the V5A constants in an 8x region: alias floor under -60 dB at 3x its
  window and under -40 dB at 12x, measured as the diagnosis measures it.
- `Regression_Dsp.Jcm800AliasFloorImprovedAtLeast30dBOverD5b` passes at its -33.8 dB bound and
  is no longer skipped; the 5150 at pre gain 0.5 and -12 dBFS reads under -30 dB on the same
  metric, asserted by a new test shaped like the JCM800's.
- `PentodeStage` alone at 1.0 model volt of drive: no positive clamp (transfer symmetric to
  within the tanh), alias floor under -80 dB.
- All four amplifiers peak at -6 dBFS +/- 1 dB for a -12 dBFS input at the one agreed frequency.
- The four goldens re-pinned with README rows; `KNOWN_ISSUES.md`'s alias-floor entry closed.

## 12. Decisions taken, and what part 1 built

The maintainer ruled on 2026-09-07: decision 1 Norman; decision 5 the solved -1.175 V; decision 6
82 Hz; decision 7 a real audio-pot taper. Decision 2 waits on a 16x measurement, decisions 3 and
4 are held. Part 1 implements the four rulings and nothing under sections 4 or 5.

- `TriodeStage` takes a `GridConductionLaw` (scale, threshold, transition) in place of the
  Shockley pair; the constant `kTriode12AX7GridConduction` is read from `Triode12AX7`'s IG0,
  VGAMMA and VTG, which are `12ax7_koren.sub`'s `.param` line. The six-argument path is deleted.
  The solver's bounds sit where each side's tanh has saturated in float (an argument of 10), so
  the plate ceiling is the cutoff: the V5A transfer reaches 90.73 V at -30 V in.
- Measured, V5A alone in an 8x region: -81.2 dB at 6 V and -53.7 dB at 24 V of drive, against
  -37.3 and -13.7 dB; the diagnosis's -46.5 dB at 24 V was taken with the 1.55 V Shockley bound
  still clamping the grid. `Regression_Dsp.TriodeStageAliasFloorAtThreeAndTwelveTimesItsWindow`
  pins section 11's first line.
- Whole amplifiers at -12 dBFS, `measure::alias_floor()`: 5150 default +26.0 to -17.2 dB, IIC+
  +9.1 to -6.9 dB, JCM800 at preamp 1.0 -6.7 to -15.1 dB, at preamp 0.5 -18.4 to -11.2 dB, VH4
  -10.6 to -11.6 dB. Section 11's second line stays open for the antialiasing part.
- The 5150's pair: Vgk -1.174528 V, Ia 1.249497 mA, gain 63.512682, degeneration 1.914527,
  swing 113.704 / 79.597 V, solved with the grid leaks on node X.
- Unity constants at 82 Hz: JCM800 0.3105599, 5150 0.2400876, IIC+ 0.3343019, VH4 0.3543949;
  each reads -6.000 dBFS. At 82 Hz the old 5150 and IIC+ values sat on the output clamp.
- The 5150's PRE GAIN is (b^n - 1) / (b - 1) with b = ((1 - m) / m)^2 = 81 for a midpoint fraction
  m = 0.10; the JCM800 and the IIC+ keep n * sqrt(n) until the maintainer says otherwise.
- The eight goldens of section 9 move and are not regenerated here.

## 13. What part 2 built

The maintainer ruled on 2026-09-07: decision 3, the EL34's constants come from the tube's
datasheet operating point; decision 4, the JCM800's V1b gain and inverter bias are corrected now.
Part 2 implements both and section 5, and nothing under section 4.

- `PentodeStage` takes the same six arguments `TriodeStage` does: source, grid leak, cathode
  bias, gain, a `GridConductionLaw` and a `PlateSwing`, all in real volts. Its grid conducts on
  `el34_koren.sub`'s Norman law through `kPentodeEL34GridConduction`, referenced to the cathode,
  and its plate is bounded by `PlateSwing::bound`, the two-sided tanh both stages now call from
  `PlateSwing.hpp`; `GridConductionLaw` lives in its own header. `PowerTubePreset` carries the
  law and the swing in place of the Shockley pair and the saturation drive. `kPowerTubeGridScale`
  and the IIC+'s two pair scales are deleted from the four modules; each module's transformer
  trim maps the pair's full swing onto the level the transformer stage received at full clip
  before, so the transformer and the sag stage keep their range.
- The power-tube presets stand on datasheet rows. `kEl34` is the Philips EL34 sheet's class B
  push-pull point at Vg1 −38 V, Raa 3.4 kOhm, 2 x 30 mA, 55 W, with 1 kOhm screen resistors, the
  2203's own arrangement: window 38 V, swing 25.5 V toward cutoff and 280.28 V toward the bottom,
  gain 6.88 from the sheet's transconductance scaled to 30 mA. The VH4's EL34 is the same sheet's
  500 V row at Diezel's 35 mA; the 5150's side is two RCA 6L6-GC class AB1 tubes in parallel at
  the dossier's 2 x 30 mA and −55 V; the IIC+'s inner pair is the RCA row at its own 58 mA, its
  outer pair Mesa's 25 W split evenly as a class A stage. Each module's README lists which field
  is the amplifier's and which is the datasheet's. The repository's Koren EL34 fit reads 5.6 mA/V
  and 40.7 mA at the 2203's point against the sheet's 8.4 mA/V and 30 mA, so the presets do not
  lean on it.
- The JCM800's three common-cathode gains are the Koren Jacobian at each idle point: V1a 55.43,
  V1b 7.774 with its 10 kOhm cathode unbypassed, V2a 67.00. Its inverter is re-solved with both
  grid leaks on the 470/10k junction as the 2203 wires them: Ia 1.431927 mA per half,
  Vgk −1.346012 V, gain 61.927402, degeneration 1.888775, swing 117.418 / 89.548 V. The 147.4 V
  of section 6 was B+ − Vpk, which counts the 30 V cathode; the headroom toward the supply is
  Ia·Ra. The VH4's stand-in inverter copies the four constants. The EL34 grid's source is the
  corrected inverter's 82k || 43.056k plus the deck's 5.6 kOhm stopper, 33.83 kOhm.
- Measured, `PentodeStage` with `kEl34` alone in an 8x region: −88.1 dB at 8 V of drive (one
  model volt of the old stage) and −55.1 dB at its 38 V window, against −49.7 dB at one model
  volt before; `Regression_Dsp.PentodeStageAliasFloorInsideAndAtItsWindow` pins section 11's
  third line under −80 and −50 dB. The composite of a pair is odd in the drive to the float
  while both grids are inside their windows, and a cathode-bias difference now moves the handover.
- Whole amplifiers at −12 dBFS, `measure::alias_floor()`: JCM800 at preamp 0.5 −11.2 to −16.3 dB
  (−18.4 dB before part 1: part 2 recovers 5.1 of the 7.2 dB and does not close it; the inverter
  and V1b corrections alone, over the bias-referenced power stage, read −8.7 dB there and
  −18.1 dB at preamp 1.0), at preamp 1.0 −15.1 to −13.4 dB, at preamp 0.02 −24.6 to −48.0 dB; 5150 default −17.2 to −11.4 dB
  and at pre gain 0.02 −6.0 to −35.4 dB; IIC+ default −6.9 to −0.1 dB; VH4 default −11.6 to
  −13.3 dB. With the power stage made linear the 5150 reads −12.0 dB and the IIC+ −0.4 dB, and
  with only its cut-off ceiling removed −11.8 and −0.4 dB, so the fold that now shows at those
  two defaults is the preamp's: the bias-referenced stage's +0.2 V clamp had been re-squaring the
  output and masking it. Section 11's second line stays open for the antialiasing part.
- The JCM800's THD at a 0.2 input runs 6.1 % at preamp 0.02 to 42.9 % at 1.0, where it ran
  16.2 to 45.0 %; the cold clipper at 7.77 gives the knob a travel. Its clean end reads 0.174 %.
- Unity constants at 82 Hz: JCM800 0.2534701, 5150 0.1730211, IIC+ 0.2402319, VH4 0.2365060;
  each reads −6.000 dBFS.
- `Peavey5150EvenHarmonicsRiseFromLowToFullPreGain` compares the same two physical drive levels
  it always did, PRE GAIN 0.4776 on the audio taper standing for 0.2 on `n·√n`, and gates a
  factor of 2 on a measured 3.18.
- The eight goldens of section 9 move and are not regenerated here.

## 14. What part 3 built, and what 16x measures

The first build of parts 1 and 2 failed two VH4 tests, both real. Part 3 fixes them and takes
the 16x measurement decision 2 waits on; the oversampling factor stays 8x and there is no ADAA.
Every figure is from a probe compiled with the build's toolchain and flags
(`.superpowers/sdd/amp-gain-staging/da5-part3-report.md`).

- **The VH4's DC offset was the output transformer's.** Traced stage by stage at bass 0, a 0.2
  220 Hz input and the master at unity: the inverter's plate carries a mean of +5.5 V on a ±117 V
  swing (+9.9 V at volume 1, +13.0 V at gain 1), the pair's difference carries +0.1035 on a 2.2222
  full clip (4.66 %; 2.31 % at volume 1), `OutputTransformerStage`'s 60 Hz rolloff removes that
  mean to within the window's residual, and its tanh puts +0.0211 back, because the two halves of
  the square it compresses are not equally long; the sag stage scales that to +0.0096, Deep at its
  default lifts DC by its 5 dB to +0.0161, and the clamp reads +0.0167 (the test's 220 Hz window
  reads 0.0152). The pair's mean is the circuit's: every stage past V1a clips asymmetrically and
  each coupling corner removes the mean the stage before it made, so the square that reaches the
  power tubes has unequal halves. In the amplifier the coupling capacitors and the transformer
  pass none of it. The 2203's 0.022 µF into 230 kΩ (31.45 Hz) added between the inverter and the
  EL34 grids moves the pair's mean from 0.045 to 0.064 at volume 1 and does not cure it, so no
  capacitor is added; the transformer's float rolloff leaks 0.0006 of a 0.1 DC at 384 kHz, which
  is negligible. The fix is in the brick: the stage's secondary carries no mean, a one-pole
  running mean in double subtracted after the tanh, its corner a decade under the rolloff's (6 Hz,
  `kSecondaryMeanCornerFraction`). The transformer's mean reads +0.00003 at bass 0, the VH4's
  output 0.0010 on the test's window (0.0023 on a 117-cycle window), the remainder made where the
  ±1 clamp cuts the unequal halves of a 1.06 peak. The IIC+'s every-knob-at-1 setting reads
  −0.00009 (−80.7 dBFS; −0.00054 before, −0.0506 when its README was written), and its test's
  bound returns from 0.25 to the 0.01 an ideal transformer gives.
  `OutputTransformerStageSecondaryCarriesNoMean` pins the brick: a 220 Hz square of 2.2 at 40 %
  duty read −0.063500 through the old stage and reads +0.000005.
- **The reorder was measured and rejected.** Leakage, tanh, then rolloff also takes the mean out
  of the transformer (−0.0007 at bass 0) but the rolloff's edge overshoot on a clipped square is
  then uncompressed: the VH4's pre-clamp peak rises from 1.036 to 1.249, the clamp makes 0.0093 of
  DC on its own, the four default peaks at 82 Hz move to −3.94, −4.85, −3.67 and −3.77 dBFS, and
  the stage's ceiling test reads 5.18 against its 4.33 bound.
- **The Deep shelf is intact; the measurement was on the rail.** `frequency_response()` sweeps at
  −40 dBFS, and since part 2 the VH4 turns that into a full-scale output: at unity the flat
  response at 80 Hz reads +39.8 dB over the sweep, the output sits on the ±1 clamp, and a shelf
  lifting a clamped signal reads 2.07 dB at 80 Hz. With the master 10, 20 and 40 dB under unity
  the lift reads 4.813 dB alike, against the RBJ shelf's exact 5.000 at its corner (the estimator's
  own error; it read 4.622 before part 2). The test measures 20 dB under unity. 20 Hz reads
  6.840 dB, where C13 has thrown the fundamental away before the first distorting stage.
- **Unity constants at 82 Hz**, re-pinned because the DC the transformer no longer carries was
  part of each peak (readings before the re-pin −5.88, −5.90, −5.81, −5.83 dBFS): JCM800
  0.2500494, 5150 0.1710120, IIC+ 0.2349335, VH4 0.2319680; each reads −6.000 dBFS. THD sweeps
  move by under 0.25 percentage points; the alias floors at 8x do not move.
- **16x, measured.** One more `HalfbandUpsampler2x` / `HalfbandDownsampler2x` pair on
  `kHalfbandStage3` (at 16x its passband reaches 1.1 f_base and its stopband starts at 6.9, so the
  8x image at 7.45 f_base is rejected), the region's corners re-derived at 768 kHz.
  `measure::alias_floor()` at −12 dBFS, 8x as shipped against 16x:

  | Setting | 8x | 16x | Gain | Short of −33.8 dB |
  | --- | --- | --- | --- | --- |
  | JCM800 default (preamp 0.5) | −16.3 dB | −35.0 dB | 18.7 dB | inside by 1.2 dB |
  | JCM800 preamp 1.0, master 0 dB | −13.4 dB | −25.1 dB | 11.7 dB | 8.7 dB |
  | 5150 default (pre gain 0.5) | −11.4 dB | −19.9 dB | 8.5 dB | 13.9 dB |
  | IIC+ default | −0.1 dB | −14.3 dB | 14.2 dB | 19.5 dB |
  | VH4 default | −13.3 dB | −27.3 dB | 14.0 dB | 6.5 dB |

  At every default the worst bin is a fold of a 17 to 20 kHz tone landing between 1 and 8 kHz at
  both rates, so the figures are aliases. The gains exceed the 6 dB section 4 estimated because the
  worst products at 8x are the fold's highest-order terms, which 16x moves out of the band rather
  than attenuating.
- **The clean settings' floor is float rounding, not aliasing.** The JCM800 at preamp 0.02 reads
  −48.0 dB at 8x and −35.9 dB at 16x, and at both rates its worst bins sit between 20 and 100 Hz.
  With `Biquad`'s coefficients and state in double (a probe-only header, interface unchanged) the
  same setting reads −78.0 dB at 8x and −81.2 dB at 16x with its worst bins at kilohertz, and the
  hot defaults do not move (JCM800 −16.3 and −34.9 dB). The transformer's rolloff alone in double
  changes nothing, and the sag envelope in double changes nothing, so the source is the other
  low-corner second-order biquads at the oversampled rate: the JCM800's 120 Hz bass shelf sits at
  1.56e-4 of 768 kHz, the transformer's 60 Hz rolloff at 7.8e-5, under `set_high_pass`'s own 1e-4
  precondition. The shipped part-2 figure of −48.0 dB at preamp 0.02 is this floor, not a fold.
- **The cost of 16x**, `tests/benchmark/Benchmark.hpp`'s harness at the build's `-O0 -g`, 48 kHz,
  256 frames, best of 200, against the 5.33 ms block: JCM800 at the bench's knobs 1.310 → 2.519 ms
  (the shipped bench reads 1.298), JCM800 default 1.337 → 2.583, 5150 default 2.633 → 5.187 (97 %
  of the block), IIC+ default 2.022 → 3.931, VH4 default 1.687 → 3.274; the oversampler round trip
  0.207 → 0.335 ms. At `-O2`: 1.220 → 2.349, 1.256 → 2.411, 2.534 → 5.052, 1.948 → 3.826,
  1.616 → 3.123 ms.
- **Recommendation.** 16x alone does not bring the amplifiers under −33.8 dB: one of five settings
  crosses it, the 5150 stays 13.9 dB short and the IIC+ 19.5 dB, and the 5150 alone would take the
  whole block. What closes the rest is the fold's own remedy, ADAA on the composite stage function
  (section 4; `ts_clip`'s ADAA measured 23.5 dB on its 13th harmonic), at 8x, with 16x held as the
  6 to 19 dB it buys if ADAA leaves a gap. Whichever is chosen, `Biquad` wants double state before
  the region's rate rises: the clean settings' floor is already the float biquads' at 8x, and 16x
  raises it 12 dB.
- The eight goldens of section 9 move and are not regenerated here.

## 15. What part 4 built: the antiderivative of the composite stage, and the double biquad

The maintainer ruled on 2026-09-07: decision 2 is ADAA at 8x on the composite stage function, and
`Biquad` carries its state in double, both landed together and each measured on its own. Every
figure is from a probe compiled with the build's toolchain and flags
(`.superpowers/sdd/amp-gain-staging/da5-part4-report.md`).

### 15.1 The derivation

One stage, in the grid's own coordinate u = Vg − Vk. The KCL the solver already states is

    (u − x) / Rs + u / Rg + IG0 · softplus((u − c) / w) = 0,    c = VGAMMA − Vk,

so the input that puts the grid at u is explicit:

    x = g(u) = A u + K softplus((u − c) / w),    A = 1 + Rs / Rg,    K = Rs · IG0,
    g'(u) = A + (K / w) σ((u − c) / w),          σ the logistic.

g is strictly increasing, so u(x) exists and is unique; it has no closed form (the equation is
linear plus softplus in u, a Lambert-W kind of inverse), which is why the stage solves it by
Newton. The plate output is f(x) = bound(−gain · u(x)) with bound(L) = H tanh(L / H), H the
side's headroom.

First-order ADAA wants F with F' = f and reads y[n] = (F(x[n]) − F(x[n−1])) / (x[n] − x[n−1]).
Change variable to u: F(g(u)) = G(u) with

    G(u) = ∫ f(g(u)) g'(u) du = ∫ bound(−gain u) [A + (K / w) σ((u − c) / w)] du
         = A · P(u) + (K / w) · R(u),

    P(u) = ∫₀ᵘ bound(−gain t) dt = −(H² / gain) ln cosh(gain u / H),
    R(u) = ∫ bound(−gain t) σ((t − c) / w) dt.

P is closed-form on each side and C¹ at rest (`PlateSwing::bound_antiderivative`). R is the
product of a tanh on the plate's scale H / gain and a logistic on the knee's scale w, and **has no
elementary antiderivative**: with t = e^u the integrand is a rational function of two
incommensurate powers of t, and the integral is not elementary nor a polylogarithm for a generic
ratio 2 · gain · w / H (0.057 for the V5A). This is the part section 4 said would be tabulated,
and it is the only part that is: σ is within 4e-11 of 0 or 1 outside ±24 w, so R is confined to a
7.2 V window around the knee, tabulated there at w / 16 by eight-point Gauss-Legendre quadrature
on each interval with the exact slope bound · σ at every knot (cubic Hermite), R ≡ 0 below the
window and R = R(top) + P(u) − P(top) above it. P is tabulated as well, from its closed form, at
64 knots per tanh scale over the solver's own ±10 scales, so one sample costs one exp and one
log1p beyond the Newton solve.

The divided difference divides by g(u1) − g(u0), computed from the same two excursions in double,
never by the float inputs: y is then the g'-weighted mean of bound over [u0, u1], a smooth
function of (u0, u1), so the solver's and the float's error in u is not amplified. Nor is the
table's: the interpolant G̃ is the exact antiderivative of its own derivative G̃', so y is the
exact mean of G̃' / g' and differs from the mean of f by at most max |G̃' − G'| / g'. Verified
against the brute force (the KCL bisected to double precision, Simpson's rule on the composite
transfer): G' = f · g' to 1.3e-7 of H · g' on the V5A, 6e-8 on the JCM800's V1a, 4e-8 on the
EL34; divided differences across the knee and across both plate ceilings within 3e-6 of H of
the brute-force mean; `AntiderivativeAntialiaserAntiderivativeDifferentiatesBackToTheTransfer`,
`...DividedDifferenceIsTheMeanOfTheCompositeTransfer` and `...InputMapInvertsTheGridSolve` pin the
three identities.

**The ill-conditioned point.** As g(u1) − g(u0) → 0 the quotient loses its precision. Under
`kMinimumInputDifferenceVolts` = 1e-4 V the antialiaser returns G̃'(u_m) / g'(u_m) at the
midpoint, from the same tables. The two branches differ by O(Δu² · f'') ≈ 1e-9 V there, the
quotient's round-off is at most 2 ulp(G) / 1e-4 ≈ 4e-8 V, and the measured largest step while the
difference sweeps through the threshold is 1.5e-5 to 2.7e-5 V, the float output's own quantisation
(−134 to −145 dB re H). Two identical samples and two a float ulp apart return the plate bound.
`AntiderivativeAntialiaserIsContinuousWhereConsecutiveSamplesNearlyCoincide` drives exactly those
cases. After `reset()` the first sample is the plain transfer, as `AdaaDiodePairPort` does.

### 15.2 What each change bought

`measure::alias_floor()` at 48 kHz and −12 dBFS, four builds of the same tree: as shipped before
part 4, ADAA alone (the biquads float), the double `Biquad` alone (the stages plain), and both:

| Setting | Shipped | ADAA alone | Double `Biquad` alone | Both (shipped now) |
| --- | --- | --- | --- | --- |
| JCM800 default (preamp 0.5) | −16.3 dB | −25.0 dB | −16.3 dB | −24.9 dB |
| JCM800 preamp 1.0, master 0 dB | −13.4 dB | −23.4 dB | −13.4 dB | −23.4 dB |
| JCM800 preamp 0.02, master 0 dB | −48.0 dB | −49.4 dB | −78.0 dB | −78.4 dB |
| 5150 default (pre gain 0.5) | −11.4 dB | −26.8 dB | −11.4 dB | −26.8 dB |
| 5150 pre gain 0.02 | −35.4 dB | −46.3 dB | −35.4 dB | −51.5 dB |
| IIC+ default | −0.1 dB | −14.8 dB | −0.0 dB | −14.8 dB |
| VH4 default | −13.3 dB | −23.0 dB | −13.3 dB | −23.0 dB |

The antialiaser buys 8.7 to 15.4 dB at the hot settings and nothing at the clean ones; the
double biquad buys 30 dB at the JCM800's preamp 0.02 and nothing at the hot ones, which is what
section 14 predicted. The 5150 at pre gain 0.02 is the one setting where the two add (−46.3 and
−35.4 to −51.5 dB). One stage alone in an 8x region: the V5A −81.2 to −102.4 dB at 3x its window
and −53.7 to −79.8 dB at 12x; the EL34 −88.1 to −101.2 dB at 8 V and −55.1 to −84.4 dB at its
38 V window.

### 15.3 What still folds, and why

None of the five hot settings reaches −33.8 dB (the JCM800 at preamp 1.0 is 10.4 dB short, the
5150 3.2 dB short of the spec's −30 dB, the IIC+ 19 dB). Three variants of the shipped build
locate the remainder: the transformer's tanh made linear, the sag stage made linear, and the
solver at 16 Newton iterations each leave the IIC+ at −14.7 or −14.8 dB, so none of them is the
source. At every default the worst line is the 20 kHz tone's 19th harmonic folding about 384 kHz
to 3995 Hz, and it is made inside the antialiased stages: a first-order antiderivative averages
f over a straight line between two samples, which is exact for the first stage's sine but not
for the later stages, whose input is the previous stage's clipped output with content up to the
oversampled Nyquist; the 26 to 29 dB one stage gains on a clean sine becomes 9 to 15 dB on a
cascade. What closes the rest is the maintainer's choice among 16x on top of ADAA (section 14
measured 16x alone at 8.5 to 18.7 dB), a band limit between the stages, or a higher-order
antiderivative. Measured through probes, 16x on top of both changes reads JCM800 default
−45.3 dB, preamp 1.0 −32.7 dB (1.1 dB short), 5150 −36.2 dB, IIC+ −36.9 dB, VH4 −35.3 dB: four
of the five hot settings cross the bound, at 2.0x the cost (5150 and IIC+ 8.92 ms of the 5.33 ms
block at `-O0`, 5.28 and 5.45 ms at `-O2`), so it closes the bar and does not fit the block.

### 15.4 The cost

`tests/benchmark/Benchmark.hpp`'s harness, 48 kHz, 256 frames, best of three runs of 200,
`DenormalGuard` on, every column compiled from source with the same flags, against the 5.33 ms
block:

| Build | JCM800 bench knobs | JCM800 default | 5150 | IIC+ | VH4 |
| --- | --- | --- | --- | --- | --- |
| Shipped, `-O0 -g` | 1.310 ms | 1.340 | 2.643 | 2.021 | 1.693 |
| ADAA alone, `-O0 -g` | 2.286 | 2.265 | 4.123 | 4.144 | 2.724 |
| Both, `-O0 -g` | 2.317 | 2.301 | 4.227 (79 %) | 4.289 (80 %) | 2.756 |
| Shipped, `-O2` | 0.776 | 0.793 | 1.571 | 1.153 | 1.029 |
| ADAA alone, `-O2` | 1.191 | 1.180 | 2.203 | 2.197 | 1.422 |
| Both, `-O2` | 1.208 | 1.196 | 2.260 | 2.250 | 1.452 |

The antialiaser costs 35 ns per stage-sample at `-O2`, one exp and one log1p; at the build's
`-O0` it costs 80 ns, the calls and the debug library's checks that no longer inline. The double
biquad costs 1 to 3 %. 16x on top doubles each row again (measured: 5150 8.916 ms and IIC+
8.919 ms at `-O0`, 5.280 and 5.447 ms at `-O2`), so neither fits the block at either level.

### 15.5 Golden files

All eight goldens of section 9 move: the four amplifiers through the antialiaser, and, for the
first time in this phase, `tube_screamer_48k/96k` and `dd3_48k/96k` through `Biquad`'s double
state. None is regenerated here.
