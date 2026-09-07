# Room acoustics: a hybrid image-source and feedback-delay-network room, rendered binaurally

**Status:** approved by the maintainer on 2026-09-08. Nothing in it is built; the implementation
plan is `docs/superpowers/plans/2026-09-08-room-acoustics.md`. Roadmap prefix **DE** (Dsp
Environment); phases DE0 through DE8 below. The prefix collides with none of DA, DC, DD, DI, DK,
DR, DS or DT, which are the ones `docs/design/sushidsp.md` §9 and the specs in this directory use.

Approval settled three of the judgements §13 had left open, and the sections below are written as
the rulings rather than as the draft. The modal solver runs in the hybrid mode as well as the max
mode (§9a.1, §10, §14), `direct_delay` defaults to `compensated` (§9.4), and the converted SADIE II
set ships in the tree under `assets/hrtf/` with its converter in `tools/dsp/` (§9.2). §13 keeps the
judgements that stay open, each still the maintainer's to overrule.

Five decisions of the maintainer's, taken as given and not argued here: the room is a hybrid of
image-source early reflections and a feedback delay network tail; the output is binaural over
headphones through an HRTF; the room's dimensions and materials, both positions, the microphone and
the cabinet's directivity are all adjustable while audio runs; SIMD is allowed once the plain C++
has been engineered and measured; and, arriving after the first four, a "max" quality mode above the
hybrid, asked for as "max ray tracing" and asked for because of the low end. §9a designs what that
reason needs, which is a wave solver below the Schroeder frequency, and says why more rays alone
would not have delivered it.

## 1. Goal

A product module, `modules/room`, whose one node takes the cabinet's mono output and returns two
channels that sound like that cabinet standing in a particular room, heard from a particular spot.
The maintainer's test is not a number: it is whether, with his own garage's dimensions and surfaces
typed in and the amplifier placed where it stands, the headphones feel like the garage. Every
acceptance item in §12 is a measurement chosen because it stands between the model and that
feeling; §11 says which parts of full realism do not.

The node is the first in the tree that needs two channels for a physical reason rather than a pan
law: the two ears hear different things. It sits on the channel routing DS0 built
(`docs/superpowers/specs/2026-09-07-stereo-tracks-design.md` §2 and §3) and declares
`PortLayout{1, 2}` like `routing::PanNode`.

## 2. What the ear uses, and what it does not

The design is sized by what makes a room recognisable, because that is what "his own room" means.
In the order they matter for a guitar cabinet in a small room:

1. **The ratio of direct to reflected energy and the timing of the first few reflections.** The
   floor bounce and the wall behind the cabinet arrive within 2 to 10 ms and set the sense of
   nearness. Their delays follow from geometry to the sample and the image-source method gets them
   exactly.
2. **The decay time per frequency band.** A bare concrete garage rings for over a second and keeps
   its highs; a carpeted bedroom with a bed and curtains dies in 300 ms and loses everything above
   2 kHz first. This is the materials table through Eyring's formula, and it is the one thing that
   must come from the room and never from a knob.
3. **Where the cabinet is.** Interaural time and level differences of the direct sound and the
   lateral early reflections. The HRTF supplies these; front-versus-back needs the pinna cues a
   measured HRTF has and a spherical-head model lacks.
4. **The tone changing as you move around the cabinet.** Directivity, §7.
5. **Low-frequency modes.** Below the Schroeder frequency f_s = 2000·sqrt(RT60 / V) Hz, which is
   about 260 Hz for a 60 m³ garage with RT60 = 1 s, a small room's response is a sparse set of
   standing waves. The image-source sum with near-unity reflection factors reproduces the modal
   build-up exactly if it runs to a high enough order; at the order this design can afford (§4) it
   reproduces the first few bounces of it. The FDN's modes are its own, not the room's. The modal
   solver of §9a.2 supplies the room's, and it runs in both quality modes: it is the layer that
   puts pressure in the corners and swells the bottom end when the cabinet meets a wall, which is
   the part of the room the maintainer said he wants to feel.

Everything else on the "everything modelled" list, from edge diffraction to the phase of a wall's
reflection factor to the exact spatial distribution of the diffuse tail, is either inaudible at
these distances or is masked by items 1 to 4, and §11 defends leaving each out.

## 3. Bricks and who owns what

```
modules/room/include/SushiDSP/room/
    RoomScene.hpp              the shoebox, six surface materials, source, listener and microphone
                               poses; a plain struct, control-thread owned, the host writes it
    Materials.hpp              the absorption table, six octave bands per material, cited per row
    BandResponse.hpp           six band gains and the arithmetic on them (product, dB, power mean)
    BandResponseFit.hpp        fits a BandResponse to two Biquads (4th-order minimum phase)
    AirAbsorption.hpp          ISO 9613-1 attenuation per band for a path length
    ImageSourceSolver.hpp      control thread: RoomScene -> EarlyTapSet
    EarlyTapSet.hpp            up to kMaxEarlyTaps taps: delay, gain, two Biquad coefficient sets,
                               a direction (for the ears) or a pattern gain (for the microphone)
    CabinetDirectivity.hpp     piston-array gain per band for an outgoing direction
    MicrophoneModel.hpp        polar pattern, proximity effect, optional published on-axis curve
    LateReverbDesign.hpp       control thread: RoomScene -> FDN line lengths, per-line filters,
                               input gain, crossover time
    RoomRenderSet.hpp          everything the audio thread reads for one scene: EarlyTapSet, the
                               FDN design, the direct-path HRIR pair; immutable once published
    EarlyReflectionRenderer.hpp   audio thread: one shared input ring, the taps, the encode
    LateReverb.hpp             audio thread: the 16-line FDN and its binaural or mono output
    AmbisonicEncoder.hpp       third-order encode gains for a direction, 16 coefficients
    BinauralDecoder.hpp        16 Ambisonic channels -> 2 ears through PartitionedConvolution
    HrtfSet.hpp                minimum-phase HRIRs, ITD table, direction grid, neighbour table
    HrtfSetLoader.hpp          reads the converted asset file; SphericalHeadHrtf builds one from
                               the Duda-Martens model with no file at all
    IRoomHost.hpp              what a host calls to install a RoomScene and an HrtfSet
    RoomNode.hpp               the IAudioNode: topology, the swap, the wet/dry mix
    ModalSolver.hpp            control thread (DE3): the shoebox's eigenmodes below the Schroeder
                               frequency, their damping and their source and ear gains
    ModalBank.hpp              audio thread (DE3): up to kMaxModes second-order resonators
    CrossoverPair.hpp          audio thread (DE3): the Linkwitz-Riley pair that splits the field
                               at the Schroeder frequency
    max/RayTracer.hpp          control thread (DE8): stochastic rays -> per-band directional
                               energy histogram of the late field
    max/LateIrSynthesizer.hpp  control thread (DE8): histogram -> a binaural late impulse response
```

No brick names another except `RoomNode` (which knows the topology), `ImageSourceSolver` and
`LateReverbDesign` (which consume `Materials`, `BandResponse`, `BandResponseFit`, `AirAbsorption`
and `CabinetDirectivity`), and `RoomRenderSet`, which is the one struct both threads see. The
solver and the renderer share `EarlyTapSet` and nothing else, so the renderer can be tested by
hand-writing a tap set, and the solver by checking arrival times without rendering a sample. That
is the DD-3's shape (`modules/dd3/README.md`, "The blocks and who owns each") with a control-thread
half added.

The module depends on `sushi_dsp` only, as `modules/dd3/CMakeLists.txt` does. It reuses
`Biquad`, `Fft`, `PartitionedConvolution`, `RtSwapSlot`, `SmoothedParameter` and `WavFile` from
the core and adds nothing to the core. `BandResponseFit` is the one candidate for promotion to
`include/SushiDSP/math/` later; it stays in the module until a second consumer appears.

## 4. DE0: geometry, materials and the two fits

### 4.1 The shoebox and the image-source method

