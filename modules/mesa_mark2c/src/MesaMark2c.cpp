/**************************************************************************/
/* MesaMark2c.cpp                                                         */
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

/**
 * @file MesaMark2c.cpp
 * @brief The Mark IIC+ lead chain's constants, its per-sample order and its descriptor.
 * @author Mustafa Garip & Sushi Systems
 */

// _USE_MATH_DEFINES must precede every include: it is what makes MSVC's <cmath> declare M_PI.
#define _USE_MATH_DEFINES

#include "SushiDSP/mesa_mark2c/MesaMark2c.hpp"

#include "SushiDSP/circuits/stages/PowerTubePresets.hpp"

#include <algorithm>
#include <array>
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
        /// @brief Small-signal gain of one triode, from the dossier's own mu and rp arithmetic.
        constexpr float kV1aGain = 70.6f;        // Dossier 3.1, 100 x 150 / 212.5.
        constexpr float kV1bGain = 61.5f;        // Dossier 3.2, 100 x 100 / 162.5.
        constexpr float kV3bGain = 56.7f;        // Dossier 3.3, 100 x 82 / 144.5.
        constexpr float kV4aGain = 70.0f;        // Dossier 3.4, loaded by the output divider.
        /// @brief V2A's gain into the loop network's 58.746 k AC load with its 1.5 k cathode
        /// unbypassed: 91.561 x 58.746 / (58.746 + 46.848 + 92.561 x 1.5) on the Koren model's
        /// local mu and r_a at 1.2902 mA; dossier 3.6.
        constexpr float kV2aGain = 22.005f;
        constexpr float kV2bGain = 73.118f;      // Dossier 3.7, computed here: 100 x 170 / 232.5.

        /// @brief One inverter triode's full common-cathode gain on its own load line, the pair's
        /// split being carried by kPhaseInverterDegeneration instead: mu x Ra / (Ra + ra) is
        /// 96.378299 x 82000 / (82000 + 34061.086) = 68.093628, with dossier 5.1's 82 k plate. The
        /// mu is the local gm x ra from the Koren Jacobian at Vgk -1.5 V, Vpk 284.5 V, as
        /// Jcm800.cpp does, not the model's nominal 100.
        /// That point is dossier 5.1's printed operating point rather than a solve on the 82 k
        /// load line, where the Koren model draws 2.946 mA against the line's 1.293 mA and
        /// dossier question 6 carries the disagreement; solving it gives gain 63.6 or 57.2 with
        /// degeneration 2.226 or 2.183.
        constexpr float kPhaseInverterGain = 68.093628f;

        /// @brief How much wider the inverter's input is than the grid-cathode window it opens, the
        /// shared cathode following the driven grid through the tail: 1 + (Rtail || Zshunt) / Zk.
        /// Zk is the driven triode's own cathode impedance (ra + Ra) / (mu + 1) = 1191.858 Ohm,
        /// Zshunt adds dossier 5.1's 470 Ohm drawn between the cathodes for 1661.858 Ohm, and Rtail
        /// is 5.1's shared 15 k tail glyph: 15000 || 1661.858 = 1496.104, so the factor is
        /// 1 + 1496.104 / 1191.858 = 2.2552703, always divided by. ra is 34061.086 Ohm and mu the
        /// local 96.378299, both from the Koren Jacobian at Vgk -1.5 V, Vpk 284.5 V.
        constexpr float kPhaseInverterDegeneration = 2.2552703f;

        /// @brief The DC operating point each triode's grid rests at, from the printed cathode volts.
        constexpr float kV1aCathodeBias = -1.6f;   // Dossier 3.1, printed cathode voltage.
        constexpr float kV1bCathodeBias = -1.9f;   // Dossier 3.2, printed cathode voltage.
        constexpr float kV3bCathodeBias = -1.8f;   // Dossier 3.3, resolved by elimination.
        constexpr float kV4aCathodeBias = -1.2f;   // Dossier 3.4, printed cathode voltage.
        constexpr float kV2aCathodeBias = -1.9353f;  // Dossier 3.6, 1.2902 mA self-biased through the unbypassed 1.5 k.
        constexpr float kV2bCathodeBias = -1.2f;   // Dossier 3.7, 1.2 mA through the 1 k cathode.
        constexpr float kPhaseInverterCathodeBias = -1.5f;  // Dossier 8 question 6, not established.

        /// @brief The Thevenin source resistance each grid is driven from.
        constexpr float kV1aSourceResistance = 10000.0f;   // Dossier 3.1 draws no stopper; the JCM800's figure.
        constexpr float kV1bSourceResistance = 250000.0f;  // Dossier 3.2, the 1 M Volume 1 wiper at its worst case.
        constexpr float kV3bSourceResistance = 250000.0f;  // Dossier 3.3, the 1 M Lead Drive wiper at its worst case.
        constexpr float kV4aSourceResistance = 48571.0f;   // Dossier 3.4, 170 k in parallel with 68 k.
        constexpr float kV2aSourceResistance = 3251.2f;    // Dossier 3.5 and 3.6, the lead output divider's 220 k || 3.3 k.
        constexpr float kV2bSourceResistance = 5909.0f;    // Dossier 3.6a, the loop network's Thevenin at V2B's grid, loop normalled.
        constexpr float kPhaseInverterSourceResistance = 25000.0f;  // Dossier 3.7, the 100 k Master 1 wiper at its worst case.

        /// @brief The grid leak every triode's solver pulls toward its bias point.
        constexpr float kInputGridLeak = 1000000.0f;  // Dossier 3.1, 1 M at the input jack.
        constexpr float kTriodeGridLeak = 1000000.0f; // A modelling choice: no other grid leak is drawn.
        constexpr float kV2aGridLeak = 680000.0f;     // Dossier 3.6, the 680 k grid shunt (S8 R11).

        /// @brief Interstage attenuations the dossier states as ratios.
        constexpr float kLeadDriveDivider = 0.595f;      // Dossier 3.3, 1 M / 1.68 M.
        constexpr float kLeadOutputDivider = 0.014778f;  // Dossier 3.5, 3.3 / 223.3 at DC, the shunt as traced (S8 R30); question 21.
        constexpr float kLeadOutputDividerZeroHz = 2893.7f;  // Dossier 3.5, computed from 220 k and 250 pF.
        constexpr float kLeadOutputDividerPoleHz = 65269.6f; // Dossier 3.5, computed: 1 / (2 pi x (220 k || 3.3 k) x 750 pF).

        /// @brief The effects loop network between V2A's plate and V2B's grid, loop normalled;
        /// dossier 3.6a. The series 47 k (R105) works against node W, which the Lead Master
        /// rheostat shunts in parallel with the 150 k (R102) into P's 4273 ohm shunt; the two
        /// fixed dividers are W to P, 4273 / (150 k + 4273), and P to the grid, 47 k || 1 M over
        /// 2.2 k (R12) plus that.
        constexpr float kLoopSeriesOhms = 47000.0f;
        constexpr float kLoopShuntToWOhms = 154273.0f;
        constexpr float kLoopWToPDivider = 0.02770f;
        constexpr float kLoopPToGridDivider = 0.9533f;
        constexpr float kLeadMasterPotOhms = 250000.0f;  // Dossier 3.6a, the 250 k rheostat from W through LDR4 to ground.

        /// @brief Coupling-capacitor corners, computed from the dossier's R and C. Each is one
        /// capacitor into one resistance, so each is a first-order section rather than a biquad.
        constexpr double kV1bCouplingHz = 15.92;   // Dossier 3.2, 0.1 uF into the 100 k shunt.
        constexpr double kV3bCouplingHz = 33.44;   // Dossier 3.4, 0.02 uF into 170 k + 68 k.
        constexpr double kV4aCouplingHz = 10.58;   // Dossier 3.5, 0.047 uF into 220 k + 100 k.
        constexpr double kV2bCouplingHz = 33.86;   // Dossier 3.7, 0.047 uF into the 100 k Master 1 pot.

        /// @brief The loop's coupling capacitor C9, 0.047 uF, into V2A's 65.0 k output resistance
        /// plus the 142.4 k the network hangs on it at Lead Master maximum; dossier 3.6a.
        constexpr double kV2bLoopReturnCouplingHz = 16.33;

        /// @brief The V3B-to-V4A interstage network, as a zero, a pole and the loss below the pole.
        constexpr double kV4aInterstageZeroHz = 19.939;   // Dossier 3.4, computed: 69.717 x 0.286.
        constexpr double kV4aInterstagePoleHz = 69.717;   // Dossier 3.4, 0.047 uF against 170 k in parallel with 68 k.
        constexpr double kV4aInterstageLowGain = 0.286;   // Dossier 3.4, 68 / (170 + 68) below the pole, unity above.

        /// @brief The partially bypassed cathodes, as a zero, a pole and the loss below the zero.
        constexpr double kV4aCathodeZeroHz = 219.22;   // Dossier 3.4, 3.3 k with 0.22 uF.
        constexpr double kV4aCathodePoleHz = 438.97;   // Dossier 3.4, computed from 3.3 k against (Rp + rp) / (mu + 1).
        constexpr double kV4aCathodeLowGain = 0.4994;  // Dossier 3.4, computed: -6.03 dB below the zero.
        constexpr double kV2bCathodeZeroHz = 338.63;   // Dossier 3.7, 1 k with 0.47 uF, PULL DEEP out.
        constexpr double kV2bCathodePoleHz = 485.73;   // Dossier 3.7, computed the same way as V4A's.
        constexpr double kV2bCathodeLowGain = 0.6972;  // Dossier 3.7, computed: -3.13 dB below the zero.

        /// @brief The Lead Drive bright cap, taken at the 470 k reading of its series resistor.
        constexpr double kLeadDriveBrightHz = 1881.3;    // Dossier 3.3, computed from 180 pF and 470 k.
        constexpr float kLeadDriveBrightMaxDb = 18.0f;   // A modelling choice: the shelf's ceiling.

        /// @brief The presence network from the 8 ohm tap.
        constexpr double kPresenceHz = 1063.9;   // Dossier 5.2, computed from 6.8 k and 0.022 uF.
        constexpr float kMaxPresenceDb = 9.0f;   // A modelling choice: the dossier states no depth.

        /// @brief The five graphic-equaliser traps and the range the sliders sweep.
        constexpr float kEqualizerCentresHz[MesaMark2c::kEqualizerBandCount] = {
            71.5f, 371.8f, 979.5f, 1576.0f, 4823.0f   // Dossier 4.2, f0 = 1 / (2 pi sqrt(LC)).
        };
        constexpr float kEqualizerBandQ[MesaMark2c::kEqualizerBandCount] = {
            1.434f, 1.938f, 1.571f, 0.673f, 1.0f      // Dossier 4.2, Q = (1/R) sqrt(L/C).
        };
        constexpr float kEqualizerMaxGainDb = 17.0f;  // Dossier 4.2, +17 / -18 dB modelled symmetric.

        /// @brief The output section's fixed constants.
        constexpr float kSimulClassPairWeight = 0.5f;  // A modelling choice: both pairs sum equally.
        /// @brief The level the output transformer stage receives at both pairs' full clip: the
        /// (2 x 20.408 + 2 x 14.493) V x 0.5 x 0.05 this chain delivered before PentodeStage
        /// became volt-true, a modelling choice adopted from modules/jcm800/src/Jcm800.cpp.
        constexpr float kTransformerFullClipUnits = 1.745265f;

        /// @brief The output trim from the power-tube plates to the transformer's units: the two
        /// pairs' full swings, each preset's two headrooms summed and weighted by
        /// kSimulClassPairWeight, land at kTransformerFullClipUnits.
        constexpr float kPowerTubeToTransformerAttenuation = kTransformerFullClipUnits
            / (kSimulClassPairWeight
               * (kMesaMark2cPower6L6GC.plate_swing.headroom_to_supply_volts
                  + kMesaMark2cPower6L6GC.plate_swing.headroom_to_bottom_volts
                  + kMesaMark2cPowerEl34Triode.plate_swing.headroom_to_supply_volts
                  + kMesaMark2cPowerEl34Triode.plate_swing.headroom_to_bottom_volts));
        constexpr float kOutputTransformerRolloffHz = 60.0f;   // The JCM800's constant; dossier 8 question 3.
        constexpr float kOutputTransformerLeakageHz = 8000.0f; // The JCM800's constant; dossier 8 question 3.
        constexpr float kOutputTransformerSaturationDrive = 0.3f;  // The JCM800's constant; dossier 8 question 3.
        constexpr float kSagAmount = 0.3f;                     // The JCM800's constant; no IIC+ figure exists.
        constexpr float kSagRecoveryTimeSeconds = 0.05f;       // The JCM800's constant; no IIC+ figure exists.

        // 12AX7 Miller input capacitance, the JCM800's constants: C_in = Cgk + Cgp * (1 + |A|)
        // plus stray wiring, driven from the preceding stage's own plate load.
        constexpr float kMillerCgkPicofarads = 1.6f;    // The 12AX7 Koren model's grid-cathode capacitance.
        constexpr float kMillerCgpPicofarads = 1.7f;    // The 12AX7 Koren model's grid-plate capacitance.
        constexpr float kStrayWiringPicofarads = 10.0f; // Point-to-point wiring, the JCM800's assumption.
        constexpr float kV1aPlateLoadOhms = 150000.0f;  // Dossier 3.1, 150 k to node D.
        constexpr float kV1bPlateLoadOhms = 100000.0f;  // Dossier 3.2, 100 k to node D.
        constexpr float kV3bPlateLoadOhms = 82000.0f;   // Dossier 3.3, 82 k to node C.
        constexpr float kV4aPlateLoadOhms = 270000.0f;  // Dossier 3.4, 270 k to node C.
        constexpr float kV2aPlateLoadOhms = 100000.0f;  // Dossier 3.6, 100 k to node C.
        constexpr float kV2bPlateLoadOhms = 170000.0f;  // Dossier 3.7, 170 k to node C.

        /// @brief V1A's plate swing: node D 382 V minus the 222 V idle plate on the cutoff side,
        /// and 222 V minus the 101.039 V the plate reaches at Vgk = 0 on the 150 k load line.
        constexpr PlateSwing kV1aPlateSwing{ 160.0f, 120.961f };

        /// @brief V1B's plate swing: 382 - 255 V toward node D, and 255 - 126.149 V down the
        /// 100 k load line to Vgk = 0.
        constexpr PlateSwing kV1bPlateSwing{ 127.0f, 128.851f };

        /// @brief V3B's plate swing: 392 - 294 V toward node C, and 294 - 142.446 V down the
        /// 82 k load line to Vgk = 0.
        constexpr PlateSwing kV3bPlateSwing{ 98.0f, 151.554f };

        /// @brief V4A's plate swing: 392 - 294 V toward node C, and 294 - 72.784 V down the
        /// 270 k load line to Vgk = 0.
        constexpr PlateSwing kV4aPlateSwing{ 98.0f, 221.216f };

        /// @brief V2A's plate swing on the 58.746 k AC load line at Lead Master maximum: 1.2902 mA
        /// x 58.746 k toward the supply, and 263.0 V down to Vgk = 0; dossier 3.6. The stage holds
        /// both at the knob's maximum (docs/superpowers/specs/2026-09-08-iicplus-loop-network-design.md
        /// section 5).
        constexpr PlateSwing kV2aPlateSwing{ 75.795f, 114.171f };

        /// @brief V2B's plate swing: 392 - 188 V toward node C, and 188 - 95.662 V down the
        /// 170 k load line to Vgk = 0; the cold, low-headroom stage of dossier 3.7.
        constexpr PlateSwing kV2bPlateSwing{ 204.0f, 92.338f };

        /// @brief The inverter's plate swing: 392 - 286 V toward node C, and 286 - 142.232 V down
        /// the 82 k load line to Vgk = 0.
        constexpr PlateSwing kPhaseInverterPlateSwing{ 106.0f, 143.768f };

        constexpr double kFilterQ = 0.707;
        constexpr float kSmoothingMs = 20.0f;

        // Below this a recomputed coefficient set is indistinguishable from the one in place, so
        // the audio thread skips the rebuild.
        constexpr float kCoefficientEpsilon = 1e-4f;

        constexpr float kMinMasterDb = -60.0f;
        constexpr float kMaxMasterDb = 12.0f;
        constexpr float kMinInputTrimDb = -24.0f;
        constexpr float kMaxInputTrimDb = 24.0f;
        constexpr float kMinOutputDb = -60.0f;
        constexpr float kMaxOutputDb = 12.0f;

        /// @brief The presence shelf's gain at one knob position.
        /// @param normalized Presence position in [0, 1]; 0.5 is flat.
        /// @return The shelf gain in decibels.
        float presence_gain_db(float normalized)
        {
            return (normalized - 0.5f) * 2.0f * kMaxPresenceDb;
        }

        /// @brief The corner of one grid's Miller input-capacitance pole.
        /// @param stage_voltage_gain The driven stage's mid-band gain.
        /// @param plate_load_ohms The driving stage's plate resistor.
        /// @return The -3 dB corner of the grid's RC pole, in Hz.
        double miller_corner_hz(double stage_voltage_gain, double plate_load_ohms)
        {
            const double input_capacitance_farads =
                (kMillerCgkPicofarads + kMillerCgpPicofarads * (1.0 + stage_voltage_gain)
                 + kStrayWiringPicofarads) * 1e-12;
            return 1.0 / (2.0 * M_PI * plate_load_ohms * input_capacitance_farads);
        }

        /// @brief The loop network's gain from V2A's coupled plate to V2B's grid at one Lead
        /// Master position: the rheostat in parallel with the fixed load on W against the series
        /// 47 k, times the two fixed dividers; dossier 3.6a.
        /// @param lead_master Lead Master position in [0, 1]; 0 is the rheostat at 0 ohm, a mute.
        /// @return The linear gain from the coupling capacitor's node to V2B's grid.
        float loop_network_gain(float lead_master);

        /// @brief The audio-taper law the JCM800's preamp_attenuation() uses, without its ceiling.
        /// @param normalized Pot position in [0, 1].
        /// @return The pot's voltage division at that position.
        float audio_taper(float normalized)
        {
            return normalized * std::sqrt(normalized);
        }

        float loop_network_gain(float lead_master)
        {
            const float pot_ohms = kLeadMasterPotOhms * audio_taper(lead_master);
            const float w_ohms = pot_ohms * kLoopShuntToWOhms / (pot_ohms + kLoopShuntToWOhms);
            return (w_ohms / (kLoopSeriesOhms + w_ohms)) * kLoopWToPDivider * kLoopPToGridDivider;
        }
    }

    MesaMark2c::MesaMark2c(std::vector<float> cabinet_impulse_response)
        : cabinet_impulse_response_(std::move(cabinet_impulse_response))
        , v1a_(kV1aSourceResistance, kInputGridLeak, kV1aCathodeBias, kV1aGain, kTriode12AX7GridConduction, kV1aPlateSwing)
        , tone_stack_(48000.0 * kOversamplingFactor)
        , v1b_(kV1bSourceResistance, kTriodeGridLeak, kV1bCathodeBias, kV1bGain, kTriode12AX7GridConduction, kV1bPlateSwing)
        , v3b_(kV3bSourceResistance, kTriodeGridLeak, kV3bCathodeBias, kV3bGain, kTriode12AX7GridConduction, kV3bPlateSwing)
        , v4a_(kV4aSourceResistance, kTriodeGridLeak, kV4aCathodeBias, kV4aGain, kTriode12AX7GridConduction, kV4aPlateSwing)
        , v2a_(kV2aSourceResistance, kV2aGridLeak, kV2aCathodeBias, kV2aGain, kTriode12AX7GridConduction, kV2aPlateSwing)
        , v2b_(kV2bSourceResistance, kTriodeGridLeak, kV2bCathodeBias, kV2bGain, kTriode12AX7GridConduction, kV2bPlateSwing)
        , equalizer_bank_(48000.0 * kOversamplingFactor,
                          { kEqualizerCentresHz[0], kEqualizerCentresHz[1], kEqualizerCentresHz[2],
                            kEqualizerCentresHz[3], kEqualizerCentresHz[4] },
                          { kEqualizerBandQ[0], kEqualizerBandQ[1], kEqualizerBandQ[2],
                            kEqualizerBandQ[3], kEqualizerBandQ[4] },
                          kEqualizerMaxGainDb)
        , phase_inverter_(kPhaseInverterSourceResistance, kTriodeGridLeak, kPhaseInverterCathodeBias, kPhaseInverterGain, kTriode12AX7GridConduction,
                          kPhaseInverterPlateSwing)
        , power_inner_a_(kMesaMark2cPower6L6GC)
        , power_inner_b_(kMesaMark2cPower6L6GC)
        , power_outer_a_(kMesaMark2cPowerEl34Triode)
        , power_outer_b_(kMesaMark2cPowerEl34Triode)
        , output_transformer_(48000.0 * kOversamplingFactor, kOutputTransformerRolloffHz, kOutputTransformerLeakageHz, kOutputTransformerSaturationDrive)
        , sag_(48000.0 * kOversamplingFactor, kSagAmount, kSagRecoveryTimeSeconds)
    {
        build_rate_dependent_filters(48000.0 * kOversamplingFactor);
    }

    void MesaMark2c::build_rate_dependent_filters(double oversampled_rate_hz)
    {
        v1b_coupling_.set_first_order_high_pass(oversampled_rate_hz, kV1bCouplingHz);
        v3b_coupling_.set_first_order_high_pass(oversampled_rate_hz, kV3bCouplingHz);
        // S1 draws 68 k in parallel with 0.047 uF to ground, which as drawn is a 69.7 Hz
        // low-pass no lead channel could have; the model places the capacitor across the 170 k,
        // which gives the tightness dossier 3.4's prose describes; open question 18.
        v4a_interstage_shelf_.set_first_order_shelf(oversampled_rate_hz, kV4aInterstageZeroHz, kV4aInterstagePoleHz, kV4aInterstageLowGain);
        v4a_coupling_.set_first_order_high_pass(oversampled_rate_hz, kV4aCouplingHz);
        v2b_loop_return_coupling_.set_first_order_high_pass(oversampled_rate_hz, kV2bLoopReturnCouplingHz);
        v2b_coupling_.set_first_order_high_pass(oversampled_rate_hz, kV2bCouplingHz);
        v4a_cathode_shelf_.set_first_order_shelf(oversampled_rate_hz, kV4aCathodeZeroHz, kV4aCathodePoleHz, kV4aCathodeLowGain);
        v2b_cathode_shelf_.set_first_order_shelf(oversampled_rate_hz, kV2bCathodeZeroHz, kV2bCathodePoleHz, kV2bCathodeLowGain);
        lead_output_divider_.set_first_order_shelf(oversampled_rate_hz, kLeadOutputDividerZeroHz, kLeadOutputDividerPoleHz, kLeadOutputDivider);

        // The Miller corners land at 9.0, 14.5, 14.7, 11.6, 11.6 and 15.2 kHz, each set by the
        // plate load that drives the grid rather than by one shared figure.
        v1b_grid_miller_.set_one_pole_low_pass(oversampled_rate_hz, miller_corner_hz(kV1bGain, kV1aPlateLoadOhms));
        v3b_grid_miller_.set_one_pole_low_pass(oversampled_rate_hz, miller_corner_hz(kV3bGain, kV1bPlateLoadOhms));
        v4a_grid_miller_.set_one_pole_low_pass(oversampled_rate_hz, miller_corner_hz(kV4aGain, kV3bPlateLoadOhms));
        // V2A's pole uses V4A's 270 k, the load that drives the grid, while kV2aSourceResistance
        // is the divider's 3.25 k Thevenin that sets where that grid conducts.
        v2a_grid_miller_.set_one_pole_low_pass(oversampled_rate_hz, miller_corner_hz(kV2aGain, kV4aPlateLoadOhms));
        // V2B's corner from the loop's 5.9 k is near 196 kHz; the 100 k pole here is a placeholder the band does not reach.
        v2b_grid_miller_.set_one_pole_low_pass(oversampled_rate_hz, miller_corner_hz(kV2bGain, kV2aPlateLoadOhms));
        phase_inverter_grid_miller_.set_one_pole_low_pass(oversampled_rate_hz, miller_corner_hz(kPhaseInverterGain, kV2bPlateLoadOhms));
    }

    void MesaMark2c::prepare(const PrepareInfo& info)
    {
        sample_rate_ = info.sample_rate;
        const double oversampled_rate_hz = sample_rate_ * kOversamplingFactor;

        build_rate_dependent_filters(oversampled_rate_hz);
        presence_shelf_.set_high_shelf(sample_rate_, kPresenceHz, presence_gain_db(presence_.target()), kFilterQ);

        // The tone stack and the equaliser sit inside the single oversampled region, so their
        // coefficients are derived at the oversampled rate.
        tone_stack_ = MesaMark2cToneStack(oversampled_rate_hz);
        equalizer_bank_ = GraphicEqualizer(oversampled_rate_hz,
                                           { kEqualizerCentresHz[0], kEqualizerCentresHz[1], kEqualizerCentresHz[2],
                                             kEqualizerCentresHz[3], kEqualizerCentresHz[4] },
                                           { kEqualizerBandQ[0], kEqualizerBandQ[1], kEqualizerBandQ[2],
                                             kEqualizerBandQ[3], kEqualizerBandQ[4] },
                                           kEqualizerMaxGainDb);

        SmoothedParameter* const smoothed[] = {
            &volume_1_, &treble_, &bass_, &middle_, &lead_drive_, &lead_master_, &presence_,
            &equalizer_[0], &equalizer_[1], &equalizer_[2], &equalizer_[3], &equalizer_[4],
            &master_gain_, &input_trim_gain_, &output_gain_
        };
        for (SmoothedParameter* parameter : smoothed)
        {
            parameter->prepare(sample_rate_, kSmoothingMs);
            parameter->snap(parameter->target());
        }
        applied_bass_ = -1.0f;
        applied_middle_ = -1.0f;
        applied_treble_ = -1.0f;
        applied_presence_ = -1.0f;
        applied_lead_drive_ = -1.0f;
        for (std::size_t band = 0; band < kEqualizerBandCount; ++band)
        {
            applied_equalizer_[band] = -2.0f;
        }

        output_transformer_ = OutputTransformerStage(oversampled_rate_hz, kOutputTransformerRolloffHz, kOutputTransformerLeakageHz, kOutputTransformerSaturationDrive);
        sag_ = PowerSupplySagStage(oversampled_rate_hz, kSagAmount, kSagRecoveryTimeSeconds);

        oversampler_ = std::make_unique<Oversampler8x>(info.max_block);

        scratch_base_.assign(info.max_block, 0.0f);
        scratch_oversampled_.assign(static_cast<std::size_t>(info.max_block) * kOversamplingFactor, 0.0f);

        cabinet_partition_ = StreamingConvolution::partition_size_for(sample_rate_);
        if (!cabinet_impulse_response_.empty())
        {
            cabinet_slot_.publish(std::make_unique<StreamingConvolution>(
                cabinet_partition_, cabinet_impulse_response_.data(),
                static_cast<std::uint32_t>(cabinet_impulse_response_.size()), info.max_block));
        }

        reset();
    }

    void MesaMark2c::process(ProcessContext& context)
    {
        const float* input = context.inputs[0];
        float* output = context.outputs[0];
        const std::uint32_t frames = context.frames;
        const std::uint32_t oversampled_frames = frames * kOversamplingFactor;

        const float volume_1 = volume_1_.skip(frames);
        const float bass = bass_.skip(frames);
        const float middle = middle_.skip(frames);
        const float treble = treble_.skip(frames);
        const float lead_drive = lead_drive_.skip(frames);
        const float lead_master = lead_master_.skip(frames);
        const float presence = presence_.skip(frames);
        const float input_gain = input_trim_gain_.skip(frames);
        const float master = master_gain_.skip(frames);
        const float output_gain = output_gain_.skip(frames);

        if (std::fabs(bass - applied_bass_) > kCoefficientEpsilon)
        {
            tone_stack_.set_bass(bass);
            applied_bass_ = bass;
        }
        if (std::fabs(middle - applied_middle_) > kCoefficientEpsilon)
        {
            tone_stack_.set_middle(middle);
            applied_middle_ = middle;
        }
        if (std::fabs(treble - applied_treble_) > kCoefficientEpsilon)
        {
            tone_stack_.set_treble(treble);
            applied_treble_ = treble;
        }
        if (std::fabs(presence - applied_presence_) > kCoefficientEpsilon)
        {
            presence_shelf_.set_high_shelf(sample_rate_, kPresenceHz, presence_gain_db(presence), kFilterQ);
            applied_presence_ = presence;
        }
        if (std::fabs(lead_drive - applied_lead_drive_) > kCoefficientEpsilon)
        {
            // The 180 pF shorts the pot's top section above the corner, so the shelf's height is
            // whatever the taper is throwing away at this position.
            const float bright_db = std::clamp(-gain_to_decibels(audio_taper(lead_drive)), 0.0f, kLeadDriveBrightMaxDb);
            lead_drive_bright_.set_high_shelf(sample_rate_ * kOversamplingFactor, kLeadDriveBrightHz, bright_db, kFilterQ);
            applied_lead_drive_ = lead_drive;
        }
        for (std::size_t band = 0; band < kEqualizerBandCount; ++band)
        {
            const float position = equalizer_[band].skip(frames);
            if (std::fabs(position - applied_equalizer_[band]) > kCoefficientEpsilon)
            {
                equalizer_bank_.set_band(band, position);
                applied_equalizer_[band] = position;
            }
        }

        const float volume_1_gain = audio_taper(volume_1);
        const float lead_drive_gain = audio_taper(lead_drive) * kLeadDriveDivider;
        const float loop_gain = loop_network_gain(lead_master);
        const float master_gain = master * MesaMark2c::kMasterUnityGain;

        for (std::uint32_t i = 0; i < frames; ++i)
        {
            scratch_base_[i] = input[i] * input_gain;
        }

        const float* upsampled = oversampler_->upsample(scratch_base_.data(), frames);
        for (std::uint32_t i = 0; i < oversampled_frames; ++i)
        {
            const float v1a_out = v1a_.process_sample(upsampled[i]);
            const float toned = tone_stack_.process_sample(v1a_out);
            const float v1b_out = v1b_.process_sample(v1b_grid_miller_.process_sample(toned * volume_1_gain));

            const float lead_input = lead_drive_bright_.process_sample(
                v1b_coupling_.process_sample(v1b_out)) * lead_drive_gain;
            const float v3b_out = v3b_.process_sample(v3b_grid_miller_.process_sample(lead_input));

            const float v4a_input = v4a_interstage_shelf_.process_sample(
                v3b_coupling_.process_sample(v3b_out));
            const float v4a_out = v4a_cathode_shelf_.process_sample(
                v4a_.process_sample(v4a_grid_miller_.process_sample(v4a_input)));

            const float v2a_input = lead_output_divider_.process_sample(
                v4a_coupling_.process_sample(v4a_out));
            const float v2a_out = v2a_.process_sample(v2a_grid_miller_.process_sample(v2a_input));
            const float v2b_out = v2b_cathode_shelf_.process_sample(
                v2b_.process_sample(v2b_grid_miller_.process_sample(
                    v2b_loop_return_coupling_.process_sample(v2a_out) * loop_gain)));

            // Master 1 sits at node F, between V2B's coupling capacitor and the equaliser, ahead of
            // the phase inverter (dossier 2).
            const float equalised = equalizer_bank_.process_sample(
                v2b_coupling_.process_sample(v2b_out) * master_gain);
            const float phase_inverted = phase_inverter_.process_sample(
                phase_inverter_grid_miller_.process_sample(equalised) / kPhaseInverterDegeneration);

            // Simul-Class: both pairs hang on the same transformer and see the same phase-inverted
            // grid signal, and they clip apart because their plate swings do (dossier 7.3).
            const float inner_pair = power_inner_a_.process_sample(phase_inverted)
                - power_inner_b_.process_sample(-phase_inverted);
            const float outer_pair = power_outer_a_.process_sample(phase_inverted)
                - power_outer_b_.process_sample(-phase_inverted);
            const float power_tube_out = (inner_pair + outer_pair) * kSimulClassPairWeight
                * kPowerTubeToTransformerAttenuation;

            const float transformer_out = output_transformer_.process_sample(power_tube_out);
            scratch_oversampled_[i] = sag_.process_sample(transformer_out);
        }
        oversampler_->downsample(scratch_oversampled_.data(), oversampled_frames, scratch_base_.data());

        for (std::uint32_t i = 0; i < frames; ++i)
        {
            scratch_base_[i] = presence_shelf_.process_sample(scratch_base_[i]);
        }

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

        // Nothing upstream is normalised to the DAC's range, so a hot Lead Drive, Master 1 and
        // Output combination can still land outside [-1, 1]; the clamp is a safety rail, not a
        // tone stage.
        for (std::uint32_t i = 0; i < frames; ++i)
        {
            output[i] = std::clamp(output[i] * output_gain, -1.0f, 1.0f);
        }
    }

    std::uint32_t MesaMark2c::latency_samples() const
    {
        const std::uint32_t oversampling_latency = oversampler_ ? oversampler_->latency_samples() : 0;
        return oversampling_latency + (has_cabinet() ? cabinet_partition_ : 0);
    }

    const char* MesaMark2c::type_id() const { return "sushidsp.mesa_mark2c"; }

    bool MesaMark2c::set_cabinet(Ir impulse_response)
    {
        if (impulse_response.samples.empty() || cabinet_partition_ == 0
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

    void MesaMark2c::reclaim_cabinets()
    {
        cabinet_slot_.reclaim();
    }

    bool MesaMark2c::has_cabinet() const
    {
        return cabinet_slot_.has_active();
    }

    namespace
    {
        /// @brief Collects the module's triodes in signal order, so the two probe accessors agree
        /// on the order and the count by construction.
        /// @param node The amp whose stages are listed.
        /// @return Pointers to V1A, V1B, V3B, V4A, V2A, V2B and the phase inverter.
        std::array<const TriodeStage*, 7> triode_stages_in_signal_order(
            const TriodeStage& v1a, const TriodeStage& v1b, const TriodeStage& v3b,
            const TriodeStage& v4a, const TriodeStage& v2a, const TriodeStage& v2b,
            const TriodeStage& phase_inverter)
        {
            return { &v1a, &v1b, &v3b, &v4a, &v2a, &v2b, &phase_inverter };
        }
    }

    std::size_t MesaMark2c::triode_stage_count() const
    {
        return triode_stages_in_signal_order(v1a_, v1b_, v3b_, v4a_, v2a_, v2b_, phase_inverter_)
            .size();
    }

    float MesaMark2c::grid_excursion_peak_volts(std::size_t stage) const
    {
        const std::array<const TriodeStage*, 7> stages =
            triode_stages_in_signal_order(v1a_, v1b_, v3b_, v4a_, v2a_, v2b_, phase_inverter_);
        if (stage >= stages.size())
        {
            return 0.0f;
        }
        return stages[stage]->peak_grid_excursion_volts();
    }

    PortLayout MesaMark2c::ports() const
    {
        return { 1, 1 };
    }

    void MesaMark2c::reset()
    {
        v1b_coupling_.reset();
        lead_drive_bright_.reset();
        v3b_coupling_.reset();
        v4a_interstage_shelf_.reset();
        v4a_cathode_shelf_.reset();
        v4a_coupling_.reset();
        lead_output_divider_.reset();
        v2b_cathode_shelf_.reset();
        v2b_loop_return_coupling_.reset();
        v2b_coupling_.reset();
        presence_shelf_.reset();
        v1b_grid_miller_.reset();
        v3b_grid_miller_.reset();
        v4a_grid_miller_.reset();
        v2a_grid_miller_.reset();
        v2b_grid_miller_.reset();
        phase_inverter_grid_miller_.reset();
        v1a_.reset();
        tone_stack_.reset();
        v1b_.reset();
        v3b_.reset();
        v4a_.reset();
        v2a_.reset();
        v2b_.reset();
        equalizer_bank_.reset();
        phase_inverter_.reset();
        power_inner_a_.reset();
        power_inner_b_.reset();
        power_outer_a_.reset();
        power_outer_b_.reset();
        output_transformer_.reset();
        sag_.reset();
        if (oversampler_)
        {
            oversampler_->reset();
        }
        if (StreamingConvolution* const cabinet = cabinet_slot_.active_for_publisher())
        {
            cabinet->reset();
        }
    }

    void MesaMark2c::set_volume_1(float normalized) { volume_1_.set_target(std::clamp(normalized, 0.0f, 1.0f)); }
    void MesaMark2c::set_treble(float normalized) { treble_.set_target(std::clamp(normalized, 0.0f, 1.0f)); }
    void MesaMark2c::set_bass(float normalized) { bass_.set_target(std::clamp(normalized, 0.0f, 1.0f)); }
    void MesaMark2c::set_middle(float normalized) { middle_.set_target(std::clamp(normalized, 0.0f, 1.0f)); }
    void MesaMark2c::set_lead_drive(float normalized) { lead_drive_.set_target(std::clamp(normalized, 0.0f, 1.0f)); }
    void MesaMark2c::set_lead_master(float normalized) { lead_master_.set_target(std::clamp(normalized, 0.0f, 1.0f)); }
    void MesaMark2c::set_presence(float normalized) { presence_.set_target(std::clamp(normalized, 0.0f, 1.0f)); }

    void MesaMark2c::set_equalizer_band(std::size_t band, float position)
    {
        if (band >= kEqualizerBandCount)
        {
            return;
        }
        equalizer_[band].set_target(std::clamp(position, -1.0f, 1.0f));
    }

    void MesaMark2c::set_master_1(float decibels)
    {
        master_1_db_ = std::clamp(decibels, kMinMasterDb, kMaxMasterDb);
        master_gain_.set_target(decibels_to_gain(master_1_db_));
    }

    void MesaMark2c::set_input_trim(float decibels)
    {
        input_trim_db_ = std::clamp(decibels, kMinInputTrimDb, kMaxInputTrimDb);
        input_trim_gain_.set_target(decibels_to_gain(input_trim_db_));
    }

    void MesaMark2c::set_output(float decibels)
    {
        output_db_ = std::clamp(decibels, kMinOutputDb, kMaxOutputDb);
        output_gain_.set_target(decibels_to_gain(output_db_));
    }

    bool MesaMark2c::tone_stack_is_dirty() const
    {
        for (std::size_t band = 0; band < kEqualizerBandCount; ++band)
        {
            if (std::fabs(equalizer_[band].target() - applied_equalizer_[band]) > kCoefficientEpsilon)
            {
                return true;
            }
        }
        return std::fabs(bass_.target() - applied_bass_) > kCoefficientEpsilon
            || std::fabs(middle_.target() - applied_middle_) > kCoefficientEpsilon
            || std::fabs(treble_.target() - applied_treble_) > kCoefficientEpsilon
            || std::fabs(presence_.target() - applied_presence_) > kCoefficientEpsilon
            || std::fabs(lead_drive_.target() - applied_lead_drive_) > kCoefficientEpsilon;
    }

    float MesaMark2c::get_parameter(const char* id) const
    {
        if (std::strcmp(id, "volume_1") == 0) return volume_1_.target();
        if (std::strcmp(id, "treble") == 0) return treble_.target();
        if (std::strcmp(id, "bass") == 0) return bass_.target();
        if (std::strcmp(id, "middle") == 0) return middle_.target();
        if (std::strcmp(id, "lead_drive") == 0) return lead_drive_.target();
        if (std::strcmp(id, "lead_master") == 0) return lead_master_.target();
        if (std::strcmp(id, "master_1") == 0) return master_1_db_;
        if (std::strcmp(id, "presence") == 0) return presence_.target();
        if (std::strcmp(id, "eq_80") == 0) return equalizer_[0].target();
        if (std::strcmp(id, "eq_240") == 0) return equalizer_[1].target();
        if (std::strcmp(id, "eq_750") == 0) return equalizer_[2].target();
        if (std::strcmp(id, "eq_2200") == 0) return equalizer_[3].target();
        if (std::strcmp(id, "eq_6600") == 0) return equalizer_[4].target();
        if (std::strcmp(id, "input_trim") == 0) return input_trim_db_;
        if (std::strcmp(id, "output") == 0) return output_db_;
        return 0.0f;
    }

    void MesaMark2c::set_parameter(const char* id, float value)
    {
        if (std::strcmp(id, "volume_1") == 0) { set_volume_1(value); return; }
        if (std::strcmp(id, "treble") == 0) { set_treble(value); return; }
        if (std::strcmp(id, "bass") == 0) { set_bass(value); return; }
        if (std::strcmp(id, "middle") == 0) { set_middle(value); return; }
        if (std::strcmp(id, "lead_drive") == 0) { set_lead_drive(value); return; }
        if (std::strcmp(id, "lead_master") == 0) { set_lead_master(value); return; }
        if (std::strcmp(id, "master_1") == 0) { set_master_1(value); return; }
        if (std::strcmp(id, "presence") == 0) { set_presence(value); return; }
        if (std::strcmp(id, "eq_80") == 0) { set_equalizer_band(0, value); return; }
        if (std::strcmp(id, "eq_240") == 0) { set_equalizer_band(1, value); return; }
        if (std::strcmp(id, "eq_750") == 0) { set_equalizer_band(2, value); return; }
        if (std::strcmp(id, "eq_2200") == 0) { set_equalizer_band(3, value); return; }
        if (std::strcmp(id, "eq_6600") == 0) { set_equalizer_band(4, value); return; }
        if (std::strcmp(id, "input_trim") == 0) { set_input_trim(value); return; }
        if (std::strcmp(id, "output") == 0) { set_output(value); return; }
    }

    void MesaMark2c::set_parameter(std::uint16_t index, float value)
    {
        switch (index)
        {
        case 0: set_volume_1(value); break;
        case 1: set_treble(value); break;
        case 2: set_bass(value); break;
        case 3: set_middle(value); break;
        case 4: set_lead_drive(value); break;
        case 5: set_lead_master(value); break;
        case 6: set_master_1(value); break;
        case 7: set_presence(value); break;
        case 8: set_equalizer_band(0, value); break;
        case 9: set_equalizer_band(1, value); break;
        case 10: set_equalizer_band(2, value); break;
        case 11: set_equalizer_band(3, value); break;
        case 12: set_equalizer_band(4, value); break;
        case 13: set_input_trim(value); break;
        case 14: set_output(value); break;
        default: break;
        }
    }

    namespace
    {
        // The order is a contract: a host addresses these by index and a saved preset reads them
        // back by index, so entries are appended, never reordered or removed.
        constexpr ParameterDescriptor kMesaMark2cParameters[] = {
            { "volume_1",    "Volume 1",    0.0f,            1.0f,            0.5f, ParamKind::Linear,  ""   },
            { "treble",      "Treble",      0.0f,            1.0f,            0.5f, ParamKind::Linear,  ""   },
            { "bass",        "Bass",        0.0f,            1.0f,            0.5f, ParamKind::Linear,  ""   },
            { "middle",      "Middle",      0.0f,            1.0f,            0.5f, ParamKind::Linear,  ""   },
            { "lead_drive",  "Lead Drive",  0.0f,            1.0f,            0.5f, ParamKind::Linear,  ""   },
            { "lead_master", "Lead Master", 0.0f,            1.0f,            0.5f, ParamKind::Linear,  ""   },
            { "master_1",    "Master 1",    kMinMasterDb,    kMaxMasterDb,    0.0f, ParamKind::Decibel, "dB" },
            { "presence",    "Presence",    0.0f,            1.0f,            0.5f, ParamKind::Linear,  ""   },
            { "eq_80",       "EQ 80",      -1.0f,            1.0f,            0.0f, ParamKind::Linear,  ""   },
            { "eq_240",      "EQ 240",     -1.0f,            1.0f,            0.0f, ParamKind::Linear,  ""   },
            { "eq_750",      "EQ 750",     -1.0f,            1.0f,            0.0f, ParamKind::Linear,  ""   },
            { "eq_2200",     "EQ 2200",    -1.0f,            1.0f,            0.0f, ParamKind::Linear,  ""   },
            { "eq_6600",     "EQ 6600",    -1.0f,            1.0f,            0.0f, ParamKind::Linear,  ""   },
            { "input_trim",  "Input Trim",  kMinInputTrimDb, kMaxInputTrimDb, 0.0f, ParamKind::Decibel, "dB" },
            { "output",      "Output",      kMinOutputDb,    kMaxOutputDb,    0.0f, ParamKind::Decibel, "dB" },
        };

        std::unique_ptr<IAudioNode> make_mesa_mark2c(const PrepareInfo&)
        {
            return std::make_unique<MesaMark2c>();
        }
    }

    const NodeDescriptor& mesa_mark2c_descriptor()
    {
        static const NodeDescriptor descriptor{
            "sushidsp.mesa_mark2c",
            "Mark IIC+",
            kMesaMark2cParameters,
            15,
            PortLayout{ 1, 1 },
            &make_mesa_mark2c,
        };
        return descriptor;
    }
}
