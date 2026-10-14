# tests/foundation/circuits/models

## `test_pentode_el34.cpp`

### `PentodeEL34MatchesKorenAtThreeOperatingPoints`

`// The three points are hand-computed from Koren's law.`

mu = 11, kp = 60, kvb = 24, kg1 = 650, kg2 = 4500, kx = 1.35.

(1) Vpk = 400, Vg2k = 400, Vg1k = -37  (a JCM800 idle point at -37 V fixed bias)

```text
u_arg = 60*(1/11 - 37/400) = -0.0954545454545452
s = ln(1+exp(u_arg)) = 0.646558421976595,  E1 = 400*s/60 = 4.31038947984397
knee = atan(400/24) = 1.51086817167369
Ip  = 2 * E1^1.35 / 650 * knee = 3.34149261300818e-2 A   (33.4 mA idle)
dIp/dVg1k = 4.98316907313410e-3 S,  dIp/dVg2k = 5.73718514953930e-4 S,
dIp/dVpk  = 3.30555615548759e-6 S   (a ~300 kOhm plate resistance: pentode, as expected)
z = 400/11 - 37 = -0.636363636363633 < 0, so Ig2 = 0 and its whole row is 0 - the screen
draws nothing at idle, which is what the max(0, .) clamp is for.
```

(2) Vpk = 250, Vg2k = 300, Vg1k = -20

```text
u_arg = 60*(1/11 - 20/300) = 1.45454545454545,  s = 1.66440636412608
E1 = 300*s/60 = 8.32203182063039,  knee = atan(250/24) = 1.47508961870809
Ip = 7.92947981392609e-2 A,  dIp/dVg1k = 1.04281593600714e-2 S,
dIp/dVg2k = 1.05203721563143e-3 S, dIp/dVpk = 2.04537709631731e-5 S
z = 300/11 - 20 = 7.27272727272727,  Ig2 = z^1.35 / 4500 = 3.23652408855726e-3 A
dIg2/dVg1k = 1.35 * z^0.35 / 4500 = 6.00779783938442e-4 S
dIg2/dVg2k = 6.00779783938442e-4 / 11 = 5.46163439944038e-5 S
```

(3) Vpk = 100, Vg2k = 400, Vg1k = -5   (hard into conduction, plate pulled down)

```text
u_arg = 60*(1/11 - 5/400) = 4.70454545454546,  s = 4.71355874131221
E1 = 400*s/60 = 31.4237249420814,  knee = atan(100/24) = 1.33525134607403
Ip = 4.31496731894557e-1 A,  dIp/dVg1k = 1.83712705886168e-2 S,
dIp/dVg2k = 1.68594235250184e-3 S,  dIp/dVpk = 7.33338162266083e-4 S
z = 400/11 - 5 = 31.3636363636364,  Ig2 = 2.32789367120296e-2 A,
dIg2/dVg1k = 1.00200640630040e-3 S,  dIg2/dVg2k = 9.10914914818548e-5 S
```

### `PentodeEL34GridCurrentFollowsNormansSoftLaw`

`// Ig1 = 1e-6 * ln(1 + exp((Vg1k - 0.5)/0.15)).`

Ig1 = 1e-6 \* ln(1 + exp((Vg1k - 0.5)/0.15)), dIg1/dVg1k = (1e-6/0.15) \* logistic(same).

```text
Vg1k = -1.5 : argument = -13.3333333333333, ln(1+exp) = 1.61959548076714e-6
              Ig1 = 1.61959548076714e-12 A,  dIg1 = 1.07972944614873e-11 S
Vg1k =  0.5 : argument = 0, ln(2) = 0.693147180559945
              Ig1 = 6.93147180559945e-7 A,  dIg1 = (1e-6/0.15)*0.5 = 3.33333333333333e-6 S
Vg1k =  1.0 : argument = 3.33333333333333, ln(1+exp) = 3.36838574941280
              Ig1 = 3.36838574941280e-6 A,  dIg1 = 6.43703202889193e-6 S
```

