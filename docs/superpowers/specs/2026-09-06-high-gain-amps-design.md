# High-gain amplifiers: Peavey 5150, Mesa/Boogie Mark IIC+, Diezel VH4

**Status:** DA0 and DA1 shipped; DA2 and DA3 planned in
`docs/superpowers/plans/2026-09-06-high-gain-amps-part2.md`. Roadmap prefix **DA** (Dsp Amplifier).
Maintainer decisions of 2026-09-06: the VH4 is modelled with the EL34 its evidence points to; the VH4
module ships as a documented approximation (its dossier's verdict); the Mark IIC+ module reads the
five-band EQ as the dossier's series R-L-C traps.

## 1. Goal

Three white-box amplifier product modules that distort on their own, without a pedal in front,
in the order the maintainer set: Peavey 5150 first, Mesa/Boogie Mark IIC+ second, Diezel VH4
third. All three are built at once where the dependency graph allows it. Each module exposes one
`NodeDescriptor`, follows spec §5's shape (oversampled nonlinear chain, calibrated in decibels,
optional cabinet convolution), and reuses the JCM800's stage bricks instead of copying them.

The JCM800 2203 (`modules/jcm800`) is complete and is the reference implementation every new
module mirrors: file layout, parameter plumbing, RT-safety trap, golden-vector tests.

## 2. Scope

In scope, per amplifier, the one channel that carries its high-gain identity:

