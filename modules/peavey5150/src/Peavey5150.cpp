/**************************************************************************/
/* Peavey5150.cpp                                                         */
/**************************************************************************/
/*                         This file is part of:                          */
/*                                SushiDSP                                */
/*                https://github.com/SushiSystems/SushiDSP                */
/*                        https://sushisystems.io                         */
/**************************************************************************/
/* Copyright (c) 2026-present Mustafa Garip & Sushi Systems               */
/*                                                                        */
/* Licensed under the Apache License, Version 2.0 (the "License");        */
/* you may not use this file except in compliance with the License.       */
/* You may obtain a copy of the License at                                */
/*                                                                        */
/*     http://www.apache.org/licenses/LICENSE-2.0                         */
/*                                                                        */
/* Unless required by applicable law or agreed to in writing, software    */
/* distributed under the License is distributed on an "AS IS" BASIS,      */
/* WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or        */
/* implied. See the License for the specific language governing           */
/* permissions and limitations under the License.                         */
/**************************************************************************/

// _USE_MATH_DEFINES must precede every include: it is what makes MSVC's <cmath> declare M_PI.
#define _USE_MATH_DEFINES

#include "SushiDSP/peavey5150/Peavey5150.hpp"

#include "SushiDSP/circuits/stages/PowerTubePresets.hpp"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <utility>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace SushiDSP
{
    namespace
    {
        // Every number in this block cites the section of
        // docs/reference/research/2026-09-06-peavey-5150-design-dossier.md it is read from or
        // derived from. A value the dossier marks not established, or that is a modelling choice
        // the dossier does not carry, says so here and appears in modules/peavey5150/README.md.

        constexpr float kTriodeSaturationCurrent = 1e-9f;   // Dossier section 7, unchanged from kEl34.
        constexpr float kTriodeThermalVoltage = 0.02585f;   // kT/q at room temperature; dossier section 7.

        // Small-signal gains, dossier section 3's per-stage table at B+ = 340 V.
        constexpr float kV1aGain = 77.4f;
        constexpr float kV1bGain = 65.2f;
        constexpr float kV2aGain = 2.21f;   // The cold clipper, R15 39k unbypassed.
        constexpr float kV2bGain = 77.4f;
        constexpr float kV5bGain = 41.4f;
        constexpr float kV5aGain = 25.3f;
        constexpr float kV3aGain = 14.0f;   // R14 3.3k read as unbypassed in the audio band.

        // Dossier section 5 gives the long-tailed pair's resistors but no gain figure, so this one
        // is derived here and is a modelling choice, not a dossier value: mu = 100 from the Koren
        // 12AX7 of section 3, ra = 62 kOhm from section 7's own phase-inverter arithmetic, and the
        // per-plate gain of a long-tailed pair is half the common-cathode figure. The V4B side
        // gives 100*82/(62+82)/2 = 28.47 and the V4A side 100*100/(62+100)/2 = 30.86; 29.7 is the
        // pair's average, the same way section 7 averages the two halves for source_resistance.
        constexpr float kPhaseInverterGain = 29.7f;

        // Grid-leak resistors, dossier section 2's diagram, one per driven grid.
        constexpr float kV1aGridLeak = 1000000.0f;   // R25 at the input jack.
        constexpr float kV1bGridLeak = 803000.0f;    // R82 470k in series with the pot node's 333k.
        constexpr float kV2aGridLeak = 1000000.0f;   // R9.
        constexpr float kV2bGridLeak = 330000.0f;    // R11 at V2A's output node.
        constexpr float kV5bGridLeak = 100000.0f;    // R101.
        constexpr float kV5aGridLeak = 1000000.0f;   // R91.
        constexpr float kV3aGridLeak = 1000000.0f;   // R26.
        constexpr float kPhaseInverterGridLeak = 1000000.0f;  // R50, node X to V4B's grid.

        // Cathode bias, dossier section 3's DC table at B+ = 340 V. The rail is not established
        // (dossier open question 1), so this column is the middle of the three the table gives.
        constexpr float kV1aCathodeBias = -0.90f;
        constexpr float kV1bCathodeBias = -1.34f;
        constexpr float kV2aCathodeBias = -4.00f;
        constexpr float kV2bCathodeBias = -0.90f;
        constexpr float kV5bCathodeBias = -1.49f;
        constexpr float kV5aCathodeBias = -2.00f;
        constexpr float kV3aCathodeBias = -2.53f;

        // The long-tailed pair has no row in the dossier's DC table; -2.0 V is a modelling choice
        // in the range the table's other 12AX7 rows occupy.
        constexpr float kPhaseInverterCathodeBias = -2.0f;

        // Source resistances: the Thevenin impedance each grid is driven from, built the way
        // dossier section 7 builds the output tubes' - the previous stage's plate load in parallel
        // with its solved plate resistance (section 3's ra column), plus the series elements
        // section 2's diagram puts between them, in parallel with the shunt at the grid node.
        constexpr float kV1aSourceResistance = 90000.0f;    // R27 22k plus R22 68k.
        constexpr float kV1bSourceResistance = 671500.0f;   // (40.3k + R29 470k) || 333k, plus R82 470k.
        constexpr float kV2aSourceResistance = 333700.0f;   // (30.7k + R6 470k) || R9 1M.
        constexpr float kV2bSourceResistance = 281100.0f;   // (75.0k || R11 330k) plus R7 220k.
        constexpr float kV5bSourceResistance = 91200.0f;    // (40.3k + R32 1M) || R101 100k.
        constexpr float kV5aSourceResistance = 671730.0f;   // (46.3k + R87 1M + R88 1M) || R91 1M.
        constexpr float kV3aSourceResistance = 200000.0f;   // The post-gain node's R85 || R81.
        constexpr float kPhaseInverterSourceResistance = 132800.0f;  // 32.8k plus R48 100k.

        // Interstage dividers, dossier section 3, each computed the way section 3 computes V2B's:
        // the series element against the shunt at the driven grid.
        constexpr float kV1bToV2aAttenuation = 0.680f;    // R9 1M / (R6 470k + R9 1M), -3.34 dB.
        constexpr float kV2aToV2bAttenuation = 0.8147f;   // R11 330k against V2A's 75.0k source.
        constexpr float kV2bToV5bAttenuation = 0.0909f;   // R101 100k / (R32 1M + R101 100k), -20.8 dB.

        // Section 3 reads R88/R91 as a 0.5 divider and records R87 1 MOhm as a series element
        // beside it; folded together the way the same section folds R32 into V2B's divider, the
        // three 1 MOhm resistors give R91 / (R87 + R88 + R91) = 0.3333.
        constexpr float kV5bToV5aAttenuation = 0.3333f;

        // Level staging. Modelling choices, top to bottom, and not dossier values: the dossier's
        // own dividers above are the real circuit's, and on their own they leave every grid past
        // V1A driven two to three orders of magnitude beyond its conduction knee, which turns the
        // cascade into a fixed-amplitude square-wave generator with a dead gain knob. Measured
        // before these were added: at a -40 dBFS input and PRE GAIN 0.05, V5A's grid sat at 5.63 V
        // against a knee near 0.10 V.
        //
        // One rule sets all seven, so none of them is a free knob. A link's total attenuation is
        // 1/(the gain of the stage it drives), which is what carries the operating level forward
        // unchanged - the argument modules/jcm800/src/Jcm800.cpp spells out for its own interstage
        // constants. Each staging factor below is that target divided by the dossier's own divider
        // for the same link, so the dossier still sets each link's frequency behaviour and its
        // relative weight and the staging factor only sets where the cascade sits in level.
        constexpr float kPreGainPotCeiling = 0.0153374f;               // (1/65.2) / 1.0.
        constexpr float kV1bToV2aLevelStaging = 0.665425f;             // (1/2.21) / 0.680.
        constexpr float kV2aToV2bLevelStaging = 0.0158584f;            // (1/77.4) / 0.8147.
        constexpr float kV2bToV5bLevelStaging = 0.265728f;             // (1/41.4) / 0.0909.
        constexpr float kV5bToV5aLevelStaging = 0.118589f;             // (1/25.3) / 0.3333.
        constexpr float kToneStackToV3aLevelStaging = 0.289396f;       // (1/14.0) / (0.4825*0.522*0.98).
        constexpr float kV3aToPhaseInverterLevelStaging = 0.0336700f;  // (1/29.7) / 1.0.

        // The eighth runs the other way, as the VH4's kFollowerToMixBusLevelStaging does. The seven
        // above hand the pair's plate a fraction of a volt, and a 6L6GC with this preset's gain of
        // 6 reaches its 1/0.048 = 20.8 V ceiling only at 3.47 V of grid drive, so without a lift
        // the power section would never leave its linear region and the amplifier would have no
        // power-stage character at all. Measured to put the default setting just into saturation.
        constexpr float kPhaseInverterToPowerTubeLevelStaging = 30.0f;

        // Dossier section 5: the pair's plates drive R55/R54 220k through C29/C28 with only the
        // 2.2 kOhm stoppers between the node and the grids, so the loss is the 220 kOhm leak
        // against the plate's own 82k || 62k = 35.3 kOhm: 220/(220 + 35.3) = 0.8617.
        constexpr float kPhaseInverterToPowerTubeAttenuation = 0.8617f;

        // Not a dossier value. The JCM800's output-level trim into the transformer, adopted
        // unchanged from modules/jcm800/src/Jcm800.cpp so the transformer and the sag stage see
        // the level they were calibrated at; a modelling choice.
        constexpr float kPowerTubeToTransformerAttenuation = 0.05f;

        // Dossier section 3: V3B's plate ties straight to V2 with no plate resistor, and section 3
        // uses 0.98 as the follower's gain when it computes the bootstrapped grid return. The
        // follower is modelled as that scalar and its coupling capacitor; its compression is not
        // modelled.
        constexpr float kCathodeFollowerGain = 0.98f;

        // Grid bias mismatch between the two halves of the output stage, applied input-referred.
        // Not a dossier value: adopted from modules/jcm800/src/Jcm800.cpp's
        // kPowerTubeGridBiasMismatchVolts, whose comment carries the argument for why a
        // cathode-bias difference would be inert and an input-referred offset is not.
        constexpr float kPowerTubeGridBiasMismatchVolts = 0.03f;

        // Not dossier values. Dossier section 5 leaves the output transformer's primary impedance
        // and turns ratio not established (open question 2), so the JCM800's transformer and sag
        // constants are adopted unchanged from modules/jcm800/src/Jcm800.cpp.
        constexpr float kOutputTransformerRolloffHz = 60.0f;
        constexpr float kOutputTransformerLeakageHz = 8000.0f;
        constexpr float kOutputTransformerSaturationDrive = 0.3f;
        constexpr float kSagAmount = 0.3f;
        constexpr float kSagRecoveryTimeSeconds = 0.05f;

        // 12AX7 Miller input capacitance, the same model modules/jcm800/src/Jcm800.cpp uses:
        // C_in = Cgk + Cgp * (1 + |A|) + stray, with Cgk 1.6 pF and Cgp 1.7 pF from
        // docs/reference/spice/models/12ax7_koren.sub and 10 pF of point-to-point stray wiring.
        // The driving impedance is taken as the previous stage's plate load alone, as Jcm800.cpp
        // takes it: the real Thevenin is higher at every one of these grids (the kV*SourceResistance
        // values above are that figure), so the errors all run the same way and this model is
        // brighter than the amplifier rather than darker.
        constexpr float kMillerCgkPicofarads = 1.6f;
        constexpr float kMillerCgpPicofarads = 1.7f;
        constexpr float kStrayWiringPicofarads = 10.0f;

        // Plate loads, dossier section 2, one per driving stage.
        constexpr double kV1aPlateLoadOhms = 220000.0;   // R2.
        constexpr double kV1bPlateLoadOhms = 100000.0;   // R3.
        constexpr double kV2aPlateLoadOhms = 100000.0;   // R1.
        constexpr double kV2bPlateLoadOhms = 220000.0;   // R4.
        constexpr double kV5bPlateLoadOhms = 220000.0;   // R96.
        constexpr double kV5aPlateLoadOhms = 100000.0;   // R86.
        constexpr double kV3aPlateLoadOhms = 68000.0;    // R5.

        // Input network, dossier section 2: R22 68 kOhm against C20 39 pF.
        // 1/(2*pi*68k*39p) = 60.01 kHz.
        constexpr double kInputStopperOhms = 68000.0;
        constexpr double kInputShuntFarads = 39e-12;

        // Coupling capacitors and the loads they work into, dossier section 2. The computed corner
        // follows each pair.
        constexpr double kC6Farads = 22e-9, kC6LoadOhms = 470000.0;      // R29:         15.39 Hz.
        constexpr double kC2Farads = 22e-9, kC2LoadOhms = 1470000.0;     // R6 + R9:      4.92 Hz.
        constexpr double kC1Farads = 22e-9, kC1LoadOhms = 405000.0;      // 75.0k + R11: 17.86 Hz.
        constexpr double kC57Farads = 22e-9, kC57LoadOhms = 1100000.0;   // R32 + R101:   6.58 Hz.
        constexpr double kC56Farads = 22e-9, kC56LoadOhms = 3000000.0;   // R87+R88+R91:  2.41 Hz.
        constexpr double kC11Farads = 47e-9, kC11LoadOhms = 1000000.0;   // R26:          3.39 Hz,
                                                                        // the dossier's own 3.4 Hz.

        // C52 into V3B's bootstrapped grid. Dossier section 3 puts that grid's impedance near
        // 1M/(1 - 0.98) = 50 MOhm, so the corner is 1/(2*pi*22n*50M) = 0.145 Hz.
        constexpr double kC52Farads = 22e-9, kC52LoadOhms = 50000000.0;

        // C49 into the pair's grid. Dossier section 5 puts R48 100 kOhm in series and R50 1 MOhm
        // from node X to the same grid as its DC return, so the load is the two in series and the
        // corner is 1/(2*pi*22n*1.1M) = 6.58 Hz. Taking R48 alone would put it at 72.3 Hz, which
        // is a bass cut no coupling capacitor of this size performs.
        constexpr double kC49Farads = 22e-9, kC49LoadOhms = 1100000.0;

        // The lead pre-gain treble peaker, dossier section 3: R29 470 kOhm in parallel with
        // C17 2.2 nF, working into the pot node's R40 || R41 || VR2 = 333 kOhm. At DC the divider
        // passes 333/(470 + 333) = 0.4147 and above 371 Hz the capacitor shorts R29 and it
        // approaches unity. Zero 1/(2*pi*470k*2.2n) = 153.92 Hz, pole
        // 1/(2*pi*(470k || 333k)*2.2n) = 371.17 Hz, and 0.4147 * 371.17/153.92 = 1.0 exactly, so
        // the shelf is the network rather than a fit to it.
        constexpr double kPreGainPeakerZeroHz = 153.92;
        constexpr double kPreGainPeakerPoleHz = 371.17;
        constexpr float kPreGainPeakerLowGain = 0.41469f;

        // C48 1 nF at V2A's output node, dossier section 3. With K1B lifted (see the K1 reading
        // below) the node's shunt is R11 330 kOhm alone, and against V2A's own 100k || 301k =
        // 75.0 kOhm source the pole is 1/(2*pi*1n*(75.0k || 330k)) = 2603 Hz. The dossier's 482 Hz
        // is the same capacitor against R11 with the source taken as zero, which it says it is not.
        constexpr double kC48CornerHz = 2603.1;

        // The three bypassed cathodes, dossier section 3's V1A paragraph: R 1.02 kOhm with a 1 uF
        // non-polar across it. Each is a first-order shelf, unity above its pole - where the stage
        // runs the gain section 3's table prints - and degenerate below its zero.
        //
        // Both corners come from R, C and the stage's own small-signal figures, not from section
        // 3's printed 462 Hz. That figure is 1/(2*pi*C*(Rk || 1/gm)), the plate-follower form; a
        // plate-loaded stage degenerates against (ra + Ra)/(mu + 1) instead, and only that form
        // reproduces the shelf depth section 3 states in the same paragraph.
        //   zero = 1/(2*pi*C*Rk)
        //   pole = 1/(2*pi*C*(Rk || (ra + Ra)/(mu + 1)))
        //
        // V1A and V2B share a 220k/1.02k stage at ra 49.4k and mu 94.8, so they share its numbers:
        // (49.4k + 220k)/95.8 = 2.812 kOhm, 1.02k || 2.812k = 748.7 Ohm, zero 156.03 Hz, pole
        // 212.60 Hz. The depth that falls out, 156.03/212.60 = 0.7339, is section 3's own figure
        // (a gain of 77.4 falling to 56.8, 0.7339) to four places.
        constexpr double kV1aCathodeZeroHz = 156.03, kV1aCathodePoleHz = 212.60;
        constexpr float kV1aCathodeLowGain = 0.73391f;

        // V1B: 100k/1.02k at ra 44.4k and mu 94.1, so (44.4k + 100k)/95.1 = 1.5184 kOhm,
        // 1.02k || 1.5184k = 610.0 Ohm, zero 156.03 Hz, pole 260.90 Hz. Depth 0.5980, against
        // section 3's 39.0/65.2 = 0.5982.
        constexpr double kV1bCathodeZeroHz = 156.03, kV1bCathodePoleHz = 260.90;
        constexpr float kV1bCathodeLowGain = 0.59805f;

        // V3A's C61 4.7 nF, the same construction: 3.3k/68k at ra 63.5k and mu 86.4, so
        // (63.5k + 68k)/87.4 = 1.5046 kOhm, 3.3k || 1.5046k = 1.0332 kOhm, zero 10.261 kHz, pole
        // 32.777 kHz. The lift that falls out, 3.194, is the ratio between the stage bypassed
        // (mu*Ra/(ra+Ra) = 44.68) and as drawn (14.0), which is 3.191. Section 3's "near 56 kHz" is
        // again the 1/gm form. The shelf is unity below its zero, so the stage runs section 3's
        // 14.0 through the audio band, which is what section 3 means by calling C61 an oscillation
        // stopper rather than an audio bypass.
        constexpr double kV3aCathodeZeroHz = 10261.0, kV3aCathodePoleHz = 32777.0;
        constexpr float kV3aCathodeLowGain = 1.0f;

        // Dossier section 6: "Active Presence: +10 dB 2 kHz" and "Active Resonance: +10 dB at the
        // cabinet resonant frequency". Both are frequency-selective reductions of the global
        // feedback rather than active circuits, and the +10 dB is the loop's.
        constexpr float kMaxPresenceDb = 10.0f;
        constexpr float kResonanceMaxBoostDb = 10.0f;

        // Both corners sit below the frequency section 6 names, and the reason is the shelf's own
        // shape rather than the circuit. An RBJ shelf reaches half its decibel gain at its corner
        // and the plateau only well past it, so a shelf cornered at 2 kHz is +5 dB at 2 kHz, not
        // the +10 dB section 6 publishes. Section 6 states the boost measured *at* a frequency, so
        // each corner is placed where the plateau has arrived by then: 600 Hz reads 9.90 dB at
        // 2 kHz and 330 Hz reads 9.90 dB at 100 Hz. Both placements are modelling choices.
        constexpr float kPresenceHz = 600.0f;

        // Section 5 derives no corner for the resonance shelf and section 6 names only "the
        // cabinet resonant frequency", taken here as 100 Hz, in the range a 4x12 resonates at.
        // The shelf's own corner sits above it for the reason given above.
        constexpr float kResonanceCornerHz = 330.0f;

        // The lowest corner a coupling capacitor in this chain can actually be given. Eight of the
        // eleven compute below 20 Hz, and at 8 x 48 kHz a second-order section at 2 Hz puts its
        // pole within 5e-6 of z = 1, which a Biquad's float coefficients and float state cannot
        // hold: the section stops behaving as a filter and its state accumulates instead of
        // decaying. Realising those eight at 20 Hz costs 0.016 dB each at the low E's 82 Hz,
        // 0.13 dB across all eight, and 1.9 dB at 41 Hz. A modelling choice forced by the
        // arithmetic and not by the circuit; each capacitor's computed corner stays in its comment.
        constexpr double kMinimumCouplingCornerHz = 20.0;

        constexpr double kFilterQ = 0.7071;
        constexpr float kSmoothingMs = 20.0f;

        // Below this a recomputed coefficient set would be numerically indistinguishable from the
        // one in place, so the audio thread skips the rebuild. A cost filter, not a quality one.
        constexpr float kCoefficientEpsilon = 1e-4f;

        constexpr float kMinPostGainDb = -60.0f;
        constexpr float kMaxPostGainDb = 12.0f;
        constexpr float kMinInputTrimDb = -24.0f;
        constexpr float kMaxInputTrimDb = 24.0f;
        constexpr float kMinOutputDb = -60.0f;
        constexpr float kMaxOutputDb = 12.0f;

        /// @brief The corner of a first-order RC network.
        /// @param resistance_ohms The resistance the capacitor works into.
        /// @param capacitance_farads The capacitance.
        /// @return The -3 dB corner in hertz.
        double corner_hz(double resistance_ohms, double capacitance_farads)
        {
            return 1.0 / (2.0 * M_PI * resistance_ohms * capacitance_farads);
        }

        /// @brief One coupling capacitor's corner, raised to the lowest a float biquad can hold.
        /// @param resistance_ohms The resistance the capacitor works into.
        /// @param capacitance_farads The capacitance.
        /// @return The corner in hertz, never below kMinimumCouplingCornerHz.
        double coupling_corner_hz(double resistance_ohms, double capacitance_farads)
        {
            return std::max(corner_hz(resistance_ohms, capacitance_farads), kMinimumCouplingCornerHz);
        }

        /// @brief The corner of one grid's Miller input-capacitance pole.
        /// @param stage_voltage_gain The driven stage's mid-band gain, which multiplies Cgp.
        /// @param plate_load_ohms The driving stage's plate load.
        /// @return The -3 dB corner of the grid's RC pole, in hertz.
        double miller_corner_hz(double stage_voltage_gain, double plate_load_ohms)
        {
            const double input_capacitance_farads =
                (kMillerCgkPicofarads + kMillerCgpPicofarads * (1.0 + stage_voltage_gain)
                 + kStrayWiringPicofarads) * 1e-12;
            return corner_hz(plate_load_ohms, input_capacitance_farads);
        }

        /// @brief The presence shelf's boost at a knob position.
        /// @param normalized Pot position in [0, 1]; 0.5 is flat.
        /// @return The shelf gain in decibels.
        float presence_gain_db(float normalized)
        {
            return (normalized - 0.5f) * 2.0f * kMaxPresenceDb;
        }

        // VR2 sits from the pot node to ground with the wiper feeding V1B, so wide open it passes
        // the whole node; what it passes into is kPreGainPotCeiling, this link's staging factor.
        // The exponent is the same audio taper modules/jcm800/src/Jcm800.cpp uses for the same kind
        // of pot.
        float pre_gain_attenuation(float normalized)
        {
            return normalized * std::sqrt(normalized) * kPreGainPotCeiling;
        }
    }

    Peavey5150::Peavey5150(std::vector<float> cabinet_impulse_response)
        : cabinet_impulse_response_(std::move(cabinet_impulse_response))
        , v1a_(kV1aSourceResistance, kV1aGridLeak, kV1aCathodeBias, kV1aGain, kTriodeSaturationCurrent, kTriodeThermalVoltage)
        , v1b_(kV1bSourceResistance, kV1bGridLeak, kV1bCathodeBias, kV1bGain, kTriodeSaturationCurrent, kTriodeThermalVoltage)
        , v2a_(kV2aSourceResistance, kV2aGridLeak, kV2aCathodeBias, kV2aGain, kTriodeSaturationCurrent, kTriodeThermalVoltage)
        , v2b_(kV2bSourceResistance, kV2bGridLeak, kV2bCathodeBias, kV2bGain, kTriodeSaturationCurrent, kTriodeThermalVoltage)
        , v5b_(kV5bSourceResistance, kV5bGridLeak, kV5bCathodeBias, kV5bGain, kTriodeSaturationCurrent, kTriodeThermalVoltage)
        , v5a_(kV5aSourceResistance, kV5aGridLeak, kV5aCathodeBias, kV5aGain, kTriodeSaturationCurrent, kTriodeThermalVoltage)
        , tone_stack_(48000.0 * kOversamplingFactor)
        , v3a_(kV3aSourceResistance, kV3aGridLeak, kV3aCathodeBias, kV3aGain, kTriodeSaturationCurrent, kTriodeThermalVoltage)
        , phase_inverter_(kPhaseInverterSourceResistance, kPhaseInverterGridLeak, kPhaseInverterCathodeBias, kPhaseInverterGain, kTriodeSaturationCurrent, kTriodeThermalVoltage)
        , power_tube_a_(kPeavey5150Power6L6GC)
        , power_tube_b_(kPeavey5150Power6L6GC)
        , output_transformer_(48000.0 * kOversamplingFactor, kOutputTransformerRolloffHz, kOutputTransformerLeakageHz, kOutputTransformerSaturationDrive)
        , sag_(48000.0 * kOversamplingFactor, kSagAmount, kSagRecoveryTimeSeconds)
        , resonance_shelf_(48000.0, kResonanceCornerHz, kResonanceMaxBoostDb)
    {
        power_tube_grid_bias_mismatch_volts_ = kPowerTubeGridBiasMismatchVolts;
        resonance_shelf_.set_depth(resonance_.target());
    }

    void Peavey5150::prepare(const PrepareInfo& info)
    {
        sample_rate_ = info.sample_rate;

        // The whole amplifier from the grid stopper to the sag stage sits inside one 8x region, so
        // every filter in it is derived at the oversampled rate; deriving one at the base rate
        // would place its real corner eight times higher.
        const double rate = sample_rate_ * kOversamplingFactor;

        input_low_pass_.set_one_pole_low_pass(rate, corner_hz(kInputStopperOhms, kInputShuntFarads));
        c6_coupling_.set_high_pass(rate, coupling_corner_hz(kC6LoadOhms, kC6Farads), kFilterQ);
        pre_gain_peaker_.set_first_order_shelf(rate, kPreGainPeakerZeroHz, kPreGainPeakerPoleHz, kPreGainPeakerLowGain);
        c2_coupling_.set_high_pass(rate, coupling_corner_hz(kC2LoadOhms, kC2Farads), kFilterQ);
        c1_coupling_.set_high_pass(rate, coupling_corner_hz(kC1LoadOhms, kC1Farads), kFilterQ);
        c48_low_pass_.set_one_pole_low_pass(rate, kC48CornerHz);
        c57_coupling_.set_high_pass(rate, coupling_corner_hz(kC57LoadOhms, kC57Farads), kFilterQ);
        c56_coupling_.set_high_pass(rate, coupling_corner_hz(kC56LoadOhms, kC56Farads), kFilterQ);
        c52_coupling_.set_high_pass(rate, coupling_corner_hz(kC52LoadOhms, kC52Farads), kFilterQ);
        c11_coupling_.set_high_pass(rate, coupling_corner_hz(kC11LoadOhms, kC11Farads), kFilterQ);
        c49_coupling_.set_high_pass(rate, coupling_corner_hz(kC49LoadOhms, kC49Farads), kFilterQ);

        v1a_cathode_shelf_.set_first_order_shelf(rate, kV1aCathodeZeroHz, kV1aCathodePoleHz, kV1aCathodeLowGain);
        v1b_cathode_shelf_.set_first_order_shelf(rate, kV1bCathodeZeroHz, kV1bCathodePoleHz, kV1bCathodeLowGain);
        v2b_cathode_shelf_.set_first_order_shelf(rate, kV1aCathodeZeroHz, kV1aCathodePoleHz, kV1aCathodeLowGain);
        v3a_cathode_shelf_.set_first_order_shelf(rate, kV3aCathodeZeroHz, kV3aCathodePoleHz, kV3aCathodeLowGain);

        v1b_grid_miller_.set_one_pole_low_pass(rate, miller_corner_hz(kV1bGain, kV1aPlateLoadOhms));
        v2a_grid_miller_.set_one_pole_low_pass(rate, miller_corner_hz(kV2aGain, kV1bPlateLoadOhms));
        v2b_grid_miller_.set_one_pole_low_pass(rate, miller_corner_hz(kV2bGain, kV2aPlateLoadOhms));
        v5b_grid_miller_.set_one_pole_low_pass(rate, miller_corner_hz(kV5bGain, kV2bPlateLoadOhms));
        v5a_grid_miller_.set_one_pole_low_pass(rate, miller_corner_hz(kV5aGain, kV5bPlateLoadOhms));
        v3a_grid_miller_.set_one_pole_low_pass(rate, miller_corner_hz(kV3aGain, kV5aPlateLoadOhms));
        phase_inverter_grid_miller_.set_one_pole_low_pass(rate, miller_corner_hz(kPhaseInverterGain, kV3aPlateLoadOhms));

        tone_stack_ = Peavey5150ToneStack(rate);
        output_transformer_ = OutputTransformerStage(rate, kOutputTransformerRolloffHz, kOutputTransformerLeakageHz, kOutputTransformerSaturationDrive);
        sag_ = PowerSupplySagStage(rate, kSagAmount, kSagRecoveryTimeSeconds);

        // The presence shelf and the resonance shelf are both linear and both sit after the
        // decimator, so they are derived at the base rate and cost eight times less there.
        presence_shelf_.set_high_shelf(sample_rate_, kPresenceHz, presence_gain_db(presence_.target()), kFilterQ);
        resonance_shelf_ = ResonanceControl(sample_rate_, kResonanceCornerHz, kResonanceMaxBoostDb);
        resonance_shelf_.set_depth(resonance_.target());

        SmoothedParameter* const smoothed[] = {
            &pre_gain_, &resonance_, &presence_, &low_, &mid_, &high_,
            &post_gain_, &input_trim_gain_, &output_gain_
        };
        for (SmoothedParameter* parameter : smoothed)
        {
            parameter->prepare(sample_rate_, kSmoothingMs);
            parameter->snap(parameter->target());
        }
        applied_low_ = -1.0f;
        applied_mid_ = -1.0f;
        applied_high_ = -1.0f;
        applied_presence_ = -1.0f;
        applied_resonance_ = -1.0f;

        oversampler_ = std::make_unique<Oversampler8x>(info.max_block);
        scratch_base_.assign(info.max_block, 0.0f);
        scratch_oversampled_.assign(static_cast<std::size_t>(info.max_block) * kOversamplingFactor, 0.0f);

        // StreamingConvolution re-blocks the stream internally, so the partition is a property of
        // the sample rate alone and every block size gets the same cabinet and the same latency.
        cabinet_partition_ = StreamingConvolution::partition_size_for(sample_rate_);
        if (!cabinet_impulse_response_.empty() && info.max_block > 0)
        {
            cabinet_slot_.publish(std::make_unique<StreamingConvolution>(
                cabinet_partition_, cabinet_impulse_response_.data(),
                static_cast<std::uint32_t>(cabinet_impulse_response_.size()), info.max_block));
        }

        reset();
    }

    void Peavey5150::process(ProcessContext& context)
    {
        const float* input = context.inputs[0];
        float* output = context.outputs[0];
        const std::uint32_t frames = context.frames;
        const std::uint32_t oversampled_frames = frames * kOversamplingFactor;

        // Control changes are only ever targets; the audio thread is where they take effect. The
        // per-block advance is one closed-form step and every rebuild below is plain arithmetic on
        // stack values - no allocation, no lock, no syscall.
        const float pre_gain = pre_gain_.skip(frames);
        const float low = low_.skip(frames);
        const float mid = mid_.skip(frames);
        const float high = high_.skip(frames);
        const float resonance = resonance_.skip(frames);
        const float presence = presence_.skip(frames);
        const float input_gain = input_trim_gain_.skip(frames);
        const float post_gain = post_gain_.skip(frames);
        const float output_gain = output_gain_.skip(frames);

        if (std::fabs(low - applied_low_) > kCoefficientEpsilon)
        {
            tone_stack_.set_low(low);
            applied_low_ = low;
        }
        if (std::fabs(mid - applied_mid_) > kCoefficientEpsilon)
        {
            tone_stack_.set_mid(mid);
            applied_mid_ = mid;
        }
        if (std::fabs(high - applied_high_) > kCoefficientEpsilon)
        {
            tone_stack_.set_high(high);
            applied_high_ = high;
        }
        if (std::fabs(presence - applied_presence_) > kCoefficientEpsilon)
        {
            presence_shelf_.set_high_shelf(sample_rate_, kPresenceHz, presence_gain_db(presence), kFilterQ);
            applied_presence_ = presence;
        }
        if (std::fabs(resonance - applied_resonance_) > kCoefficientEpsilon)
        {
            resonance_shelf_.set_depth(resonance);
            applied_resonance_ = resonance;
        }

        const float pre_gain_scalar = pre_gain_attenuation(pre_gain);

        for (std::uint32_t i = 0; i < frames; ++i)
        {
            scratch_base_[i] = input[i] * input_gain;
        }

        const float* upsampled = oversampler_->upsample(scratch_base_.data(), frames);
        for (std::uint32_t i = 0; i < oversampled_frames; ++i)
        {
            const float v1a_out = v1a_cathode_shelf_.process_sample(
                v1a_.process_sample(input_low_pass_.process_sample(upsampled[i])));

            const float v1b_in = v1b_grid_miller_.process_sample(
                pre_gain_peaker_.process_sample(c6_coupling_.process_sample(v1a_out)) * pre_gain_scalar);
            const float v1b_out = v1b_cathode_shelf_.process_sample(v1b_.process_sample(v1b_in));

            const float v2a_in = v2a_grid_miller_.process_sample(
                c2_coupling_.process_sample(v1b_out) * kV1bToV2aAttenuation * kV1bToV2aLevelStaging);
            const float v2a_out = v2a_.process_sample(v2a_in);

            const float v2b_in = v2b_grid_miller_.process_sample(
                c48_low_pass_.process_sample(c1_coupling_.process_sample(v2a_out))
                * kV2aToV2bAttenuation * kV2aToV2bLevelStaging);
            const float v2b_out = v2b_cathode_shelf_.process_sample(v2b_.process_sample(v2b_in));

            const float v5b_in = v5b_grid_miller_.process_sample(
                c57_coupling_.process_sample(v2b_out) * kV2bToV5bAttenuation * kV2bToV5bLevelStaging);
            const float v5b_out = v5b_.process_sample(v5b_in);

            const float v5a_in = v5a_grid_miller_.process_sample(
                c56_coupling_.process_sample(v5b_out) * kV5bToV5aAttenuation * kV5bToV5aLevelStaging);
            const float v5a_out = v5a_.process_sample(v5a_in);

            // The tone stack's own input network carries R24's loss and the C59/R94 leg, so the
            // plate voltage goes in unattenuated and the only thing after the deck is the pot.
            const float post_gain_node = tone_stack_.process_sample(v5a_out) * post_gain;

            // V3B is a scalar and its coupling capacitor: the follower's own compression is not
            // modelled, and the dossier reads it as hanging off the post-gain node in parallel
            // with V3A rather than in series ahead of it (see the README).
            const float follower_out = c52_coupling_.process_sample(post_gain_node) * kCathodeFollowerGain;

            const float v3a_in = v3a_grid_miller_.process_sample(
                c11_coupling_.process_sample(follower_out) * kToneStackToV3aLevelStaging);
            const float v3a_out = v3a_cathode_shelf_.process_sample(v3a_.process_sample(v3a_in));

            const float pair_in = phase_inverter_grid_miller_.process_sample(
                c49_coupling_.process_sample(v3a_out) * kV3aToPhaseInverterLevelStaging);
            const float pair_out = phase_inverter_.process_sample(pair_in)
                * kPhaseInverterToPowerTubeAttenuation * kPhaseInverterToPowerTubeLevelStaging;

            const float power_a = power_tube_a_.process_sample(pair_out);
            const float power_b = power_tube_b_.process_sample(-pair_out - power_tube_grid_bias_mismatch_volts_);
            const float power_out = (power_a - power_b) * kPowerTubeToTransformerAttenuation;

            // The transformer's core saturation is a genuine tanh nonlinearity and the sag stage's
            // gain is signal-dependent, so both stay inside the oversampled region; running either
            // after the decimator would fold their harmonics back below Nyquist.
            scratch_oversampled_[i] = sag_.process_sample(output_transformer_.process_sample(power_out));
        }
        oversampler_->downsample(scratch_oversampled_.data(), oversampled_frames, scratch_base_.data());

        // Both feedback shelves are linear, so they generate nothing to alias and cost eight times
        // less on this side of the decimator.
        for (std::uint32_t i = 0; i < frames; ++i)
        {
            const float shelved = presence_shelf_.process_sample(scratch_base_[i]);
            scratch_base_[i] = resonance_shelf_.process_sample(shelved) * kPostGainUnityGain;
        }

        // One atomic acquire is the audio thread's entire share of a cabinet swap; the convolver it
        // hands back was built in full on the control thread.
        StreamingConvolution* const cabinet = cabinet_slot_.acquire_for_audio_thread();
        if (cabinet != nullptr)
        {
            cabinet->process(scratch_base_.data(), output, frames);
        }
        else
        {
            for (std::uint32_t i = 0; i < frames; ++i)
            {
                output[i] = scratch_base_[i];
            }
        }

        // Nothing upstream is normalised to the converter's range - every stage is bounded only by
        // its own tube's conduction window - so a hot pre-gain, post-gain and output combination
        // can still land outside [-1, 1]. This is a safety rail, not a tone stage: a soft-knee
        // limiter here would need its own oversampled region to avoid aliasing its own harmonics.
        for (std::uint32_t i = 0; i < frames; ++i)
        {
            output[i] = std::clamp(output[i] * output_gain, -1.0f, 1.0f);
        }
    }

    std::uint32_t Peavey5150::latency_samples() const
    {
        const std::uint32_t oversampling_latency = oversampler_ ? oversampler_->latency_samples() : 0;
        return oversampling_latency + (has_cabinet() ? cabinet_partition_ : 0);
    }

    const char* Peavey5150::type_id() const { return "sushidsp.peavey5150"; }

    bool Peavey5150::set_cabinet(Ir impulse_response)
    {
        // cabinet_partition_ is 0 until prepare() has run with a real block size, and a
        // zero-partition StreamingConvolution has empty FIFOs that process() would index out of
        // bounds. Rejected outright rather than stashed: a false return means nothing was
        // installed and nothing was armed.
        if (impulse_response.samples.empty() || cabinet_partition_ == 0 || scratch_base_.empty()
            || impulse_response.sample_rate != sample_rate_)
        {
            return false;
        }
        cabinet_impulse_response_ = std::move(impulse_response.samples);
        cabinet_slot_.publish(std::make_unique<StreamingConvolution>(
            cabinet_partition_, cabinet_impulse_response_.data(),
            static_cast<std::uint32_t>(cabinet_impulse_response_.size()),
            static_cast<std::uint32_t>(scratch_base_.size())));
        return true;
    }

    void Peavey5150::reclaim_cabinets() { cabinet_slot_.reclaim(); }

    bool Peavey5150::has_cabinet() const { return cabinet_slot_.has_active(); }

    PortLayout Peavey5150::ports() const { return { 1, 1 }; }

    void Peavey5150::reset()
    {
        Biquad* const filters[] = {
            &input_low_pass_, &c6_coupling_, &pre_gain_peaker_, &c2_coupling_, &c1_coupling_,
            &c48_low_pass_, &c57_coupling_, &c56_coupling_, &c52_coupling_, &c11_coupling_,
            &c49_coupling_, &v1a_cathode_shelf_, &v1b_cathode_shelf_, &v2b_cathode_shelf_,
            &v3a_cathode_shelf_, &v1b_grid_miller_, &v2a_grid_miller_, &v2b_grid_miller_,
            &v5b_grid_miller_, &v5a_grid_miller_, &v3a_grid_miller_, &phase_inverter_grid_miller_,
            &presence_shelf_
        };
        for (Biquad* filter : filters)
        {
            filter->reset();
        }

        TriodeStage* const triodes[] = { &v1a_, &v1b_, &v2a_, &v2b_, &v5b_, &v5a_, &v3a_, &phase_inverter_ };
        for (TriodeStage* triode : triodes)
        {
            triode->reset();
        }

        tone_stack_.reset();
        power_tube_a_.reset();
        power_tube_b_.reset();
        output_transformer_.reset();
        sag_.reset();
        resonance_shelf_.reset();
        if (oversampler_)
        {
            oversampler_->reset();
        }
        if (StreamingConvolution* const cabinet = cabinet_slot_.active_for_publisher())
        {
            cabinet->reset();
        }
    }

    void Peavey5150::set_pre_gain(float normalized) { pre_gain_.set_target(std::clamp(normalized, 0.0f, 1.0f)); }
    void Peavey5150::set_low(float normalized) { low_.set_target(std::clamp(normalized, 0.0f, 1.0f)); }
    void Peavey5150::set_mid(float normalized) { mid_.set_target(std::clamp(normalized, 0.0f, 1.0f)); }
    void Peavey5150::set_high(float normalized) { high_.set_target(std::clamp(normalized, 0.0f, 1.0f)); }
    void Peavey5150::set_resonance(float normalized) { resonance_.set_target(std::clamp(normalized, 0.0f, 1.0f)); }
    void Peavey5150::set_presence(float normalized) { presence_.set_target(std::clamp(normalized, 0.0f, 1.0f)); }

    void Peavey5150::set_power_tube_grid_bias_mismatch_volts(float volts)
    {
        power_tube_grid_bias_mismatch_volts_ = volts;
    }

    void Peavey5150::set_post_gain(float decibels)
    {
        post_gain_db_ = std::clamp(decibels, kMinPostGainDb, kMaxPostGainDb);
        post_gain_.set_target(decibels_to_gain(post_gain_db_));
    }

    void Peavey5150::set_input_trim(float decibels)
    {
        input_trim_db_ = std::clamp(decibels, kMinInputTrimDb, kMaxInputTrimDb);
        input_trim_gain_.set_target(decibels_to_gain(input_trim_db_));
    }

    void Peavey5150::set_output(float decibels)
    {
        output_db_ = std::clamp(decibels, kMinOutputDb, kMaxOutputDb);
        output_gain_.set_target(decibels_to_gain(output_db_));
    }

    bool Peavey5150::tone_stack_is_dirty() const
    {
        return std::fabs(low_.target() - applied_low_) > kCoefficientEpsilon
            || std::fabs(mid_.target() - applied_mid_) > kCoefficientEpsilon
            || std::fabs(high_.target() - applied_high_) > kCoefficientEpsilon
            || std::fabs(presence_.target() - applied_presence_) > kCoefficientEpsilon
            || std::fabs(resonance_.target() - applied_resonance_) > kCoefficientEpsilon;
    }

    float Peavey5150::get_parameter(const char* id) const
    {
        if (std::strcmp(id, "pre_gain") == 0) return pre_gain_.target();
        if (std::strcmp(id, "low") == 0) return low_.target();
        if (std::strcmp(id, "mid") == 0) return mid_.target();
        if (std::strcmp(id, "high") == 0) return high_.target();
        if (std::strcmp(id, "post_gain") == 0) return post_gain_db_;
        if (std::strcmp(id, "resonance") == 0) return resonance_.target();
        if (std::strcmp(id, "presence") == 0) return presence_.target();
        if (std::strcmp(id, "input_trim") == 0) return input_trim_db_;
        if (std::strcmp(id, "output") == 0) return output_db_;
        return 0.0f;
    }

    void Peavey5150::set_parameter(const char* id, float value)
    {
        if (std::strcmp(id, "pre_gain") == 0) { set_pre_gain(value); return; }
        if (std::strcmp(id, "low") == 0) { set_low(value); return; }
        if (std::strcmp(id, "mid") == 0) { set_mid(value); return; }
        if (std::strcmp(id, "high") == 0) { set_high(value); return; }
        if (std::strcmp(id, "post_gain") == 0) { set_post_gain(value); return; }
        if (std::strcmp(id, "resonance") == 0) { set_resonance(value); return; }
        if (std::strcmp(id, "presence") == 0) { set_presence(value); return; }
        if (std::strcmp(id, "input_trim") == 0) { set_input_trim(value); return; }
        if (std::strcmp(id, "output") == 0) { set_output(value); return; }
    }

    void Peavey5150::set_parameter(std::uint16_t index, float value)
    {
        switch (index)
        {
        case 0: set_pre_gain(value); break;
        case 1: set_low(value); break;
        case 2: set_mid(value); break;
        case 3: set_high(value); break;
        case 4: set_post_gain(value); break;
        case 5: set_resonance(value); break;
        case 6: set_presence(value); break;
        case 7: set_input_trim(value); break;
        case 8: set_output(value); break;
        default: break;
        }
    }

    namespace
    {
        // The order is a contract: a host addresses these by index and a preset written by one
        // build is read back by another, so entries are appended - never reordered or removed.
        constexpr ParameterDescriptor kPeavey5150Parameters[] = {
            { "pre_gain",   "Pre Gain",   0.0f,             1.0f,             0.5f, ParamKind::Linear,  ""   },
            { "low",        "Low",        0.0f,             1.0f,             0.5f, ParamKind::Linear,  ""   },
            { "mid",        "Mid",        0.0f,             1.0f,             0.5f, ParamKind::Linear,  ""   },
            { "high",       "High",       0.0f,             1.0f,             0.5f, ParamKind::Linear,  ""   },
            { "post_gain",  "Post Gain",  kMinPostGainDb,   kMaxPostGainDb,   0.0f, ParamKind::Decibel, "dB" },
            { "resonance",  "Resonance",  0.0f,             1.0f,             0.5f, ParamKind::Linear,  ""   },
            { "presence",   "Presence",   0.0f,             1.0f,             0.5f, ParamKind::Linear,  ""   },
            { "input_trim", "Input Trim", kMinInputTrimDb,  kMaxInputTrimDb,  0.0f, ParamKind::Decibel, "dB" },
            { "output",     "Output",     kMinOutputDb,     kMaxOutputDb,     0.0f, ParamKind::Decibel, "dB" },
        };

        std::unique_ptr<IAudioNode> make_peavey5150(const PrepareInfo&)
        {
            return std::make_unique<Peavey5150>();
        }
    }

    const NodeDescriptor& peavey5150_descriptor()
    {
        static const NodeDescriptor descriptor{
            "sushidsp.peavey5150",
            "5150",
            kPeavey5150Parameters,
            9,
            PortLayout{ 1, 1 },
            &make_peavey5150,
        };
        return descriptor;
    }
}