The room is a rectangular box, dimensions L_x, L_y, L_z, six surfaces each with one material. The
image-source method (Allen and Berkley, "Image method for efficiently simulating small-room
acoustics", JASA 65(4), 1979) mirrors the source across every wall recursively; each image at
position **p**_i is a copy of the source arriving after |**p**_i − **r**| / c seconds, attenuated by
1 / |**p**_i − **r**| and by the product of the reflection factors of the walls it crossed.

The count grows with the cube of the order. Images with reflection order at most N are the lattice
points with L1 norm at most N in three dimensions:

    count(N) = (2/3)·N·(N+1)·(2N+1) + 2N + 1

    N = 1:   7      N = 2:  25      N = 3:  63      N = 4: 129      N = 5: 231      N = 6: 377

The direct sound is one of these, so order 4 gives exactly 128 reflections. That is
`kMaxEarlyTaps = 128`, and it is a fixed capacity: every buffer the audio thread reads is sized for
128 taps at `prepare()` and never grows.

**Which taps the set keeps is decided by arrival time, not by order.** The image set of order N is
complete only up to the arrival of the earliest missing order-(N+1) path, and in a room 2.5 m high
that path bounces between floor and ceiling: t_c ≈ (N+1)·L_min / c. For N = 4 and L_min = 2.5 m,
t_c = 36 ms. After t_c the image sum is missing echoes and would sound thin, so the solver drops any
tap arriving after t_c and hands the tail to the FDN from t_c onwards. The perceptual mixing time,
after which individual reflections stop being resolvable, is about 2·sqrt(V) ms (Polack's rule as
quoted by Schlecht and Habets, below): 15 ms for a 60 m³ garage, 12 ms for a 36 m³ bedroom. Order 4
therefore covers the whole audibly discrete part of the response with a margin of two, and the
only argument for order 5 (231 taps, 1.8× the cost) is a room taller than 4.5 m, which the
maintainer does not have. A larger room lowers no requirement here; it raises t_c, and a 10 m room
at order 4 hands over at 146 ms, which is well past its mixing time of 63 ms.

`ImageSourceSolver` runs on the control thread. For each kept image it records the delay in
fractional samples, the 1/r gain, the list of walls crossed (as a `BandResponse` product), the
outgoing direction from the cabinet (for directivity, §7) and the incoming direction at the
listener (for the ears) or the incidence angle at the microphone (for the pattern, §8). It then
multiplies the wall product by the directivity band gains and the air absorption for that path
length and hands the six numbers to `BandResponseFit`. One fitted filter per tap, whatever the
number of physical factors in it. That is the efficiency the whole design rests on: the audio
thread never knows what a material is.

### 4.2 Materials as filters

A material is six random-incidence absorption coefficients α at the octave centres 125, 250, 500,
1000, 2000 and 4000 Hz, as ISO 354 reverberation-room measurements report them. The pressure
reflection factor magnitude per band is |R| = sqrt(1 − α); its phase is taken as zero, so the
fitted filter is minimum phase. The table ships in `Materials.hpp` with a citation on every row.
The rows are copied from two sources that reprint the classic Knudsen and Harris measurements and
later ISO 354 data: Everest and Pohlmann, *Master Handbook of Acoustics*, 5th ed., McGraw-Hill
2009, Appendix "Absorption coefficients", and Vorländer, *Auralization*, Springer 2008, Appendix.
The first release carries the surfaces a garage or a bedroom has: unpainted and painted concrete,
concrete block, brick, gypsum board on studs, plywood panel, wood floor, carpet on concrete,
carpet on underlay, glass window, heavy curtain, acoustic foam, a steel garage door, and an
"open doorway" pseudo-material with α = 1 in every band. Representative rows, from Everest:

| Material | 125 | 250 | 500 | 1k | 2k | 4k |
| --- | --- | --- | --- | --- | --- | --- |
| Concrete, unpainted | 0.01 | 0.01 | 0.02 | 0.02 | 0.02 | 0.03 |
| Concrete block, painted | 0.10 | 0.05 | 0.06 | 0.07 | 0.09 | 0.08 |
| Gypsum board 12.7 mm on studs | 0.29 | 0.10 | 0.05 | 0.04 | 0.07 | 0.09 |
| Heavy carpet on concrete | 0.02 | 0.06 | 0.14 | 0.37 | 0.60 | 0.65 |
| Glass, ordinary window | 0.35 | 0.25 | 0.18 | 0.12 | 0.07 | 0.04 |

The numbers in the shipped header are transcribed from the cited pages, not from this table; this
table only shows the shape of the data. Each material also carries one scattering coefficient s in
[0, 1] (Vorländer, ch. 11): the fraction of reflected energy that leaves specularly is (1 − s) and
goes to the tap, and the rest is handed to the tail's input gain. Default s = 0.1 for a flat
painted wall and 0.4 for a wall with shelves on it; the user picks from three words, not a number.

`BandResponseFit` turns six band gains into two `Biquad` coefficient sets. Its target is the six
log-magnitudes; its structure is a low shelf, a high shelf and two peaking sections' worth of
freedom folded into two biquads; its method is a weighted least-squares fit of the log-magnitude at
the six centres, started from a closed-form two-shelf guess and refined by at most eight
Gauss-Newton steps in `double`. It is control-thread code, so its cost is a few thousand flops per
fit; 128 taps plus 16 FDN lines is under 150 fits per scene change, well under a millisecond.
Acceptance is 1 dB at every band centre for every row of the materials table raised to any power
from 1 to 4 (one to four bounces off the same wall), and the test enumerates exactly that.

Air absorption follows ISO 9613-1:1993 at 20 °C and 50 % relative humidity, fixed, no knob. At
those conditions the attenuation is 0.0297 dB/m at 4 kHz and 0.105 dB/m at 8 kHz, so a 30 m early
path loses 0.9 dB and the early taps barely feel it; a one-second tail travels 343 m and loses
10 dB at 4 kHz on top of what the walls take. Air absorption therefore matters to the FDN's
per-band decay (§6) and is included in the taps only for consistency.

### 4.3 Acceptance for DE0

Unit tests, no audio: the image count for N = 1 to 4 equals the formula; for a 6 × 4 × 2.5 m room
with the source at (1.5, 2, 0.5) and the listener at (4, 2, 1.2) the six first-order arrival times
match hand-computed |**p**_i − **r**| / c within 1e-9 s; every fit clears the 1 dB bound; the tap
set is sorted by arrival and contains no tap past t_c.

## 5. DE1: the early-reflection renderer and the swap

### 5.1 The renderer

All 128 taps read one signal: the cabinet's mono output. So `EarlyReflectionRenderer` owns one
ring buffer of the input, sized at `prepare()` for the longest t_c the room bounds allow (a 12 m
room at order 4 gives 175 ms, 8400 frames at 48 kHz, 33 kB), and each tap is a read at its own
fractional offset. Per tap per sample the work is:

| Step | Flops | Note |
| --- | --- | --- |
| Fractional-delay read | 8 | 4-tap Lagrange; the coefficients are constant per tap and precomputed in the set |
| Gain | 1 | 1/r, scattering, level |
| Two biquads | 18 | the fitted material, directivity and air filter |
| Encode | 16 | third-order Ambisonic gains (ears) |
| | **43** | or 28 for the microphone, whose encode is one pattern gain |

128 taps × 43 flops × 48 000 samples/s = 264 Mflop/s; per 256-frame block, 1.41 Mflop. The biquad
recursion is dependent across samples and independent across taps, so the loop is tap-outer,
sample-inner, and the 16 Ambisonic accumulation buffers are written once per tap. A scalar core
sustaining 1.5 Gflop/s on that loop shape takes about 0.9 ms per block. This is the brick the
budget (§10) expects to dominate, and it is the one whose loop is eight independent lanes wide
(sixteen groups of eight taps through identical biquad structures), which is the SIMD opportunity
DE7 measures before taking.

The Ambisonic choice needs its argument, because the obvious alternative is to run every tap
through its own HRIR pair. A 128-tap minimum-phase HRIR pair is 256 multiply-adds per tap per
sample; 128 taps make that 1.57 Gflop/s, six times the entire early renderer. Encoding each tap to
third-order Ambisonics (16 channels) costs 16 multiply-adds, and the 16 channels are decoded to two
ears once per block through a fixed bank of 32 FIR filters (§9), whose cost does not depend on the
tap count at all. Third order rather than second because the strongest lateral reflection's
direction is a room-size cue and second order (9 channels) blurs it to about 30°; fourth order (25
channels) adds 9 flops per tap and 18 decoder filters for a sharpening the reflections, which are
already spread by the head's own movement, do not need. The direct sound does not go through this
path: it gets its own exact HRIR pair (§9).

### 5.2 What the audio thread reads, and the swap

Everything the audio thread reads that depends on the scene lives in one `RoomRenderSet`: the
taps, the FDN design (§6), the direct-path HRIR pair and ITD, the crossover gains. It is built
whole on the control thread and published through `RtSwapSlot<RoomRenderSet>`, exactly as
`Jcm800` publishes a `StreamingConvolution` through `cabinet_slot_`
(`modules/jcm800/include/SushiDSP/jcm800/Jcm800.hpp`). The audio *state*, meaning the input ring,
the biquad histories, the FDN lines and the convolvers' spectra, belongs to the node and survives
a publish; a set carries coefficients only. A publish allocates on the control thread and the audio
thread does one atomic load per block; the host calls `reclaim()` once per GUI frame, as it already
does for cabinets through `ICabinetHost::reclaim_cabinets`.

A new set has different tap delays and filters, so switching sets between two blocks would click.
When `process()` sees a new set it renders the block twice, once from the retiring set into the old
biquad states and once from the new set into a second bank of states that start at zero, and
crossfades the two over 512 frames with a raised cosine. For two blocks the early renderer costs
twice; §10 carries that peak. The new states' zero start is a transient of a few samples of a
4th-order shelf, under the fade, and inaudible; the test in §5.3 confirms it by measurement, not
by assertion. The old set's states are then reused as the next spare bank, so the node owns
exactly two banks and allocates nothing.

The alternative, smoothing each tap's delay toward its new value so a moved amplifier produces a
Doppler slur the way a real move does, was considered and rejected: taps appear and disappear as
the geometry changes, so a per-tap ramp still needs a fade for the births and deaths, and the
slur is a sound the maintainer will never hear in his garage because nobody moves a 4x12 while
playing. The crossfade is the same mechanism for every change, which is one brick instead of two.

While a publish is being built the audio thread renders the old set; there is no silent block. A
scene edit that arrives while a crossfade is running is coalesced by the host into the next
publish, so the node never crossfades more than two sets. The host is asked to publish at most 30
times a second while a slider drags; `ImageSourceSolver` for 129 images plus 150 fits is under a
millisecond of control-thread time, so the rate is bounded by the GUI, not by the solver.

The related open item stays open: `RigController::publish()` (`apps/host_gui/RigController.hpp`)
recompiles the graph on a structural edit and the fresh nodes start from empty state
(`docs/superpowers/specs/2026-09-05-dd3-spx90-design.md`, "Prerequisites owned by other tracks",
which names the function by its earlier name). Dragging the room node to another slot will cut its tail
until that lands. Moving the amplifier inside the room is a scene edit, not a graph edit, and does
not touch it.

### 5.3 Acceptance for DE1

With the microphone listener set to an omnidirectional pattern and the FDN muted, the node's
rendered impulse response has its first-order peaks at the DE0 arrival times within half a sample;
a publish mid-signal produces no sample-to-sample step larger than the signal's own peak
difference over the fade (the test renders a 1 kHz tone through a swap and asserts the residual
against a reference crossfade). `RoomNode::process` passes the `operator new` trap in
`tests/functional/unit/test_rt_safety.cpp` with a publish landing between two blocks.

## 6. DE2: the late tail

### 6.1 Shape

A feedback delay network of 16 delay lines (Jot and Chaigne, "Digital delay networks for designing
artificial reverberators", 90th AES Convention, 1991) with a 16 × 16 Hadamard feedback matrix,
applied as a fast Walsh-Hadamard transform (64 additions per sample), a 4th-order absorptive filter
on every line from `BandResponseFit`, and Jot's tone-correction filter on the output so the
frequency-dependent decay does not also tilt the tail's spectrum.

Sixteen lines and not eight or thirty-two: the count sets two densities. The echo density grows as
a polynomial whose coefficients come from the line lengths (Schlecht and Habets, "Feedback delay
networks: echo density and mixing time", IEEE/ACM TASLP 25(2), 2017); with 16 lines it reaches the
perceptual mixing time of a small room within the FDN's first two recirculations, and because the
tail is switched in at t_c behind 128 discrete taps, the sparse first recirculation is masked by
the image set anyway. The modal density is set by the sum of the line lengths, not their count
(below). Eight lines with the same total length give the same modal density but a sparser onset
and, at the short lengths a small room wants, an audible flutter on transients. Thirty-two doubles
the filter cost for a tail that is already dense.

### 6.2 Line lengths from the room

A room's tail rings without metal when its modes overlap: the mode bandwidth is 2.2 / RT60 Hz and
the FDN's mean modal spacing is 1 / Στ_i Hz, so three modes per bandwidth needs

    Σ τ_i ≥ 3 · RT60 / 2.2 ≈ 1.4 · RT60.

For a garage with RT60 = 1.2 s at 500 Hz, the sixteen lines must total at least 1.7 s, a mean of
105 ms each; for a bedroom at 0.35 s, 30 ms each. `LateReverbDesign` therefore sets the mean line
length from the room's mid-band RT60, spreads the sixteen lengths logarithmically over a 1 : 2
range about it so no two lines share a recirculation period, and rounds each to the nearest prime
number of samples so the periods share no common factor (Schroeder's rule, kept by Jot). The
lines are allocated at `prepare()` for the longest room the scene bounds allow (RT60 = 3 s gives
263 ms mean, 526 ms longest, 25 000 frames, 100 kB per line) and a shorter room reads at a shorter
offset. A length change is a read-offset change, and the line crossfades between the old and new
read heads over the same 512 frames the early renderer uses.

### 6.3 Decay time from the materials, and nothing else

The RT60 per band is Eyring's, with air absorption:

    RT60(f) = 0.161 · V / ( −S · ln(1 − ᾱ(f)) + 4 · m(f) · V )

where ᾱ(f) is the surface-area-weighted mean absorption over the six surfaces and m(f) is the
ISO 9613-1 energy attenuation coefficient per metre. Eyring rather than Sabine because Sabine's
linear form overestimates the decay time of an absorbent room by a factor that reaches 1.5 at
ᾱ = 0.5, which a carpeted bedroom with curtains approaches at 4 kHz; the two agree within 5 % below
ᾱ = 0.1, so a concrete garage comes out the same either way and the acceptance test (§6.5) checks
both rooms.

Each line's absorptive filter then has the band gains g_i(f) = 10^(−3 · τ_i / RT60(f)), so that
after RT60(f) seconds of recirculation every band is down 60 dB, and `BandResponseFit` turns the
six numbers into two biquads exactly as it does for a tap. Here the same brick serves both halves
of the hybrid, which is why it is a brick.

The tail's input level is the one place where the two halves must agree. The tail begins at t_c
with a level set so that its energy per unit time continues the image set's trend at t_c:
`LateReverbDesign` sums the energy of the kept taps in the last 5 ms before t_c, scales it by the
statistical decay over that interval, and sets the FDN input gain so its expected energy density at
t_c matches. The scattered fraction of the early energy (§4.2) is added to the same input. The
acceptance test measures the Schroeder backward-integrated decay curve across t_c and refuses a
step of more than 2 dB.

The source's directivity enters the tail as its per-band power average (the directivity index,
§7), since a diffuse field receives the cabinet's total radiated power per band and not its
on-axis response.

### 6.4 Two ears, or one microphone

For the microphone listener the tail is one output: the sixteen lines summed through a ±1 vector.
For the ears it is two. A real diffuse field at the two ears is coherent below about 500 Hz and
incoherent above: the interaural coherence of a diffuse field is sin(k·d) / (k·d) with d the
inter-ear spacing of about 0.17 m (Lindevald and Benade, "Two-ear correlation in the statistical
sound fields of rooms", JASA 80(2), 1986). So the tail takes two orthogonal ±1 output vectors,
A and B, and forms L = A and R = H_γ(A) + H_s(B), where H_γ and H_s are the coherence function and
sqrt(1 − γ²) as fitted 4th-order filters through the same `BandResponseFit`, evaluated at the six
centres. Both ears then pass through the diffuse-field average of the HRTF set (one 128-tap FIR
per ear, computed at conversion time, §9), which gives the tail the colour of a head in a room
without the direction any single HRIR would impose.

### 6.5 Acceptance for DE2

For three reference rooms fixed in the test (a 6 × 4 × 2.5 m unpainted-concrete garage with a
steel door, a 4 × 3.5 × 2.6 m carpeted bedroom with gypsum walls and one window, and a
5 × 4 × 2.7 m wood-floored room with plaster walls), the node's impulse response is rendered, band-
filtered at the six octaves, backward-integrated (Schroeder), and its T20 extrapolated to RT60.
Each band's figure lands within 10 % of the Eyring prediction for that room. The decay curve's step
at t_c is under 2 dB. The normalized echo density (Abel and Huang, "A simple, robust measure of
reverberation echo density", 121st AES Convention, 2006) reaches 1 no later than 2 · t_mix. The
interaural cross-correlation of the tail, measured on the ear outputs, is above 0.8 in the 125 Hz
band and below 0.3 in the 4 kHz band. `LateReverb::process` passes the allocation trap.

## 7. DE4, part one: cabinet directivity

A 4x12 beams treble forward and radiates bass everywhere, and that is the physics that makes
walking around the cabinet change its tone. The model is four rigid circular pistons in an
infinite baffle, arranged as the 2 × 2 array of a real 4x12, in front of a closed box.

A piston of radius a radiates with the directivity D(θ) = 2·J₁(k·a·sin θ) / (k·a·sin θ), k = 2πf/c
(Kinsler, Frey, Coppens and Sanders, *Fundamentals of Acoustics*, 4th ed., Wiley 2000, §7.4). For a
12-inch driver the effective radius is about 0.125 m, so k·a = 1 at 437 Hz: below that the cone is
omnidirectional over the front hemisphere, above it the beam narrows, and by 2 kHz (k·a = 4.6) the
first null is at 30° off axis. The four drivers on 0.3 m centres add an array factor whose first
grating null appears at 1.1 kHz at 90°. Behind the baffle, the closed box radiates as a source
whose front-to-back ratio rises from 0 dB below the baffle-step frequency (about 115 / W Hz for a
baffle W metres wide, 150 Hz for a 0.75 m cabinet) to a 6 dB shelf above it, plus the piston's own
fall-off. `CabinetDirectivity` evaluates this closed form at the six band centres for a given
outgoing direction and returns a `BandResponse`; the piston model needs no data beyond three
numbers: the cone radius, the driver spacing and the baffle width, with the 4x12 defaults above and
a 1x12 and a 2x12 preset beside it.

Where the piston model is wrong, and knowingly: a real cone breaks up above 2 to 3 kHz, so its
radiating area shrinks with frequency and the real cabinet is less directional up high than the
rigid piston predicts. The model takes an effective radius a(f) = a / sqrt(1 + (f / f_b)²) with
f_b = 2.5 kHz as a documented approximation, flagged in the module README as such, and the
acceptance test (§7.1) checks the model against its own closed form, not against a real cabinet.
If the maintainer later measures his 4x12 at, say, eight angles, `CabinetDirectivity` gains a
second implementation behind the same six-band interface that interpolates measured data, and
nothing else changes. That is the only point at which this section would ever need data.

The cabinet IR the rig already convolves (`assets/ir/README.md`) is a close-microphone capture,
so it already contains one microphone at one position. The room treats that signal as the
cabinet's on-axis free-field response at 1 m, and the directivity is applied relative to on-axis,
so on-axis at 1 m in the room reproduces the IR's tone exactly and every other position departs
from it by the model. The alternative, removing the IR's microphone before adding the room's, would
need a free-field measurement the maintainer does not have.

### 7.1 Acceptance

The on-axis minus 60°-off-axis level at 2 kHz for the 4x12 preset equals the closed-form array
value within 0.5 dB, and the same difference at 125 Hz is under 1 dB; the directivity index per
band, integrated numerically over the sphere, is within 0.5 dB of the analytic piston value in the
bands where a rigid piston applies (below f_b).

## 8. DE4, part two: the microphone

The microphone is the second kind of listener. It has a position, an orientation, a polar pattern
and a distance, and the recorded guitar tone the maintainer knows comes from an SM57 at 2 to 10 cm
off a cone. At that distance the room is barely present in the microphone's signal; what the
microphone adds is its pattern's rejection of the reflections and its proximity effect on the
direct sound.

A first-order pattern is P(θ) = a + (1 − a)·cos θ: omni (a = 1), cardioid (a = 0.5), supercardioid
(a = 0.366), figure-eight (a = 0). The gradient term's sensitivity to a spherical wave at distance r
exceeds its plane-wave sensitivity by sqrt(1 + (c / (2π·f·r))²), which is the proximity effect: a
6 dB per octave rise below f_p = c / (2π·r) (Eargle, *The Microphone Book*, 2nd ed., Focal Press
2004, ch. 3). For r = 5 cm, f_p = 1.09 kHz and a cardioid gains 15.5 dB at 100 Hz over its
far-field response, the boost applying to the gradient half of the pattern and not to the
pressure half; at r = 30 cm, f_p = 182 Hz and the same cardioid gains 3.7 dB at 100 Hz. `MicrophoneModel`
evaluates pattern times proximity at the six bands for the direct path's r and θ, and the pattern
alone for each reflection (whose r is many times larger, so its proximity term is unity within
0.1 dB past 1 m); the result is one more `BandResponse` factor folded into each tap's single fitted
filter.

A named microphone's own frequency response (the SM57's presence rise, its 150 Hz roll-off) is a
published curve on its manufacturer's specification sheet. The model accepts an optional six-band
on-axis curve per microphone preset transcribed from that sheet and cited in the header; it does
not model the capsule. The first release ships the pattern set with flat curves and one named
preset, the SM57, from Shure's published specification; the maintainer adds others by transcribing
six numbers.

Only one listener renders at a time. The node has two output channels, and the ears need both;
rendering the microphone beside the ears would need a third channel and a second image set, and
`RigGraph` carries `kMaxChannels = 2`. So the node's `listener` parameter selects ears or
microphone, the microphone's mono signal fills both channels, and hearing both at once is out of
scope (§11) with the shape of the change that would allow it.

### 8.1 Acceptance

A cardioid at 5 cm on axis measures 15.5 ± 0.5 dB more at 100 Hz than the same capsule at 3 m; a
figure-eight at 90° rejects the direct path by at least 40 dB; the SM57 preset's six band values
equal the header's transcription.

## 9. DE5: the HRTF and the binaural path

### 9.1 Which data, and its licence

**SADIE II**, University of York, Neumann KU100 dummy-head subject, 48 kHz measurements. The
database is copyright University of York and released under the Apache License 2.0
(https://www.york.ac.uk/sadie-project/database.html), the same licence this repository carries, so
a converted copy can ship inside the tree without a second licence text. The KU100 set has 1550
measured directions on the sphere (https://www.york.ac.uk/sadie-project/database_old.html), and
the same project publishes Ambisonic binaural decoder filter sets derived from that head up to
fifth order in the ACN/SN3D convention (https://www.york.ac.uk/sadie-project/ambidec.html), which
is the convention `AmbisonicEncoder` adopts so those filters can be used directly if they meet the
decoder acceptance in §9.5; otherwise the conversion tool computes its own.

The alternative considered is MIT KEMAR (Gardner and Martin, MIT Media Lab Perceptual Computing
TR 280, 1994; https://sound.media.mit.edu/resources/KEMAR.html), copyright MIT 1994, free with no
restriction on use provided the authors are cited. Its 710 directions and 44.1 kHz sampling are
both worse than SADIE's for this use, and its citation clause is a small obligation the Apache
licence does not add. It stays as the fallback dataset if SADIE II is ever withdrawn, since the
conversion tool reads either.

### 9.2 Where the HRTF lives: core, module or host

This is a third-party dependency question and the answer separates code from data.

- **Code:** none. SADIE II is distributed as SOFA (HDF5) and as WAV. Reading SOFA needs an HDF5
  library, which no target in this repository may take except `apps/host_gui`, and even there it
  would be a large library for a one-time read. So the repository does not read SOFA at all. A
  developer-side Python script, `tools/dsp/convert_hrtf.py`, beside `tools/dsp/design_resampler.py`,
  reads the database once and writes the asset file below. Python is already the shape of every
  generator in `tools/dsp/`, and `python-sofa` or `netCDF4` is a developer dependency of the tool,
  never of the library. The module's `HrtfSetLoader` reads the asset with `std::ifstream`, and the
  module keeps `sushi_dsp` as its only dependency.
- **Data:** an asset, in `assets/hrtf/`, loaded by the host and installed into the node through
  `IRoomHost::set_hrtf(HrtfSet)`, exactly as `assets/ir/default_4x12.wav` is loaded by
  `apps/host_gui` and installed through `ICabinetHost::set_cabinet`. The host resolves the path
  beside its executable; the build copies the directory the way `apps/host_gui/CMakeLists.txt`
  copies `assets/ir`.
- **Fallback, so the module builds and tests with no file:** `SphericalHeadHrtf` constructs an
  `HrtfSet` from the Duda and Martens spherical-head model ("Range dependence of the response of a
  spherical head model", JASA 104(5), 1998) with Woodworth's ITD, a = 8.75 cm. It has correct ITD
  and ILD and no pinna cues, so a cabinet placed behind the head sounds in front of it. The node
  runs on it until the host installs a measured set, and the ITD acceptance (§9.5) is measured
  against it because its answer is known in closed form.

The maintainer settled the shipping question on 2026-09-08: the converted set ships. The tree
carries `assets/hrtf/sadie2_ku100_48k.hrtf`, about 2 MB, and the converter that wrote it lives at
`tools/dsp/convert_hrtf.py` beside `tools/dsp/design_resampler.py`. The IR precedent stays
drop-in because no 4x12 capture the project can redistribute has been chosen; SADIE II is Apache
2.0, the licence this repository already carries, so nothing about it needs a second licence file.
Reading SOFA inside `apps/host_gui` would have put HDF5 on the host's link line for a job done
once per dataset, and that is why the reader is Python and the shipped artefact is a flat file.

`assets/hrtf/README.md` records the licence and the citation, the way `assets/ir/README.md` asks a
contributor to record a cabinet capture's source: the database name and version, the Apache 2.0
licence with the University of York copyright line, the URL the set came from, the SHA-256 of the
converted file, the converter's version and the flags it ran with, and the citation the SADIE
project asks for — Armstrong, Thresh, Murphy and Kearney, "A Perceptual Evaluation of Individual
and Non-Individual HRTFs: A Case Study of the SADIE II Database", *Applied Sciences* 8(11), 2018.
`tools/dsp/convert_hrtf.py` writes that block itself, copying the licence and citation text out of
the downloaded database's own documentation, so the shipped wording is York's rather than a
transcription.

**With the file absent** the module builds, the tests run and the host makes a sound: the loader
returns no set, `RoomNode` keeps the `SphericalHeadHrtf` its constructor built, and the room is
rendered with correct ITD and ILD and no pinna cues, so front and back are not distinguishable.
The user is told this twice and never has to guess. The host prints one line to stderr naming the
path it looked for, as it already does for a missing cabinet IR (`assets/ir/README.md`, "Not
present?"), and the room panel of DE6 shows the source as a word beside the room —
`HrtfSource::Measured` with the file's name, or `HrtfSource::SphericalHead` with the sentence
"front and back sound alike". `IRoomHost::hrtf_source()` is what the panel reads, so the state is
a value the acceptance test can assert and not a log line.

### 9.3 Storage and interpolation

The asset is a flat little-endian file with a fixed header (`magic`, version, sample rate,
direction count, tap count, decoder order), then per direction: azimuth, elevation, the ITD in
samples (a `float`), and a 128-tap minimum-phase HRIR per ear. Minimum-phase plus a separate ITD
(Kistler and Wightman, JASA 91(3), 1992) is what makes interpolation between directions clean: two
minimum-phase HRIRs blend without comb filtering, and the ITD is interpolated as a number. The
conversion tool computes the minimum-phase part by cepstral folding, measures each direction's ITD
by the onset of the raw pair, and truncates to 128 taps (2.7 ms at 48 kHz; the KU100's pinna
response is under 1 ms long and the truncation error measured by the tool is reported and must be
under −50 dB). 1550 directions × 2 ears × 128 taps × 4 bytes = 1.6 MB, plus the 32 decoder filters
of 256 taps, 32 kB, plus the two diffuse-field average filters.

Only the direct sound needs a direction the grid does not hold, since the reflections travel
through the Ambisonic path. The direct path's HRIR pair is built on the control thread as part of
`RoomRenderSet`: `HrtfSet` carries a Delaunay-style neighbour table computed at load, the solver
finds the spherical triangle containing the direction and blends the three minimum-phase pairs by
barycentric weight, and the ITD is blended the same way and applied as a fractional delay on the
far ear. A moved listener yields a new pair inside a new set and goes through the §5.2 crossfade
like every other change. The audio thread interpolates nothing.

### 9.4 The decoder, and the seam between early and late

`BinauralDecoder` holds 32 `PartitionedConvolution` instances at the partition
`StreamingConvolution::partition_size_for` picks for the rate (128 frames at 48 kHz), one per
Ambisonic channel per ear, each 256 taps. Its cost is fixed by the order and independent of the
scene: 32 forward transforms of 256 points, 32 × 2 partition products, 2 inverse transforms per 128
frames. Each `PartitionedConvolution::process` transforms its own input, so the sixteen channels
are each transformed twice; sharing those spectra across the two ears is a later optimisation inside
the decoder that changes no interface, and the budget (§10) counts the unshared figure. The KU100's
left-right symmetry halves the filter count for a head that is symmetric, which the conversion
tool checks and records; the decoder uses it when the flag is set.

The decoder filters are the magnitude-least-squares design (Schörkhuber, Zaunschirm and Höldrich,
"Binaural rendering of Ambisonic signals via magnitude least squares", DAGA 2018), computed by the
conversion tool from the measured set at third order, or taken from SADIE's own published set when
those pass the same acceptance. Magnitude least squares is chosen over a plain least-squares
decoder because the latter's high-frequency comb artefacts at third order are the known defect of
low-order binaural Ambisonics and the former removes them by relaxing phase above 2 kHz, where the
ear reads level, not phase.

The three binaural paths (exact HRIR direct sound, Ambisonic early reflections, coherence-shaped
tail through the diffuse-field average) meet in the sum. Their consistency is what keeps the
seam inaudible: the tail's diffuse-field filter is the power average of the same HRTF set the
decoder was designed from, so a reflection crossing from the early set to the tail changes from a
directional colour to the mean of all directional colours and not to a different head. The direct
path and the decoder carry the same 128-frame partition latency, and the early taps and tail are
delayed to match, so the node's `latency_samples()` reports one partition plus the direct path's
propagation delay when the `direct_delay` parameter is `physical`, and one partition alone when it
is `compensated`. **`compensated` is the default**, because the maintainer plays live through this
node: `physical` adds the room's 5.8 ms at 2 m on top of the device's own buffer latency, and a
player in a real room also hears his strings acoustically at zero delay, which headphones take
away. `physical` is the honest choice for rendering a take rather than playing one, and the
parameter is there for it.

### 9.5 Acceptance for DE5

On `SphericalHeadHrtf`, the ITD of the node's direct path, measured as the lag of the interaural
cross-correlation peak at 30°, 60° and 90° azimuth, equals Woodworth's (a / c)·(θ + sin θ), that is
0.261, 0.488 and 0.656 ms for a = 8.75 cm and c = 343 m/s, within 20 µs (one sample at 48 kHz).
The draft printed 0.479 ms at 60°; (a / c)·(θ + sin θ) with θ = π/3 is 0.488 ms, and the corrected
figure is the one the test asserts. On the converted
SADIE set, the same measurement matches the ITD the tool recorded for the nearest grid direction
within 50 µs. The converted file round-trips: the tool's writer and the loader agree on every field
in a fixture of four directions. The decoder's magnitude response to a single plane wave from each
of 24 test directions matches the measured HRTF's magnitude within 3 dB in third-octave bands from
200 Hz to 8 kHz (the magnitude-least-squares bound at third order in the cited paper). Front-back
discrimination is a listening judgement, which is not a test; the objective proxy asserted is that
the rendered direct sound for a source at 180° azimuth carries the KU100's rear spectral notch
between 6 and 10 kHz at least 6 dB deep.

## 9a. The wave layer, the max mode, and what the bass needs

### 9a.1 What was asked for and what it is

The maintainer asked for "max ray tracing", and his reason was the low end: he wants to feel the
bass, and hear everything the headphones can reproduce. This section designs the reason and not
the name, and says so here so that nobody later reads "max" as "more rays".

Geometric acoustics, whether image sources or rays, treats sound as travelling along lines that
reflect off surfaces. That is true only while the wavelength is small against the surfaces. In a
garage of 6 × 4 × 2.5 m with RT60 near 0.5 s the Schroeder frequency f_s = 2000·sqrt(RT60 / V) is
about 180 Hz, and below it the room's response is a handful of standing waves whose pressure peaks
in the corners and nulls at the mid-points. The 6 m dimension resonates at c / 2L = 28.6 Hz and its
harmonics; the 4 m at 42.9 Hz; the 2.5 m at 68.6 Hz; the first tangential modes fall between. A
cabinet pushed against a wall drives every mode whose pressure is high at that wall, which is all of
them, and that is the swell a player hears when the amp goes into the corner. No number of rays
converges on this: a ray carries energy, not phase, and modes are a phase phenomenon. Above f_s,
rays are excellent. So the engine is three layers and a seam:

1. **A wave solver below f_s**, the modal decomposition of §9a.2. This is the layer that answers
   the request.
2. **Geometric acoustics above f_s**: the image-source early set of §4, and a late field that is
   the FDN of §6 in the hybrid mode or the stochastic ray tracer of §9a.3 in the max mode.
3. **A crossover at f_s** computed from the configured room, §9a.4.

Layers 1 and 3 run in both quality modes, on the maintainer's ruling of 2026-09-08. The modal
bank costs 0.32 ms of a 5.33 ms block, six per cent of the hybrid's budget (§10), and it is the
one brick that produces what he asked for. Locking it behind the mode that also costs a 1.9 Mflop
convolution would have priced the best part of the engine out of the mode he plays in. What
`quality` selects is therefore the late field above f_s and nothing else: `hybrid` runs the FDN,
`max` runs the ray tail. DE3 builds the wave layer; DE8 builds the ray tail.

### 9a.2 The wave solver: modes, not a grid

Two candidates. A finite-difference time-domain (FDTD) grid solves the wave equation on cells and
handles any geometry; a modal decomposition uses the closed-form eigenfunctions of the rectangular
room and handles only a shoebox. The room is a shoebox by §4.1 and §11, so the question is cost and
accuracy alone.

**FDTD arithmetic.** To hold numerical dispersion under 1 % with the standard leapfrog scheme the
grid needs about ten cells per wavelength at the highest frequency solved. For f_s = 250 Hz (a live
garage), λ = 1.37 m and h = 0.137 m, so the 60 m³ room is 44 × 29 × 18 = 23 000 cells. The Courant
limit sets the time step at h / (c·sqrt 3) = 0.23 ms, an update rate of 4.3 kHz, and each cell
update is about ten flops: 23 000 × 10 × 4300 = 1.0 Gflop/s, which is 5.3 Mflop per 256-frame block
and about 3.5 ms of a scalar core, most of the budget, for a layer that carries a tenth of the
bandwidth. It also needs frequency-dependent boundary impedances as per-cell boundary filters to
represent the materials table, which is where FDTD room codes spend their research effort, and it
would buy a geometric freedom this design has already refused.

**Modal arithmetic.** For a rigid rectangular room the eigenfunctions are
ψ(x, y, z) = cos(n_x·π·x / L_x)·cos(n_y·π·y / L_y)·cos(n_z·π·z / L_z) with eigenfrequencies
f = (c / 2)·sqrt((n_x / L_x)² + (n_y / L_y)² + (n_z / L_z)²) (Kuttruff, *Room Acoustics*, 5th ed.,
Spon Press 2009, ch. 3; Morse and Ingard, *Theoretical Acoustics*, ch. 9). The number of modes
below f is

    N(f) ≈ (4π/3)·V·(f/c)³ + (π/4)·S·(f/c)² + (L/8)·(f/c),   L = 4·(L_x + L_y + L_z)

which for the 60 m³ garage (S = 98 m², L = 50 m) is 61 modes below 180 Hz and 143 below 250 Hz.
Each mode is one second-order resonator, a `Biquad` at the mode's frequency with a bandwidth set by
its damping, fed by the input times ψ_n(source) and read into each ear times ψ_n(ear). Per mode
per sample: one biquad (9 flops) and two output multiply-adds (4 flops). At full rate,
143 × 13 × 48 000 = 89 Mflop/s, 0.48 Mflop per block, about 0.3 ms of a scalar core. Two hundred
times cheaper than the grid, exact in the mode shapes, and with the two ears evaluated as two
points 17 cm apart in the standing-wave field, which is the physically right interaural cue at
these frequencies and comes free. Modal decomposition wins, and FDTD is not built.

Damping is the one place the modal picture is approximate. The eigenfunctions above assume rigid
walls; real walls absorb, which shifts the modes slightly and gives each one a decay. For the small
absorption a hard room has at low frequency (α ≤ 0.1 for concrete, brick and glass in the 125 Hz
band) the first-order perturbation is the standard treatment (Kuttruff ch. 3, Morse and Ingard
§9.4): each mode's damping constant is the sum over the six surfaces of the surface's absorption
weighted by the mode's mean-square pressure at that surface, so an axial mode between two walls is
damped by those two and grazes the other four. `ModalSolver` computes exactly that from the
materials table's 125 Hz and 250 Hz rows, interpolated in log frequency to each mode, and the 60 dB
decay of each mode follows. For a soft wall at low frequency, gypsum on studs at α = 0.29 in the
125 Hz band being the one common case, the perturbation is a 30 % error in that mode's decay, not
in its frequency; the module README carries the number.

The layer's geometry dependence is entirely in gains. A moved cabinet or listener changes ψ_n at
two points and nothing else: no resonator changes frequency, so a scene change publishes new gain
vectors and the §5.2 crossfade covers them. Only a changed dimension or material changes the
resonators, and then the bank is rebuilt like every other part of the set.

The cabinet itself bounds what this layer can show. A closed 4x12's response falls below 70 to
100 Hz and the cabinet IR the rig convolves carries that roll-off, so the 28.6 Hz and 42.9 Hz axial
modes of the garage are driven at 10 to 20 dB below the modes between 60 and 180 Hz. The swell the
maintainer will hear when the cabinet meets the wall is in that upper range, and it is real.

### 9a.3 Rays above the Schroeder frequency

Above f_s the early field is the image set of §4, unchanged: for a shoebox the image-source
method is the exact specular solution and rays add nothing to it. What rays add is the late field.
Eyring gives one decay time per band for the whole room; a garage with a bare concrete floor and an
absorbent ceiling of rockwool has a decay that is not one exponential but two, steep while the
energy is still bouncing floor to ceiling and slow once it has settled into the horizontal plane
between hard walls, and the ear hears the difference as the room's "shape". A stochastic ray
tracer (Krokstad, Strøm and Sørsdal, JSV 8(1), 1968; Vorländer ch. 11) with per-band absorption,
per-material scattering (the same s of §4.2, now a real Lambert branch instead of an energy hand-
over) and a spherical receiver at the listener records exactly that: an energy histogram per
band, per time slot and per arrival direction.

Its cost is a control-thread cost. 20 000 rays, each followed for 50 reflections or until 60 dB
down, each reflection testing six planes at about ten flops each, is 60 Mflop, about 40 ms, per
configuration. `LateIrSynthesizer` then turns the histogram into a binaural late impulse response
from t_c onward: Poisson-distributed echoes at the histogram's density, each with the slot's band
energy and a direction, encoded to Ambisonics and decoded to two ears with the same §9.4 decoder,
offline, so the output is two IRs of the tail's length. For a 1.2 s garage that is 57 600 taps per
ear. The real-time part is two `PartitionedConvolution` instances at the 128-frame partition: 450
partitions × 129 bins × 8 flops per 128 frames per ear, 0.93 Mflop per 256-frame block for both
ears, about 0.35 ms on the multiply-accumulate side of the scalar estimate, plus the transforms.
The FDN is switched off in max mode; the ray tail replaces it and the two do not run together.

That is the truth about real time for the max mode: the tail renders in real time, but a scene
change takes about 50 ms of control-thread work (rays, synthesis, decoder) before the new tail can
be published, and its convolver crossfade costs a second convolver for two blocks. So while a
slider drags at 30 Hz, the early set and the modal gains follow at 30 Hz and the ray tail follows
when its computation lands, which the host may show. The maintainer moves things and hears the
early reflections and the bass move with them at once; the late decay's shape catches up within a
tenth of a second. If the ray count is raised for a smoother histogram, that lag grows in
proportion and nothing on the audio thread changes.

The mixing matrix, the line lengths and Σ τ_i of §6 do not apply here; the ray tail's density and
decay are the histogram's. What the ray layer does not change is item 1 of §2, the early
reflections, and it is stated plainly that of the three layers this is the one whose contribution
to "his own garage" is smallest: a double-slope decay and a directional late field are audible on a
sustained chord and inaudible on a palm mute.

### 9a.4 The crossover, from the room and not from a constant

f_s is computed per scene as 2000·sqrt(RT60_mid / V), with RT60_mid the Eyring value of §6.3 at
500 Hz. Three cases:

- **The usual room**, f_s between 60 and 400 Hz: the modal layer carries everything below f_s, the
  geometric layers everything above, through a fourth-order Linkwitz-Riley pair whose two halves
  sum to unit magnitude and are in phase at every frequency, so the seam adds no dip and no hump.
  The modal layer's steady-state energy in the octave below f_s is scaled once per scene to the
  diffuse-field energy density Eyring predicts for the same octave, so the two physical models of
  the same room agree in level where they meet; the acceptance test (§9a.6) measures the band
  energy of the rendered response on both sides of f_s and refuses a discontinuity over 2 dB.
- **A room so large or so dead that f_s falls under 40 Hz** (a 300 m³ hall at 0.3 s gives 63 Hz; a
  dead 500 m³ studio at 0.25 s gives 45 Hz): the mode count is small, the layer is cheap, and when
  f_s drops below 40 Hz the modal layer is switched off and the geometric layers carry the whole
  band, because nothing below 40 Hz reaches the headphones anyway (§9a.5). The host shows the
  computed f_s beside the room.
- **A room so small and live that the count exceeds `kMaxModes`** (a 20 m³ tiled bathroom at
  1.5 s gives f_s = 548 Hz and about 440 modes): `kMaxModes` = 512 covers it. Past that, the
  solver lowers the crossover to the highest frequency whose mode count fits, reports the
  lowered figure through `IRoomHost`, and the geometric layer takes over at a modal overlap
  somewhat under three, which is a gradual loss of accuracy and never a failure.

### 9a.5 What the headphones can and cannot do

The engine's honest promise is correct reproduction down to the transducer's own limit and no
further. Closed-back headphones roll off below 20 to 30 Hz and many open-back designs earlier; the
28.6 Hz axial mode of a 6 m garage is at or under that limit and will be heard at reduced level or
not at all, whatever the model does. And part of what a player calls feeling the bass is not
heard: it is the chest and the floor at 100 dB SPL between 30 and 100 Hz, which headphones do not
produce and no acoustic model can. The modal layer delivers the pressure the room would put at his
two ears, at the right frequencies and the right relative levels, into a transducer that
reproduces from about 30 Hz up. That is what "feel the bass" can mean over headphones, and no
acceptance item in this document claims more. What he will hear is the 60 to 180 Hz region
swelling and shifting as the cabinet and his head move through the standing waves, which is the
audible half of the sensation.

### 9a.6 Acceptance for DE3 and DE8

**DE3, the wave layer, measured with `quality` at `hybrid` and again at `max`.** For the garage of
§6.5, the modal layer's rendered response has spectral peaks at the analytic eigenfrequencies below
f_s within 0.5 % (the first six: 28.6, 42.9, 51.5, 57.2, 68.6 and 71.5 Hz for 6 × 4 × 2.5 m), and
each peak's −3 dB width matches the perturbation damping within 20 %. The pressure at a corner
exceeds the pressure at the room's centre by at least 12 dB for the (1, 0, 0) mode, as the cosine
shape predicts. The band energy of the full response is continuous across f_s within 2 dB, and the
two renders differ by under 1 dB below f_s, because the same bank produced both. A moved cabinet
publishes new modal gains within one GUI frame. `ModalBank::process` and `CrossoverPair::process`
pass the allocation trap, and the modal and crossover rows of §10's hybrid table are measured by
`tests/benchmark`.

**DE8, the ray tail.** With the absorbent-ceiling garage, the ray tail's Schroeder curve shows two
slopes whose ratio matches the tracer's own histogram within 10 %, and its late-field arrival
directions, decoded back from the synthesised IR, weight the hard walls above the soft ceiling. A
scene change publishes the early set within one GUI frame and the ray tail within 100 ms, timed in
the test. The two convolvers pass the allocation trap. The steady-state and swap-peak rows of
§10's max-mode table are measured by `tests/benchmark`.

## 10. The CPU budget

At 48 kHz and 256 frames one block is 5.333 ms. The figures below are flop counts from the
sections above, converted at 1.5 Gflop/s for the loops whose recursion is dependent across samples
(the biquads, the FDN) and 3 Gflop/s for the transforms and the multiply-accumulate loops, on one
core of a desktop x86 in plain C++ with the Release flags `cmake/SushiDSPCompilerFlags.cmake`
sets. They are estimates to be replaced by `tests/benchmark` figures in DE1 through DE5, and the
acceptance in §12 is on the measured numbers.

| Brick | Work per block | Mflop | ms, scalar | Share of 5.33 ms |
| --- | --- | --- | --- | --- |
| Input ring and direct path: fractional delay, one fitted filter, 2 HRIR convolutions | 256 × 27 + 2 conv(256 taps) | 0.05 | 0.03 | 0.6 % |
| Early reflections, 128 taps × 43 flops | 256 × 128 × 43 | 1.41 | 0.94 | 17.6 % |
| Ambisonic binaural decode, 32 unshared FIRs, 2 × 128-frame partitions | 2 × 32 × (FFT₂₅₆ + 2 × 129 × 8 + share of IFFT) | 0.66 | 0.22 | 4.1 % |
| FDN, 16 lines: 16 × 18 (filters) + 64 (Hadamard) + 64 (in/out vectors) | 256 × 416 | 0.11 | 0.07 | 1.3 % |
| Tail binauralisation: 2 coherence filters, 2 diffuse-field FIRs of 128 taps | 256 × (36 + 256) | 0.07 | 0.03 | 0.6 % |
| Modal bank, 143 modes × 13 flops (garage; 512 modes at the cap is 3.6×) | 256 × 143 × 13 | 0.48 | 0.32 | 6.0 % |
| Crossover pair, LR4 on two ears and the modal input: 6 biquads | 256 × 54 | 0.01 | 0.01 | 0.2 % |
| Mix, smoothing, bookkeeping | 256 × 10 | 0.003 | 0.01 | 0.2 % |
| **Steady state** | | **2.79** | **1.63** | **31 %** |
| Swap block: early reflections rendered twice, for 512 frames | + 1.41 | 4.20 | 2.57 | 48 % |

The modal bank and its crossover are the two rows the maintainer's ruling of 2026-09-08 moved out
of the max mode and into this table. They add 0.33 ms, and they take the hybrid steady state from
1.30 ms to 1.63 ms and its swap peak from 2.24 ms to 2.57 ms; §12 item 10 carries the raised
ceilings.

The early renderer is 58 % of the steady cost and the whole of the swap peak. Its loop is
128 independent recurrences of the same shape, which is the textbook SIMD case: eight taps per
AVX2 register, sixteen groups, the Lagrange read gathered per lane, the Ambisonic accumulation as
sixteen fused multiply-adds per group. The expectation, to be measured and not assumed, is 5× to
6× on that brick, taking the steady state to about 0.85 ms and the swap peak to about 1.05 ms. The
decoder's transforms are the second candidate and the core's `Fft` would be where that work
lands, which is a core change and is not in this spec. Nothing else is worth vectorising.

The maintainer's order is maximum C++ engineering first. The engineering the plain version gets
before any intrinsic is written: the tap-outer loop with the 16 accumulators in registers,
coefficients laid out structure-of-arrays so the compiler's auto-vectoriser has a chance at the
Lagrange and encode steps, `double` state for the biquads as `Biquad` already does, the two-bank
state so a swap allocates nothing, and the shared input spectra in the decoder. DE7 opens only if
the DE1 benchmark, with the JCM800 chain running in the same graph, shows the room over its
2.0 ms ceiling.

The microphone listener is cheaper: no encode (28 flops per tap), no decoder, a mono tail, and a
modal bank reading one point instead of two (11 flops per mode, 0.27 ms). Steady state about
1.05 ms.

**Max mode (§9a)**, same conventions. The two late convolvers replace the FDN and its
binauralisation; everything else, the modal bank included, is the hybrid's.

| Brick | Work per block | Mflop | ms, scalar | Share of 5.33 ms |
| --- | --- | --- | --- | --- |
| Direct path, early reflections, Ambisonic decode (as above) | | 2.12 | 1.19 | 22.3 % |
| Modal bank, 143 modes × 13 flops (garage; 512 modes at the cap is 3.6×) | 256 × 143 × 13 | 0.48 | 0.32 | 6.0 % |
| Crossover pair, LR4 on two ears and the modal input: 6 biquads | 256 × 54 | 0.01 | 0.01 | 0.2 % |
| Ray tail, 2 × 57 600-tap convolutions, 450 partitions each | 2 × 2 × (FFT₂₅₆ + 450 × 129 × 8 + IFFT) | 1.90 | 0.65 | 12.2 % |
| **Steady state** | | **4.51** | **2.17** | **41 %** |
| Swap block, early set and modal gains only (a moved cabinet or head) | + 1.41 | 5.92 | 3.11 | 58 % |
| Swap block, ray tail also replaced (a changed dimension or material) | + 1.41 + 1.90 | 7.82 | 3.76 | 71 % |

Beside a JCM800 chain the max mode's steady state fits and its worst swap block does not leave
comfortable room; the swap that replaces the tail happens once per material or dimension edit and
never while a position drags, so it is a click-free peak on a rare event rather than a sustained
load. Uniform partitioning is what makes the tail's convolution 1.9 Mflop; a non-uniform scheme
would cut it by three or four but is a core change to `PartitionedConvolution` and not in this
spec. If the measured max-mode figure with the amplifier chain exceeds 3.5 ms steady, the first
remedy is the shared-spectra decoder of §9.4 and the second is DE7.

## 11. Out of scope, stated and defended

- **Any shape but a shoebox, and any furniture.** An L-shaped garage or a bed in the room changes
  the individual paths past second order and changes the statistics of the tail not at all beyond
  what its surface area and absorption already say. The ear resolves the first handful of
  reflections and then hears a density; the shoebox gets the handful right in timing and the
  density right through Eyring. Supporting arbitrary polygons would replace the closed-form image
  solver with a visibility-tested one at ten to a hundred times the control-thread cost for a
  difference that shows in a plot and not in headphones. A large object (a car in the garage) is
  best expressed as absorption and scattering on the surface it stands against, which the
  materials table already allows.
- **Edge diffraction and occlusion.** Nothing stands between the cabinet and the listener.
- **The phase of a wall's reflection factor.** Real walls are not phase-flat at low frequency, but
  the effect is a shift of the room's modal frequencies by a few percent, below what the modal
  layer's own perturbation damping (§9a.2) already loses.
- **Exact modal damping at a soft low-frequency boundary.** `ModalSolver` damps each mode by the
  first-order perturbation of §9a.2, which is right to a few percent for concrete, brick and glass
  (α ≤ 0.1 at 125 Hz) and about 30 % wrong in the decay of a mode bounded by gypsum on studs
  (α = 0.29 at 125 Hz). The frequency is right in both cases. Solving the damped eigenproblem
  against a complex wall impedance costs what §9a.2 measures FDTD at, for a decay error inside a
  layer the hybrid mode did not have at all before 2026-09-08.
- **The FDN's own modes below f_s.** They are crossed out by the Linkwitz-Riley pair of §9a.4 in
  both quality modes, so the FDN carries no low end and its line lengths are designed for the
  band above f_s alone.
- **FDTD, or any grid solver.** §9a.2 shows it at two hundred times the cost of the modal bank for
  a geometric freedom this design does not use.
- **Rays for the early field, and rays below the Schroeder frequency.** The image-source method is
  the exact specular solution for a shoebox, and rays carry no phase; §9a.1 says why "max ray
  tracing" is not what the bass request needs.
- **Tactile bass.** §9a.5: the chest-felt part of a loud low end is not sound at the ears and no
  headphone model produces it.
- **Doppler on a moving source or listener**, per §5.2.
- **Head tracking.** No sensor is attached. The listener's yaw is a scene parameter, so a key bound
  to it in the host would already turn the head at the 30 Hz publish rate; a sensor is a host
  concern for a later spec.
- **Individualised or near-field HRTFs.** SADIE's KU100 is measured at 1.2 m; a cabinet 40 cm from
  the head would need the near-field set the database does not have. The 1/r gain and the far-field
  set are used at every distance, and the error under 1 m is an ILD a few dB too small.
- **Two listeners at once, or two sources.** Both need more than two channels or two image sets
  (§8). A second cabinet is a second node instance in a parallel branch, each with its own room,
  until a 2-in room node is asked for.
- **The guitarist's own body, and the cabinet's box resonances.** No published data at hand for
  either; both are colourations the cabinet IR partly contains already.
- **Temperature and humidity.** Fixed at 20 °C and 50 % RH inside `AirAbsorption`; the difference
  between a summer and a winter garage is under 1 dB at 4 kHz over a one-second tail.

## 12. Acceptance

Every item is a test in `tests/functional/unit/` or a figure from `tests/benchmark`, run by the
maintainer through `sd test`; none is a listening judgement.

1. **Geometry.** Image counts 7, 25, 63 and 129 for N = 1 to 4; first-order arrival times in the
   rendered omni response within half a sample of |**p**_i − **r**| / c; no tap past t_c (§4.3, §5.3).
2. **Filters.** Every materials row raised to powers 1 to 4 fits within 1 dB at the six centres
   (§4.2).
3. **Decay.** Six-band RT60 from T20 within 10 % of Eyring for the three reference rooms; decay-
   curve step at t_c under 2 dB; normalized echo density at 1 by 2 · t_mix (§6.5).
4. **Tail coherence.** IACC above 0.8 at 125 Hz and below 0.3 at 4 kHz (§6.5).
5. **Directivity.** 4x12 on-axis minus 60° at 2 kHz within 0.5 dB of the closed form; under 1 dB
   difference at 125 Hz (§7.1).
6. **Microphone.** Cardioid proximity 15.5 ± 0.5 dB at 100 Hz for 5 cm against 3 m; figure-eight
   rejection at 90° of at least 40 dB (§8.1).
7. **Binaural.** ITD 0.261, 0.488 and 0.656 ms within 20 µs at 30°, 60° and 90° on the spherical
   head; within 50 µs of the recorded value on SADIE; decoder magnitude within 3 dB in third-
   octaves 200 Hz to 8 kHz; rear notch at least 6 dB deep at 180° (§9.5).
8. **Swap.** A 1 kHz tone through a publish leaves a residual against the reference crossfade under
   −60 dB; the allocation trap passes with a publish between blocks (§5.3).
9. **RT safety.** `RoomNode::process`, `EarlyReflectionRenderer::process`, `LateReverb::process`
   and `BinauralDecoder::process` pass the `operator new` trap; `RtSwapSlot::reclaim()` frees a
   retired set after the audio thread acquires the next.
10. **Budget.** `tests/benchmark` at 48 kHz and 256 frames: hybrid steady state at or under 1.8 ms
    scalar and swap block at or under 2.8 ms, both raised from 1.5 and 2.5 by the 0.33 ms the modal
    layer brought into the mode; max-mode steady state at or under 2.5 ms and its tail-replacing
    swap at or under 4.0 ms; with the JCM800 chain in the same graph the room's share is at or
    under 2.0 ms, or DE7 opens (§10).
11. **Latency.** A freshly constructed `RoomNode` reports `direct_delay` as `compensated`, and
    `latency_samples()` equals one partition there and one partition plus round(d / c · fs) after
    the parameter is set to `physical`; at d = 2 m, 48 kHz, c = 343 m/s that difference is 280
    samples, and the rendered direct-path onset moves by the same 280 (§9.4).
12. **The wave layer, in both quality modes.** Eigenfrequency peaks within 0.5 % of the analytic
    values and mode widths within 20 % of the perturbation damping; corner-to-centre pressure
    difference of at least 12 dB for the (1, 0, 0) mode; band energy continuous across f_s within
    2 dB; the `hybrid` and `max` renders of the same scene within 1 dB of each other below f_s;
    modal gains published within one GUI frame; the modal and crossover budget rows measured
    (§9a.6, §10).
13. **The ray tail.** The double-slope garage's slope ratio within 10 % of the tracer's histogram;
    the early set published within one GUI frame and the tail within 100 ms; the max-mode budget
    rows measured (§9a.6, §10).
14. **The HRTF asset and its absence.** The loader reads `assets/hrtf/sadie2_ku100_48k.hrtf` and
    reproduces every field of the converter's four-direction fixture; with the file renamed away
    the node still renders, `IRoomHost::hrtf_source()` returns `HrtfSource::SphericalHead`, the
    ITD acceptance of item 7 passes on the fallback, and the host prints one stderr line naming
    the path it looked for; with the file present `hrtf_source()` returns `HrtfSource::Measured`
    (§9.2).

## 13. Judgements made here for the maintainer to overrule

1. **Prefix DE.** Any other free pair works; `DE-200` is a Roland delay named once in
   `modules/dd3/README.md`, which is not a roadmap collision.
2. **Order 4 and 128 taps, truncated by time.** Order 5 would cost 1.8× for rooms he does not have.
3. **Third-order Ambisonics for the reflections, exact HRIR for the direct sound.** Second order is
   cheaper and blurrier; per-tap HRIRs are six times the cost.
4. **Crossfade over 512 frames on every scene change, no Doppler.**
5. **Sixteen FDN lines, Hadamard, lengths from Σ τ_i ≥ 1.4 · RT60.**
6. **Eyring, not Sabine.**
7. **Scattering as one coefficient per material chosen from three words.** Could be dropped for
   simplicity or made numeric.
8. **The piston-array directivity with a breakup-shrinking radius at 2.5 kHz**, an approximation
   named as such, no measured data required.
9. **The cabinet IR is treated as the on-axis 1 m response.** The alternative needs a free-field
   measurement.
10. **One listener at a time; the microphone fills both channels.**
11. **SM57 as the one named microphone preset, from Shure's published curve.**
12. **SADIE II KU100 as the dataset, converted by a Python tool, shipped as a 2 MB asset in
    `assets/hrtf/`.** Settled 2026-09-08: it ships. `assets/hrtf/README.md` carries the Apache 2.0
    licence, the York copyright line and the citation the project asks for, and the module falls
    back to `SphericalHeadHrtf` when the file is missing (§9.2).
13. **Magnitude-least-squares decoder computed by the tool, SADIE's own set accepted if it passes.**
14. **`direct_delay` defaults to `compensated`.** Settled 2026-09-08: the maintainer plays live
    through this node and the 5.8 ms at 2 m stacks on the device's buffer, so `physical` is the
    option and `compensated` is the default (§9.4).
15. **Geometry enters through `IRoomHost` in the module, not through the `NodeDescriptor`
    parameter table.** A scene is twenty-odd numbers whose change triggers a control-thread solve;
    pushing them through the RT parameter ring one float at a time would trigger twenty solves per
    edit and give the host no typed API. `IRoomHost` mirrors `ICabinetHost` but lives in
    `modules/room/include/SushiDSP/room/`, because the core should not name a room. The parameter
    table carries only what the audio thread can apply itself: wet/dry, output level, listener
    selection and `direct_delay`.
16. **The modal solver runs in both quality modes.** Settled 2026-09-08: it costs 0.32 ms, six
    per cent of the hybrid's block, and it is the layer that answers the bass request, so `quality`
    now selects the late field above f_s and nothing else (§9a.1, §10, §14).
17. **`BandResponseFit` stays in the module until a second module wants it.**
18. **Modes, not FDTD, for the wave layer**, on the §9a.2 arithmetic.
19. **The ray tail is a precomputed binaural IR through two uniform-partition convolvers**, lagging
    a scene change by up to 100 ms while the early set and the modal gains follow at once. The
    alternative, driving the FDN from the ray histogram, keeps everything at 30 Hz and loses the
    double-slope decay the rays were computed for.
20. **The Schroeder frequency comes from Eyring at 500 Hz**, and the modal layer switches off
    under 40 Hz.
21. **20 000 rays and 50 orders** as the tracer's defaults; both are control-thread knobs and the
    audio thread never sees them.

## 14. Phases, in dependency order

| Phase | Builds | Waits on | Acceptance |
| --- | --- | --- | --- |
| DE0 | `RoomScene`, `Materials`, `BandResponse`, `BandResponseFit`, `AirAbsorption`, `ImageSourceSolver`, `EarlyTapSet` | nothing | §4.3 |
| DE1 | `EarlyReflectionRenderer`, `RoomRenderSet`, the `RtSwapSlot` publish and crossfade, `RoomNode` with the omni microphone listener, registered as `sushidsp.room` under a new `NodeCategory{ "Room" }` | DE0 | §5.3, items 8 to 10 of §12 |
| DE2 | `LateReverbDesign`, `LateReverb`, the crossover at t_c, mono and two-ear tail outputs | DE0; parallel with DE1 | §6.5 |
| DE3 | `ModalSolver`, `ModalBank`, `CrossoverPair`, f_s from the scene, the level match at the seam, the modal layer running in both quality modes | DE2 | §9a.6, the DE3 paragraph; item 12 of §12 |
| DE4 | `CabinetDirectivity`, `MicrophoneModel`, the `listener` parameter and the SM57 preset | DE1 | §7.1, §8.1 |
| DE5 | `HrtfSet`, `HrtfSetLoader`, `SphericalHeadHrtf`, `AmbisonicEncoder`, `BinauralDecoder`, the direct-path HRIR, tail coherence and diffuse-field filters, `direct_delay` defaulting to `compensated`, `tools/dsp/convert_hrtf.py`, `assets/hrtf/sadie2_ku100_48k.hrtf` and its `README.md`, `IRoomHost::set_hrtf` and `IRoomHost::hrtf_source` | DE1, DE2 | §9.5, items 11 and 14 of §12 |
| DE6 | Host: a top-down room panel in `apps/host_gui/ui/` (drag the cabinet, the head and the microphone; a material per surface; the HRTF path and the source word), `IRoomHost` calls from `RigController`, the scene in the preset | DE4, DE5 | the preset round-trips a scene; a drag publishes at most 30 sets per second |
| DE7 | AVX2 lanes in `EarlyReflectionRenderer` only, behind the existing `SUSHIDSP_ENABLE_AVX2` gate, with the scalar loop kept as the reference the test compares against | the DE1 benchmark showing the room over 2.0 ms with the JCM800 chain | bit-for-bit within 1e-6 of the scalar loop; item 10 of §12 |
| DE8 | `max/RayTracer`, `max/LateIrSynthesizer`, the `quality` parameter (`hybrid`, `max`), the two late convolvers in place of the FDN in max mode, the lagging tail publish and its `IRoomHost` progress report | DE3, DE5 | §9a.6, the DE8 paragraph; item 13 of §12 |

DE1 and DE2 share `BandResponseFit` and nothing else, so two agents can build them at once with
disjoint files. DE3 waits only on DE2, because the Schroeder frequency comes from the Eyring
figure `LateReverbDesign` computes; it can be built beside DE4. DE8 waits on DE3 for the crossover
and the level match at the seam, which are what the ray tail is measured against, and on DE5 for
the decoder that turns its histogram into two ears. Documentation lands with each phase:
`modules/room/README.md` from DE1, `assets/hrtf/README.md` from DE5, a §9 roadmap row and a
§5-style "As built" section in `docs/design/sushidsp.md` when DE5 closes, and a changelog line per
landed change.