### `PentodeEL34JacobianMatchesCentredDifferencesEverywhere`

`// The screen law kinks at z = 0; skip it.`

The screen law has a max(0, z) kink at z = Vg2k/mu + Vg1k = 0. A centred difference that
straddles the kink averages two different one-sided slopes and is not the derivative there, so
skip a step-sized neighbourhood of it.

### `PentodeEL34ScreenCurrentIsCOneAcrossItsKink`

`// kx > 1, so the screen current is C1 at z = 0.`

z = Vg2k/mu + Vg1k crosses zero at Vg1k = -Vg2k/mu. kx = 1.35 > 1, so z^kx and kx\*z^(kx-1) both
vanish continuously there: no slope step for the Newton solver to trip on.

### `PentodeEL34PlateCurrentKneesOverWithPlateVoltage`

`// Ip factors into a grid term times atan(Vpk/kvb).`

The pentode's whole character is atan(Vpk/kvb): Ip factors into a grid-only term times that
knee, so quadrupling the plate voltage raises Ip only by atan(400/24)/atan(100/24) =
1.51086817167369 / 1.33525134607403 = 1.13152342150114. A triode, whose plate current tracks Vpk
directly, would have risen far more.

### `PentodeEL34JointFormAgreesWithTheSeparateEntryPoints`

`// current() and jacobian() forward to the joint form.`

Same shape as the triode case. current() and jacobian() forward to the joint form, so these
agree exactly by construction today, and pinning that keeps the three entry points from drifting
apart later. The first screen voltage sits below K_SCREEN_FLOOR_VOLTS, so the clamped Vg2k column
is exercised too.

## `test_silicon_diode.cpp`

### `SiliconDiodeMatchesShockleyAtThreeOperatingPoints`

`// 1N914: Is = 2.52e-9 A, n = 1.752, Vt = 0.025864 V.`

1N914: Is = 2.52e-9 A, n = 1.752, Vt = 0.025864 V, so n\*Vt = 0.045313728 V exactly.

```text
v = 0.4 V : v/(nVt) = 8.82734...  exp = 6.81817404536e3
            i  = 2.52e-9 * (6818.17404536 - 1)      = 1.71792785943e-5 A
            di = (2.52e-9 / 0.045313728) * 6818.174 = 3.79174244818e-4 S
v = 0.6 V : v/(nVt) = 13.2410...  exp = 5.6299187171e5
            i  = 2.52e-9 * (562991.87171 - 1)       = 1.41873699671e-3 A
            di = (2.52e-9 / 0.045313728) * 562991.9 = 3.13092649696e-2 S
v = 0.7 V : v/(nVt) = 15.4479...  exp = 5.11586582342e6
            i  = 2.52e-9 * (5115865.82342 - 1)      = 1.2891979355e-2 A
            di = (2.52e-9 / 0.045313728) * 5115866  = 2.84504993167e-1 S
```

### `SiliconDiodeReverseCurrentApproachesMinusSaturationCurrent`

`// At v = -0.5 V the current is -2.51995932614e-9 A.`

v = -0.5 V: exp(-11.0342) = 1.61404223972e-5, i = 2.52e-9 \* (1.614e-5 - 1)

```text
= -2.51995932614e-9 A, di = (2.52e-9/0.045313728)*1.614e-5 = 8.97605786067e-13 S
```

### `SiliconDiodeStaysFiniteAndCOneAcrossTheLinearExtrapolation`

`// The knee sits at v_max = 40 * n * Vt = 1.81254912 V.`

