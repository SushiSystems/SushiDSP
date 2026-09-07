# The grid knee, antialiasing and the volt-true power stage

**Status:** Part 1 built, 2026-09-07: decisions 1, 5, 6 and 7 taken and implemented (section 12); decisions 2, 3 and 4 open. Evidence in `.superpowers/sdd/amp-gain-staging/diagnosis.md` and `.superpowers/sdd/amp-gain-staging/da5-grid-law-report.md`. Succeeds DA4 (`2026-09-07-triode-plate-swing-design.md`) as the DA5 spec.

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