| Amplifier | Channel modelled | Power tubes | Controls exposed |
| --- | --- | --- | --- |
| Peavey 5150 (block letter, 1992) | Lead | 4 × 6L6GC | Pre Gain, Low, Mid, High, Post Gain, Resonance, Presence |
| Mesa/Boogie Mark IIC+ | Lead mode | Simul-Class: 2 × 6L6GC pentode, 2 × EL34 triode-strapped | Volume 1, Treble, Bass, Middle, Lead Drive, Lead Master, Presence, five-band graphic EQ (80, 240, 750, 2200, 6600 Hz) |
| Diezel VH4 | Channel 3 | 4 × EL34 (the stock tube by the dossier's evidence; KT77 deferred) | Gain, Bass, Middle, Treble, Volume, Deep, Presence, Master |

Every module also carries the same two calibration controls the JCM800 has today, Input Trim
(±24 dB) and Output (−60 to +12 dB), until the separate rig-gain-node task removes them from all
amplifiers at once. That removal is its own spec and is not part of this one.

Out of scope: clean and crunch channels, channel switching, effects loops, reverb, the IIC+'s
pull-shift switches and its 60/100 W selector, the VH4's MIDI and its channel 1, 2 and 4. Each
deferral is named here so that it is a recorded decision, not an omission.

## 3. Shared bricks move to the core (DA0)

`TriodeStage`, `PentodeStage`, `OutputTransformerStage` and `PowerSupplySagStage` sit in
`modules/jcm800` but depend only on `SushiDSP/math`. Three new amplifiers cannot depend on a
sibling product module (spec §7: a product module depends only on `sushi_dsp`), so the four
bricks move to the core, option A of the maintainer's choice on 2026-09-06:

```
include/SushiDSP/circuits/stages/TriodeStage.hpp            src/circuits/stages/TriodeStage.cpp
include/SushiDSP/circuits/stages/PentodeStage.hpp           src/circuits/stages/PentodeStage.cpp
include/SushiDSP/circuits/stages/OutputTransformerStage.hpp src/circuits/stages/OutputTransformerStage.cpp
include/SushiDSP/circuits/stages/PowerSupplySagStage.hpp    src/circuits/stages/PowerSupplySagStage.cpp
```

The namespace stays `SushiDSP`, which is where the four classes already live. Class names,
constructor signatures and behaviour do not change; this is a move, and the JCM800's regression snapshots
must pass bit-identically after it. `JcmToneStack` stays in `modules/jcm800`: it is the 2203's
own network.

`PentodeStage` is already parameterised by constructor (saturation current, thermal voltage, plate
saturation drive), so a different power stage is a different set of constants, not a different
class. The constants live beside the class as named presets in one header:

```
include/SushiDSP/circuits/stages/PowerTubePresets.hpp
    kEl34                        the JCM800 2203's constants, moved verbatim
    kPeavey5150Power6L6GC        dossier §7
    kMesaMark2cPower6L6GC        dossier §7.2
    kMesaMark2cPowerEl34Triode   dossier §7.3
    kDiezelVh4PowerEl34          dossier §7, EL34 row
```

The three dossiers showed that the constants are per installation, not per tube type:
`source_resistance_ohms` is the driver's Thevenin impedance plus the grid stopper, and `gain`
depends on the transformer's primary impedance, so two 6L6GC amplifiers get two presets. The
presets are therefore named by amplifier and power stage. `cathode_bias_volts` has no effect on
`PentodeStage`'s output (its node equation is bias-invariant); the field is kept for the record
and documented as such.

## 4. New shared bricks (DA1, parallel)

Three bricks more than one amplifier needs. Each is one class, one header, one source file, one
unit test, no dependency on any product module.

- **`ResonanceControl`** (`circuits/stages/ResonanceControl.hpp`). A low-frequency shelf inside
  the power section's negative-feedback path, the way the JCM800's presence already sits there
  for the top end. Used by the 5150 (Resonance) and the VH4 (Deep). Constructor takes the corner
  frequency and the depth range; the knob sets depth in [0, 1].
- **`GraphicEqualizer`** (`circuits/GraphicEqualizer.hpp`). Five fixed-centre peaking bands, each
  a `Biquad` from `SushiDSP/math`, ±12 dB per band, centre frequencies passed in at construction
  so the class is not Mesa-specific. Used by the IIC+ now; usable by a Mark IV later.
- **`FmvToneStack`** (`circuits/FmvToneStack.hpp`). The Fender/Marshall/Vox passive stack,
  parameterised by its component values. The JCM800's `JcmToneStack` is one instance of this
  topology with 2203 values. DA1 decided that `JcmToneStack` stays as it is: it is a
  three-biquad approximation that sits 6 to 16 dB from the passive stack, so delegating to
  `FmvToneStack` would move the JCM800 goldens. The two implementations coexist and
  `FmvToneStack` serves the new modules. The 5150 and the IIC+ both use FMV-shaped stacks with
  their own values. `FmvToneStack` as written implements the three-terminal wiper form of the
  mid pot; all four amplifiers wire that pot as a rheostat, and deriving the rheostat
  coefficients is part 2's first task.

## 5. Research dossiers (DA1, parallel with §4)

Every circuit fact an implementer needs comes from a dossier under `docs/reference/research/`,
following the two DD-3 dossiers' shape: schematic sources with their provenance, the signal path
stage by stage with component values, tube types and bias points, the tone network with its
values, the power section and its feedback network, and a list of what remained uncertain. A
module is not written from memory; if a value is not in the dossier it is not in the code.

```
docs/reference/research/2026-09-06-peavey-5150-design-dossier.md
docs/reference/research/2026-09-06-mesa-mark-iic-plus-design-dossier.md
docs/reference/research/2026-09-06-diezel-vh4-design-dossier.md
```

Where a SPICE deck is needed for a golden vector, it goes under `docs/reference/spice/` with the
existing `jcm_*` and `ts_*` naming, prefixed `p5150_`, `mk2c_`, `vh4_`.

Known risk, stated up front: the VH4's schematic is not published by Diezel and the circulating
traces are partial. If the dossier cannot establish channel 3's preamp topology with component
values, the VH4 module is downgraded to a documented approximation and the spec status line is
updated to say so before any code is written. The 5150 and IIC+ schematics are public and fully
legible.

## 6. Amplifier modules (DA2, parallel)

Three sibling modules, shaped the same as `modules/jcm800`:

```
modules/peavey5150/   include/SushiDSP/peavey5150/Peavey5150.hpp   Peavey5150ToneStack.hpp
modules/mesa_mark2c/  include/SushiDSP/mesa_mark2c/MesaMark2c.hpp  MesaMark2cToneStack.hpp
modules/diezel_vh4/   include/SushiDSP/diezel_vh4/DiezelVh4.hpp    DiezelVh4ToneStack.hpp
```

Each `<Amp>.hpp` is the `IAudioNode`, exposes `<amp>_descriptor()`, owns its stages by value, and
runs at the same oversampling factor the JCM800 uses unless the dossier shows a reason to differ.
Each module's `README.md` states its channel, its stage order, its calibration points and what it
does not model. Tests per module, mirroring `test_jcm800.cpp`: DC stability, THD sweep against the
dossier's figure, parameter clamps, an RT-safety trap case in `test_rt_safety.cpp`, and a
regression snapshot.

Signal paths, as far as the public schematics establish them; the dossier is authoritative and
overrides this section where it differs:

- **5150 lead.** Five cascaded 12AX7 triode stages with interstage attenuation and a cold-biased
  clipper stage, then the FMV stack, then Post Gain, then a long-tailed-pair phase inverter into
  the 6L6 push-pull, Resonance and Presence in the feedback loop. Fixed bias.
- **IIC+ lead.** Input stage, the FMV stack placed before the lead drive (the Mark family's
  defining ordering), Lead Drive stage into a further triode with heavy cathode bypass, Lead
  Master, the five-band EQ after the preamp, then the Simul-Class power section. The Simul-Class
  half is modelled as two `PentodeStage` pairs with different presets summed at the transformer.
- **VH4 channel 3.** Input stage, channel gain, two shared distorting stages, a DC-coupled cathode
  follower, its own stack, Volume, then a power section documented only in prose: EL34 quad, Deep
  and Presence in the feedback loop. Documented approximation per the dossier's verdict.

## 7. Registration and panels (DA3, serial)

`node_registry`, `PanelLibrary.cpp`, `docs/README.md`, `docs/reference/CHANGELOG.md` and the
design spec's §5 family section are each one file that all three modules touch. They land in one
serial task after DA2, with the three panel layouts drawn from the amplifiers' front panels the
way `kJcm800Controls` is.

## 8. Waves

| Wave | Tasks | Files | Waits on | Build after |
| --- | --- | --- | --- | --- |
| DA0 | Move four stages and add `PowerTubePresets.hpp`; update `jcm800` includes | `include/SushiDSP/circuits/stages/*`, `src/circuits/stages/*`, `modules/jcm800/*`, both CMake lists | nothing | `sd build && sd test`; JCM800 snapshots bit-identical |
| DA1a | `ResonanceControl` + test | `circuits/stages/ResonanceControl.*`, `tests/.../test_resonance_control.cpp` | DA0 | `sd build && sd test` |
| DA1b | `GraphicEqualizer` + test | `circuits/GraphicEqualizer.*`, `tests/.../test_graphic_equalizer.cpp` | DA0 | same build |
| DA1c | `FmvToneStack` + test | `circuits/FmvToneStack.*`, `tests/.../test_fmv_tone_stack.cpp` | DA0 | same build |
| DA1d–f | Three research dossiers (+ SPICE decks) | `docs/reference/research/2026-09-06-*`, `docs/reference/spice/{p5150,mk2c,vh4}_*` | nothing | none |
| DA2a | `modules/peavey5150` + tests | that module, `test_peavey5150.cpp` | DA1a, DA1c, DA1d | `sd build && sd test` |
| DA2b | `modules/mesa_mark2c` + tests | that module, `test_mesa_mark2c.cpp` | DA1b, DA1c, DA1e | same build |
| DA2c | `modules/diezel_vh4` + tests | that module, `test_diezel_vh4.cpp` | DA1a, DA1f | same build |
| DA3 | Registry, panels, RT-safety cases, docs, changelog | `node_registry`, `PanelLibrary.cpp`, `test_rt_safety.cpp`, `docs/*` | DA2a–c | `sd build && sd test`; all three load in `host_gui` |

DA1a–f run as one wave of six agents with disjoint files. DA2a–c run as one wave of three. Each
module task is bounded by one build; if a module's tone stack derivation turns out to need its
own build cycle, it is split into a `<Amp>ToneStack` task ahead of the node task.

## 9. Acceptance

- The JCM800's regression snapshots pass unchanged after DA0.
- Each new module passes DC stability at every knob extreme, matches its dossier's THD figure
  within the tolerance the dossier states, and passes the `operator new` trap in `process()`.
- Each module loads in `host_gui` with its panel and saves and loads through `rig_preset`.
- Every fact in a module's code traces to a line in its dossier.