The extrapolation knee sits at v_max = 40 \* n \* Vt = 1.81254912 V; the derivative must be
continuous across it. knee_jacobian_ itself is \~1.309e10 S (Is/(nVt) \* exp(40)), so a fixed
absolute tolerance of 1e-3/1e-9 is unsatisfiable at this magnitude for any correct
implementation: measured directly, jacobian(knee - 1e-7) and jacobian(knee + 1e-7) differ by
\~2.89e4 absolute (\~2.2e-6 relative) purely from the Shockley curve's real slope this close to
exp(40), a 1e-4 relative tolerance (\~1.31e6 absolute) left over 45x slack versus that measured
gap, so this uses 1e-5 relative (\~1.31e5 absolute, \~4.5x the measured gap) instead. The
extrapolation identity below only holds to double's rounding: measured directly, the two sides
differ by \~4e-16 relative (\~1 ULP at this magnitude), so this uses 1e-12 relative, \~2500x
the measured gap, still \~1e3x tighter than the 1e-9 absolute tolerance originally in the brief
once rescaled to this \~7e9 magnitude.

### `SiliconDiodeAntiparallelPairMatchesTwoTimesSaturationCurrentTimesSinh`

`// The pair's total current is 2 Is sinh(w / nVt).`

The D6c clip stage's nonlinear port is v[0] = w, v[1] = -w across one pair, so the pair's total
current is Is(e^(w/nVt) - 1) - Is(e^(-w/nVt) - 1) = 2 Is sinh(w / nVt) and its conductance is
jacobian(w) + jacobian(-w). n\*Vt = 1.752 \* 0.025864 = 0.045313728 V, so 1/(n\*Vt) =
22.068367449. Values computed at double precision in the D6c plan:

```text
w = 0.2  : i = 2.080515143e-7 A,  di/dw = 4.592704262e-6 S
w = 0.3  : i = 1.890822805e-6 A,  di/dw = 4.172752069e-5 S
w = 0.38 : i = 1.105058316e-5 A,  di/dw = 2.438683551e-4 S
w = 0.4  : i = 1.718179822e-5 A,  di/dw = 3.791742530e-4 S
w = 0.5  : i = 1.561297429e-4 A,  di/dw = 3.445528538e-3 S
```

### `SiliconDiodeHonoursANonDefaultIdealityFactor`

`// n = 1 makes n*Vt = 0.025864 V.`

n = 1 makes n\*Vt = 0.025864; at v = 0.6 the exponent is 23.1983..., exp = 1.19426e10, so the
current is far larger than the 1N914's at the same voltage.

### `SiliconDiodeAntiderivativeDifferentiatesBackToTheDiodeCurrent`

`// The antiderivative's slope is the diode current.`

The whole point of the antiderivative is that its slope is the current, so a centred difference
of it has to return the current the model itself reports. The sweep crosses both knees (+-1.8125
V for the defaults), so it covers the Shockley branch and both quadratic extrapolations rather
than only the range a Tube Screamer visits.

### `SiliconDiodeAntiderivativeHasNoStepAtEitherKnee`

`// The bound is the largest slope times the width.`

A step in the antiderivative would put a spike straight into the divided difference
antiderivative antialiasing takes, so neither knee may leave one. The bound is the largest slope
on the interval times its width, which is what differentiability across the knee means.

### `SiliconDiodeJointAntiderivativeFormAgreesWithTheSeparateEntryPoints`

`// The joint form must match the two entry points.`

current_and_antiderivative() exists to share one exponential; it must not drift from the two
entry points that forward to it.

### `SiliconDiodeReverseExtrapolationContinuesTheShockleyLawRatherThanMirroringForward`

`// Below the lower knee the current stays at -Is.`

Below the lower knee the model continues the reverse-biased diode it actually is: a current that
stays at -Is and a conductance of (Is/(n\*Vt))\*exp(-40), some 34 orders of magnitude under the
forward tangent's. The two tangents are not each other's mirror image, and the earlier code that
made them one put a step from -2.5 nA to -593 MA right here (D6c C2).

## `test_triode12_ax7.cpp`

### `Triode12AX7PlateCurrentMatchesKorenAtThreeOperatingPoints`

