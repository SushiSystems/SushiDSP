# tests/foundation/circuits

## `test_fmv_tone_stack.cpp`

### `closed_form_gain_db`

`/// @brief Returns the gain of the hand-derived closed form of the tone stack.`

The hand-derived closed form FmvToneStack carried until 2026-09-20.

Kept here as the reference the netlist must reproduce: it was nulled against the jcm_fmv SPICE
sweep, so it is the one independent statement of the deck's unloaded response the repository
holds. Its coefficients are the Yeh/TSC solution of the rheostat-wired stack with an ideal
source and an open output.

### `FmvToneStackNetlistReproducesTheClosedFormItReplaced`

`// 192 kHz keeps bilinear warping inside the tolerance.`

192 kHz keeps the bilinear warping of the closed form and of the DK discretisation well inside
the tolerance at 6 kHz, where a 48 kHz run would separate them by more than the measurement's
own error.

### `FmvToneStackLoadingAttenuatesTheDeck`, `FmvToneStackLoadingTiltIsTheDecksOwn` and `FmvToneStackLoadingCostsMoreThanOneDecibelAtOneKilohertz`

`// The same termination costs each deck a different tilt.`

The same termination, V1A's 44 k plate impedance ahead of the deck and a 1 M volume pot across
it, costs the two decks different things, because the slope resistor sets how much of a
perturbation a 44 k source is: 33 k on the JCM against 100 k on the IIC+. So a model cannot
carry one loading figure for every amplifier, and terminating one deck says nothing about
another. An independent nodal solve of the same network predicts 3.89 dB of tilt for the JCM's
values and 0.87 dB for the IIC+'s.

## Sizes, 2026-11-10

`FmvToneStackLoadingTiltIsTheDecksOwn` took 0.235 s a deck in the Debug run, twelve 192 kHz runs of
a second and a quarter. Its bound took the unloaded stack's slope from two more runs of each probe;
it now takes it from the closed form at the default knobs of one half, which needs no run and is exact
where the 5 % step was not, leaving eight runs, an estimate of 0.16 s a deck, and the suite stays
`Medium_FmvToneStackLoadingDeck`. The settle of one second is not cut: the crude bound on the slowest time
constant, the resistor sum times the capacitor sum, is 29.7 ms for the JCM but 95 ms for the Mark IIC+,
where one second already leaves only e^-10.5, so there is no room to shorten it without measuring the true
constant. The rate stays 192 kHz, since the warp bound grows with the square of the probe over the rate and
the Mark deck's 0.87 dB tilt would stop being told apart at 96 kHz. The other case of the fixture,
`FmvToneStackLoadingCostsMoreThanOneDecibelAtOneKilohertz`, is two runs, estimated at 0.04 s, and is back in
`Small_FmvToneStackLoadingCost`.
