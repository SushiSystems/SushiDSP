# What jrb32's traced schematic settles, 2026-09-20

Status: closed. The readings below are in the code and in
`modules/nodes/mesa_mark2c/README.md`; the dossier is archived and is not edited, so this document
carries the corrections to it.

The Mark IIC+ dossier's §8 item 16 said jrb32's traced schematic (S3/S8) and Dan__O's SLOCLONES
version sat behind Google links it could not fetch, and called opening them "the cheapest next
step in this whole list". That note was already stale when it was written: §1's S8 row records the
same drawing as downloaded on 2026-09-08. It came down again today from the link in that row, as a
two-page vector PDF whose text and line coordinates read directly, so every connection below was
checked against the drawing's own geometry rather than off a picture.

The dossier is a reading of S1, the 1983 factory scan. Where the two disagree, S8 wins here: it was
traced off a chassis and its author calls it confirmed on that chassis, it is vector rather than a
low-resolution scan, and S3 states that the factory drawing carries deliberate errors.

## The graphic equaliser's traps: items 1 and 2 close

| Panel label | Slider | R | L | C | f0 | Q |
|---|---|---|---|---|---|---|
| 60 Hz | 50 K | R51 470 | L1 1.0 H | C51 3.3 uF | 87.61 Hz | 1.171 |
| 240 Hz | 50 K | R52 470 | L2 0.39 H | C52 0.47 uF | 371.74 Hz | 1.938 |
| 750 Hz | 50 K | R53 470 | L3 0.22 H | C53 0.22 uF | 723.43 Hz | 2.128 |
| 2200 Hz | 50 K | R54 1 K | L4 0.068 H | C54 0.15 uF | 1575.87 Hz | 0.673 |
| 6600 Hz | 50 K | R55 1 K | L5 0.033 H | C55 0.033 uF | 4822.88 Hz | 1.000 |

S1 gave the dossier 1.5 H on the first band and 0.12 H on the third, which put them at 71.5 Hz and
979.5 Hz. S8 gives 1.0 H and 0.22 H. The dossier already treated three of five centres landing on
S6's independently derived Mark IV figures as strong confirmation that it had read the L and C
values right; on S8's values all five land, to five digits. The model was a minor third low on the
band the panel calls 60 Hz — the panel label the dossier reports as 80 Hz is itself 60 Hz on this
drawing — and a fourth high on the band S2 calls the most powerful of the five.

Both inductors were `[single source]` readings of one hand-drawn glyph. They are now two sources
that agree, and item 2's alternative reading, 1 H, is the one that won.

## The equaliser's active stage: item 11 closes

S1 left the dossier unable to say how Q1 to Q4 connect, and item 11 concluded that without it "the
EQ can be modelled as anything better than five biquads". S8 draws the stage in full: R41 1 M from
the input to ground, C41 0.1 uF coupling in, R42 and R43 470 K, R45, R46, R48 and R49 3.32 K,
R47 22.1 K, C46 and C47 10 pF, C48 10 uF out, and the two buses the slider bank hangs between —
the sliders' top ends on one rail, their wipers through R-L-C to the shared bottom bus, and that
bus to ground through LDR5A, with the 68 K that later units carry as the EQ pop fix.

The topology is readable. Modelling it is still a four-transistor amplifier with a Newton solve,
which is a task of its own size, so the five biquads stay for now and the plan's A4 records the
change of reason: it was blocked on a source, and it is now open but unbuilt.

## The feedback network and presence: §5.2 corrected

The dossier calls presence "a variable shunt in the feedback leg, the standard Fender/Marshall
arrangement". It is not a shunt. S8 draws the 250 K as a rheostat in series between the 8 ohm tap
and the rest of the leg, and its wiper stub connects to nothing else on the sheet.

```
8 ohm tap - PRESENCE 250 K - R60 1.5 K - P
P  -[ R61 56 K || C61 4.7 nF ]- Q
Q  - R64 22 K  - X          X - R65 100 K - V5A grid
Q  - R63 1.5 K - G          X - R66 150 K - V5B grid
G  - R62 3.3 K  - ground    X - R67 470   - V5A and V5B cathodes, joined
G  - C64 47 nF  - ground
G  - C65 0.1 uF - V5B grid
```

The signal path of the feedback is C65, from G to V5B's grid; the leg's R62 3.3 K against R63 and
what sits above it is the divider that sets how much arrives. That is the arrangement Marshall and
Mesa both use, and V5B's grid is the pair's inverting input. X is not the injection point: it is
where the pair's own DC return leaves, through 470 + 22 K + 1.5 K + 3.3 K to ground. A first
reading of this sheet on 2026-09-20 had it the other way round and is corrected here.