`// The three points are hand-computed from Koren's law.`

mu = 100, kp = 600, kvb = 300, kg1 = 1060, kx = 1.4.

(1) Vpk = 250, Vgk = -1.5  (the classic 12AX7 quiescent point)

```text
root = sqrt(300 + 62500) = 250.599281722833
t    = 600 * (0.01 - 1.5 / 250.599281722833) = 2.40860901989570
s    = ln(1 + exp(t)) = 2.49473195572810,  E1 = 250 * s / 600 = 1.03947164822004
Ip   = 2 * 1.03947164822004^1.4 / 1060 = 1.99187388684622e-3 A  (1.99 mA, right for a 12AX7)
dIp/dVgk = 2.45547036171309e-3 S,  dIp/dVpk = 2.58169362128740e-5 S
(gm = 2.46 mA/V and rp = 1/2.58e-5 = 38.7 kOhm, both a few percent off the datasheet's
 1.6 mA/V / 62.5 kOhm here - Koren's fit runs hot at this point, a known property of the
 model rather than a transcription error.)
```

(2) Vpk = 100, Vgk = -2.0  (deep toward cutoff)

```text
root = sqrt(300 + 10000) = 101.488915650922,  t = 600 * (0.01 - 2 / root) = -5.82395133797152
s = 2.95154218089098e-3,  E1 = 100 * s / 600 = 4.91923696815163e-4
Ip = 2 * 4.91923696815163e-4^1.4 / 1060 = 4.40941496483551e-8 A
dIp/dVgk = 3.64418892389800e-7 S,  dIp/dVpk = 7.69341309293717e-9 S
```

(3) Vpk = 300, Vgk = 0  (grid at the cathode; the stage is fully on)

```text
root = sqrt(300 + 90000) = 300.499584026334,  t = 600 * 0.01 = 6 exactly
s = ln(1 + exp(6)) = 6.00247568513773,  E1 = 300 * s / 600 = 3.00123784256887
Ip = 2 * 3.00123784256887^1.4 / 1060 = 8.78910615164942e-3 A
dIp/dVgk = 4.08295444882820e-3 S,  dIp/dVpk = 4.10158287076973e-5 S
```

### `Triode12AX7GridCurrentFollowsNormansSoftLaw`

`// Ig = 1e-6 * ln(1 + exp((Vgk - 0.5)/0.15)).`

Ig = 1e-6 \* ln(1 + exp((Vgk - 0.5)/0.15)), dIg/dVgk = (1e-6/0.15) \* logistic(same argument).

```text
Vgk = -1.5 : argument = -13.3333333333333, ln(1+exp) = 1.61959548076714e-6
             Ig = 1.61959548076714e-12 A,  dIg = 1.07972944614873e-11 S (effectively cut off)
Vgk =  0.5 : argument = 0, ln(2) = 0.693147180559945
             Ig = 6.93147180559945e-7 A,  dIg = (1e-6/0.15) * 0.5 = 3.33333333333333e-6 S
Vgk =  1.0 : argument = 3.33333333333333, ln(1+exp) = 3.36838574941280
             Ig = 3.36838574941280e-6 A,  dIg = (1e-6/0.15) * 0.965554 = 6.43703202889193e-6 S
```

### `Triode12AX7IsCutOffAndCOneAtNonPositivePlateVoltage`

`// The piecewise definition is C1 at Vpk = 0.`

Approaching Vpk = 0 from above, plate current and both of its derivatives go to zero, so the
piecewise definition is C1 and not a slope discontinuity like the D5b clamp was.

### `Triode12AX7JointFormAgreesWithTheSeparateEntryPoints`

`// current() and jacobian() forward to the joint form.`

current() and jacobian() forward to the joint form, so these agree exactly by construction
today. Pinning that is the point: the three entry points cannot drift apart later, and a
current() specialised for speed would have to keep matching the form the solver calls.