Two readings the model carried are wrong on S8. The plate loads are R68 82.5 K and R69 90.9 K, not
the "82 K" the low-resolution sheet gives for both; the wider load on the b half is what matches the
two plates' amplitudes. And R67's 470 runs from X to the joined cathodes, in series with the tail,
where the model had taken it as a shunt between them. With the tail at 27.27 K rather than the 15 K
glyph, the shared cathode follows 0.49678 of the two grids instead of the 1 / 2.2552703 = 0.4434 the
old constant imposed, which is 1.06 dB more drive into the power tubes.

Item 6, the phase inverter's DC operating point, still does not close on those values, but it now
has real ones to fail on.

## What the leg does, solved

The leg is not a shelf, and two capacitors are why. C61 bridges R61 above 605 Hz, which sends more
treble back; C64 shunts the divider's ground leg above 1026 Hz, which sends less. Solved as a
ladder into the 1068.5 ohm the pair's cathodes present at X, opening the presence pot from 0 to 1
removes this much feedback:

| Frequency | 80 Hz | 600 Hz | 3 kHz | 10 kHz |
| --- | --- | --- | --- | --- |
| Feedback removed | 14.15 dB | 16.21 dB | 25.94 dB | 34.38 dB |

So presence lifts the treble past the bass by about 20 dB across its travel, which the +/- 9 dB
shelf it replaces could not do. `test_power_amp_feedback.cpp` nulls the DK solve against this same
ladder and pins the two end figures.

## What this does not settle

The output transformer's primary impedance and turns ratio are absent from S8 as they are from S1,
so item 3 stands and with it the absolute depth of the feedback loop. Item 15 stands: nobody has
published a bench measurement of a Mark IIC+.

## What closing the loop measured, 2026-09-21

With `kTapToFeedbackVolts` at zero the loop is open and the forward path measures 38.33 dB at
80 Hz, 39.56 at 600 Hz, 61.59 at 3 kHz and 53.31 at 10 kHz, with nothing left in silence at any
presence position. Those four numbers are the only clean anchor the calibration has, because every
measurement taken at presence 0 with the loop closed is contaminated.

It is contaminated by an oscillation. At the arbitrary 151.7 the presence sweep came out 29.69 dB
at 3 kHz, above the 25.94 dB the leg itself travels, which no stable negative loop can do; at
80 Hz and 600 Hz it was over its bound too. Driving the amplifier and then cutting the drive left
92 percent of the peak still there 213 ms later at presence 0, against 3e-6 at presence 0.5 and 1.
Cutting the depth 5.4 times to 28.15 moved that figure to 0.9204, which is what a limit cycle does
and what a loop responding to its depth does not. Counting the sign changes in the residual put it
at 11.76 kHz.

That frequency is the whole finding. It is the pole of the output transformer, which is still the
JCM800's 60 Hz, 8 kHz and 0.3, and it is inside the loop. How deep the feedback can run is decided
by the phase of what it loops around, so the depth cannot be calibrated while the thing it loops
around belongs to another amplifier. Phase A item 6 has to come before this.

At 14.0 the amplifier no longer rings, and every one of the 1002 tests passes. It is not calibrated:
at presence 0 a 9 dB resonance sits at 10 kHz, 62.33 dB against the 53.31 the open loop gives, and
the presence knob runs backwards above it, sweeping -8.88 dB at 10 kHz. The value is in the tree so
that the tree is sound, and it is marked that way at its definition.

## What a search outside the repository found, 2026-09-21

The output transformer's primary impedance, turns ratio, primary inductance and leakage inductance
are unpublished. Mesa's own store, replacement-transformer vendors and three recent forum threads
asking this exact question all end the same way, the most recent of them from November 2025. The
part numbers are corroborated across several independent posts reading the same EIA date codes:
the Simul-Class transformer the IIC+ carries is SC-152019B, also sold as 562003, wound by
Woodward-Schumacher. No figure accompanies them. Banzai list a replacement, part 562004, at
"Raa approx. 3.6k", which is a vendor's copy for a different part number and is not a measurement
of the original. So the five stand-ins stay stand-ins, and the feedback loop's calibration stays
blocked.

The search did reach S8's power-supply sheet, which item 16 recorded as unreachable. It gives
C80 to C83 at 220 uF 300 V, annotated as four on Simul-Class; R84 and R85 at 5.6 k 2 W from B to
C; C84 to C87 at 30 uF 500 V; R86 at 1 k from C to D; and silicon rectification, D8 to D11, off a
centre-tapped secondary. None of it yields a sag figure, because how far the rail falls depends on
the transformer's winding resistance and the choke's, which the sheet does not give.

Two things in it are worth carrying to the dossier's open questions rather than into the model.
R86 reads 1 k here as it does on S1, so two independent transcriptions now agree on the resistor
and the disagreement in question 7 is more likely in the printed voltage. And the bias-supply
voltages disagree with the dossier's own table: S8's sheet prints -67 V for Simul-Class where the
dossier read -61 V, and -47 V for the 60 W where it read -43 V.
