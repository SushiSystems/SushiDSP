/****************************************************************/
/* MesaMark2c.cpp                                               */
/* SushiDSP - https://github.com/SushiSystems/SushiDSP          */
/* Copyright (c) 2026 Sushi Systems                             */
/* Licensed under PolyForm Noncommercial 1.0.0. See LICENSE.    */
/* Commercial use requires a licence from Sushi Systems.        */
/****************************************************************/

/**
 * @file MesaMark2c.cpp
 * @brief The Mark IIC+ lead chain's constants, its per-sample order and its descriptor.
 * @author Mustafa Garip & Sushi Systems
 */

#include "SushiDSP/mesa_mark2c/MesaMark2c.hpp"

#include "SushiDSP/circuits/AudioTaperPot.hpp"
#include "SushiDSP/circuits/OutputSafetyClamp.hpp"
#include "SushiDSP/circuits/stages/MillerInputCapacitance.hpp"
#include "SushiDSP/circuits/stages/PowerTubePresets.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>
#include <iterator>
#include <utility>

namespace SushiDSP
{
    namespace
    {
    /// @brief Mesa Mark IIC+ Simul-Class inner pair, 6L6GC pentode: the RCA class AB1 point at its
    /// own 58 mA idle, the printed -45 V bias (modules/nodes/mesa_mark2c/README.md, "The output section").
    constexpr PowerTubePreset kMesaMark2cPower6L6GC{
        26642.4f, 220000.0f, -45.0f, 7.388139f, kPentodeEL34GridConduction, PlateSwing{ 81.2f, 311.2283f } };

    /// @brief Mesa Mark IIC+ Simul-Class outer pair, EL34 triode-strapped: Mesa's 25 W on the same
    /// primary, split evenly as a class A stage (modules/nodes/mesa_mark2c/README.md, "The output section").
    constexpr PowerTubePreset kMesaMark2cPowerEl34Triode{
        26642.4f, 220000.0f, -45.0f, 6.635772f, kPentodeEL34GridConduction, PlateSwing{ 132.2876f, 132.2876f } };

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

        /// @brief The a half's common-cathode gain, mu x Ra / (Ra + ra) on S8's 82.5 k plate, kept
        /// only to size the grid's Miller capacitance; LongTailedPair derives each half's own gain
        /// and the split the shared cathode imposes from kPhaseInverterMu and the plate loads.
        /// The operating point is dossier 5.1's printed one rather than a solve on the load line,
        /// where the Koren model draws 2.946 mA against the line's 1.293 mA and dossier question 6
        /// carries the disagreement.
        constexpr float kPhaseInverterGain = 68.214959f;

        /// @brief The DC operating point each triode's grid rests at, from the printed cathode volts.
        constexpr float kV1aCathodeBias = -1.6f;   // Dossier 3.1, printed cathode voltage.
        constexpr float kV1bCathodeBias = -1.9f;   // Dossier 3.2, printed cathode voltage.
        constexpr float kV3bCathodeBias = -1.8f;   // Dossier 3.3, resolved by elimination.
        constexpr float kV4aCathodeBias = -1.2f;   // Dossier 3.4, printed cathode voltage.
        constexpr float kV2aCathodeBias = -1.9353f;  // Dossier 3.6, 1.2902 mA self-biased through the unbypassed 1.5 k.
        constexpr float kV2bCathodeBias = -1.2f;   // Dossier 3.7, 1.2 mA through the 1 k cathode.
        constexpr float kPhaseInverterCathodeBias = -1.5f;  // Dossier 8 question 6, not established.

        /// @brief The Thevenin source resistance each grid is driven from.
        constexpr float kV1aSourceResistance = 10000.0f;   // Dossier 3.1 draws no stopper; a stand-in.
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

        /// @brief V2B's cathode with PULL DEEP in, the 15 uF beside the 0.47 uF.
        ///
        /// Both corners scale with the capacitance, 0.47 over 15.47, so the shelf keeps its depth
        /// and only moves down the band; dossier 3.7 gives the zero as 10.3 Hz.
        constexpr double kV2bCathodeDeepZeroHz = 10.288;
        constexpr double kV2bCathodeDeepPoleHz = 14.757;

        /// @brief The Lead Drive bright cap, taken at the 470 k reading of its series resistor.
        constexpr double kLeadDriveBrightHz = 1881.3;    // Dossier 3.3, computed from 180 pF and 470 k.

        /// @brief Volume 1's bright capacitor, the one PULL BRIGHT switches across the pot's top
        /// section. Its corner moves with the wiper, 1 M x (1 - position) against 180 pF, and is
        /// taken at the pot's midpoint the way the lead drive's is; dossier 2.
        constexpr double kVolume1BrightHz = 1768.4;
        constexpr float kVolume1BrightMaxDb = 18.0f;  // A modelling choice: the shelf's ceiling.
        constexpr float kLeadDriveBrightMaxDb = 18.0f;   // A modelling choice: the shelf's ceiling.

        /// @brief What one unit at the output transformer's secondary is worth to the feedback leg.
        ///
        /// The stage works in the units kTransformerFullClipUnits sets and the turns ratio that
        /// would convert them is dossier 8 question 3, unread, so this scalar carries the gap.
        /// This value is the deepest the loop runs without ringing, not a calibration: the loop
        /// closes around an output transformer whose corners are stand-ins, and their 8 kHz pole
        /// is what sets the loop's phase and what the amplifier oscillated at. The measurement and
        /// the ordering it forces are in docs/agent/2026_09_20_V0_1_0_RELEASE/REPORT.md.
        constexpr float kTapToFeedbackVolts = 14.0f;

        /// @brief The five graphic-equaliser traps and the range the sliders sweep.
        ///
        /// Read off S8, the traced schematic, not off the factory scan; the two drawings disagree
        /// on the 60 Hz and 750 Hz inductors. `docs/agent/2026_09_20_V0_1_0_RELEASE/REPORT.md`
        /// gives the readings and why S8 wins.
        constexpr float kEqualizerCentresHz[MesaMark2c::kEqualizerBandCount] = {
            87.61f, 371.74f, 723.43f, 1575.87f, 4822.88f  // f0 = 1 / (2 pi sqrt(LC)) on S8's L and C.
        };
        constexpr float kEqualizerBandQ[MesaMark2c::kEqualizerBandCount] = {
            1.171f, 1.938f, 2.128f, 0.673f, 1.0f          // Q = (1/R) sqrt(L/C) on the same values.
        };
        constexpr float kEqualizerMaxGainDb = 17.0f;  // Dossier 4.2, +17 / -18 dB modelled symmetric.

        /// @brief The output section's fixed constants.
        constexpr float kSimulClassPairWeight = 0.5f;  // A modelling choice: both pairs sum equally.
        /// @brief The level the output transformer stage receives at both pairs' full clip: the
        /// (2 x 20.408 + 2 x 14.493) V x 0.5 x 0.05 this chain delivered before PentodeStage
        /// became volt-true. The unit the transformer stage works in is a corpus convention, held
        /// across every amplifier so one transformer model serves them all.
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
        /// @brief The output transformer's two corners and its core drive. Neither sheet prints
        /// the primary impedance or the turns ratio, so all three are stand-ins, and the leakage
        /// corner is the one the feedback loop oscillated against; modules/nodes/mesa_mark2c/README.md
        /// lists them with every other figure the schematic does not carry.
        constexpr float kOutputTransformerRolloffHz = 60.0f;
        constexpr float kOutputTransformerLeakageHz = 8000.0f;
        constexpr float kOutputTransformerSaturationDrive = 0.3f;

        /// @brief How far the supply sags and how fast it returns. The dossier's calibration
        /// targets carry no supply figure at all, so both are stand-ins.
        constexpr float kSagAmount = 0.3f;
        constexpr float kSagRecoveryTimeSeconds = 0.05f;

        // Every Miller pole below is driven from the preceding stage's own plate load.
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
        /// both at the knob's maximum (docs/archive/superpowers/specs/2026-09-08-iicplus-loop-network-design.md
        /// section 5).
        constexpr PlateSwing kV2aPlateSwing{ 75.795f, 114.171f };

        /// @brief V2B's plate swing: 392 - 188 V toward node C, and 188 - 95.662 V down the
        /// 170 k load line to Vgk = 0; the cold, low-headroom stage of dossier 3.7.
        constexpr PlateSwing kV2bPlateSwing{ 204.0f, 92.338f };

        /// @brief The inverter's plate loads, read off S8; the dossier's 5.1 gives both as "82 k"
        /// from the low-resolution sheet. The wider load on the b half is what matches the two
        /// plates' amplitudes against the tail's own asymmetry.
        constexpr float kPhaseInverterPlateAOhms = 82500.0f;
        constexpr float kPhaseInverterPlateBOhms = 90900.0f;

        /// @brief Each inverter plate's swing: 392 - 286 V toward node C and 286 - 142.232 V down
        /// to Vgk = 0 on the 82 k line, scaled to each half's own load rather than re-solved.
        constexpr PlateSwing kPhaseInverterPlateASwing{ 106.646f, 144.645f };
        constexpr PlateSwing kPhaseInverterPlateBSwing{ 117.503f, 159.365f };

        /// @brief The inverter triodes at their operating point, the local Koren Jacobian at
        /// Vgk -1.5 V, Vpk 284.5 V, which is the corpus's convention rather than the model's
        /// nominal mu of 100.
        constexpr float kPhaseInverterMu = 96.378299f;
        constexpr float kPhaseInverterPlateResistanceOhms = 34061.086f;

        /// @brief What the joined cathodes reach the tail node through, S8's R67.
        constexpr float kPhaseInverterCathodeToTailOhms = 470.0f;

        /// @brief Each inverter grid's own leak back to the tail, S8's R65 and R66. The dossier
        /// draws neither, so the model used to pull both grids to 1 M.
        constexpr float kPhaseInverterGridLeakAOhms = 100000.0f;
        constexpr float kPhaseInverterGridLeakBOhms = 150000.0f;

        /// @brief The impedance the pair's cathodes present where the feedback leg meets the tail:
        /// R67 plus the two triodes' cathode impedances in parallel, (ra + Ra) / (mu + 1) halved.
        constexpr float kPhaseInverterTailImpedanceOhms = 1068.5f;

        constexpr double kFilterQ = 0.707;
        constexpr float kSmoothingMs = 20.0f;

        /// @brief How many base-rate frames pass between two coefficient rebuilds.
        ///
        /// Rebuilding per sample would re-solve the tone stack's and the equaliser's matrices at
        /// the oversampled rate, so a knob's travel is quantised to this instead: at 48 kHz it is
        /// 0.67 ms, which cuts a 256-frame block's step by eight.
        constexpr std::uint32_t kControlPeriodFrames = 32;

        // Below this a recomputed coefficient set is indistinguishable from the one in place, so
        // the audio thread skips the rebuild.
        constexpr float kCoefficientEpsilon = 1e-4f;

        constexpr float kMinMasterDb = -60.0f;
        constexpr float kMaxMasterDb = 12.0f;
        constexpr float kMinInputTrimDb = -24.0f;
        constexpr float kMaxInputTrimDb = 24.0f;
        constexpr float kMinOutputDb = -60.0f;
        constexpr float kMaxOutputDb = 12.0f;


        /// @brief Returns the corner of one grid's Miller input-capacitance pole.
        /// @param stage_voltage_gain The driven stage's mid-band gain.
        /// @param plate_load_ohms The driving stage's plate resistor.
        /// @return The -3 dB corner of the grid's RC pole, in Hz.
        double miller_corner_hz(double stage_voltage_gain, double plate_load_ohms)
        {
            return kTriode12AX7MillerCapacitance.corner_hz(stage_voltage_gain, plate_load_ohms);
        }

        /// @brief The loop network's gain from V2A's coupled plate to V2B's grid at one Lead
        /// Master position: the rheostat in parallel with the fixed load on W against the series
        /// 47 k, times the two fixed dividers; dossier 3.6a.
        /// @param lead_master Lead Master position in [0, 1]; 0 is the rheostat at 0 ohm, a mute.
        /// @return The linear gain from the coupling capacitor's node to V2B's grid.
        float loop_network_gain(float lead_master);

        float loop_network_gain(float lead_master)
        {
            const float pot_ohms = kLeadMasterPotOhms * audio_taper_pot(lead_master);
            const float w_ohms = pot_ohms * kLoopShuntToWOhms / (pot_ohms + kLoopShuntToWOhms);
            return (w_ohms / (kLoopSeriesOhms + w_ohms)) * kLoopWToPDivider * kLoopPToGridDivider;
        }
    }

    MesaMark2c::MesaMark2c()
        : v1a_(kV1aSourceResistance, kInputGridLeak, kV1aCathodeBias, kV1aGain, kTriode12AX7GridConduction, kV1aPlateSwing)
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
        , feedback_(48000.0 * kOversamplingFactor, kMark2cFeedback,
                    PowerAmpFeedbackTermination{ kPhaseInverterTailImpedanceOhms })
        , phase_inverter_(
              LongTailedPairTube{ kPhaseInverterMu, kPhaseInverterPlateResistanceOhms,
                                  kPhaseInverterCathodeBias, kTriode12AX7GridConduction },
              LongTailedPairHalf{ kPhaseInverterPlateAOhms, kPhaseInverterSourceResistance,
                                  kPhaseInverterGridLeakAOhms, kPhaseInverterPlateASwing },
              LongTailedPairHalf{ kPhaseInverterPlateBOhms,
                                  feedback_grid_source_resistance_ohms(kMark2cFeedback),
                                  kPhaseInverterGridLeakBOhms, kPhaseInverterPlateBSwing },
              feedback_tail_resistance_ohms(kMark2cFeedback), kPhaseInverterCathodeToTailOhms)
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
        v2b_cathode_shelf_.set_first_order_shelf(oversampled_rate_hz,
                                                deep_ ? kV2bCathodeDeepZeroHz : kV2bCathodeZeroHz,
                                                deep_ ? kV2bCathodeDeepPoleHz : kV2bCathodePoleHz,
                                                kV2bCathodeLowGain);
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
        feedback_.set_presence(presence_.target());

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

        reset();
    }

    void MesaMark2c::process(ProcessContext& context)
    {
        const float* input = context.inputs[0];
        float* output = context.outputs[0];

        for (std::uint32_t offset = 0; offset < context.frames; offset += kControlPeriodFrames)
        {
            const std::uint32_t frames = std::min(kControlPeriodFrames, context.frames - offset);
            render_control_period(input + offset, output + offset, frames);
        }
    }

    void MesaMark2c::render_control_period(const float* input, float* output, std::uint32_t frames)
    {
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
            feedback_.set_presence(presence);
            applied_presence_ = presence;
        }
        if (std::fabs(volume_1 - applied_volume_1_) > kCoefficientEpsilon)
        {
            // Flat until PULL BRIGHT switches the capacitor in; then, as on the lead drive, the
            // shelf gives back what the taper is throwing away at this position.
            const float bright_db = bright_
                ? std::clamp(-gain_to_decibels(audio_taper_pot(volume_1)), 0.0f, kVolume1BrightMaxDb)
                : 0.0f;
            volume_1_bright_.set_high_shelf(sample_rate_ * kOversamplingFactor, kVolume1BrightHz,
                                            bright_db, kFilterQ);
            applied_volume_1_ = volume_1;
        }
        if (std::fabs(lead_drive - applied_lead_drive_) > kCoefficientEpsilon)
        {
            // The 180 pF shorts the pot's top section above the corner, so the shelf's height is
            // whatever the taper is throwing away at this position.
            const float bright_db = std::clamp(-gain_to_decibels(audio_taper_pot(lead_drive)), 0.0f, kLeadDriveBrightMaxDb);
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

        const float volume_1_gain = audio_taper_pot(volume_1);
        const float lead_drive_gain = audio_taper_pot(lead_drive) * kLeadDriveDivider;
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
            const float v1b_input = volume_1_bright_.process_sample(toned * volume_1_gain);
            const float v1b_out = v1b_.process_sample(v1b_grid_miller_.process_sample(v1b_input));

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
            const float master_out = v2b_coupling_.process_sample(v2b_out) * master_gain;
            // EQ OUT lifts the whole slider bus off ground, which takes the five traps out of the
            // path rather than centring them.
            const float equalised = equalizer_mode_ == EqualizerMode::Out
                ? master_out
                : equalizer_bank_.process_sample(master_out);
            // The loop closes on the previous oversampled sample, 2.6 us at 48 kHz, because an
            // implicit solve through the nonlinear power section would iterate.
            const float feedback_grid = feedback_.process_sample(tap_feedback_
                * kTapToFeedbackVolts);
            const LongTailedPair::Plates plates = phase_inverter_.process_sample(
                phase_inverter_grid_miller_.process_sample(equalised), feedback_grid);

            // Simul-Class: both pairs hang on the same transformer, each taking the plate of its
            // own side, and they clip apart because their plate swings do (dossier 7.3).
            const float inner_pair = power_inner_a_.process_sample(plates.a)
                - power_inner_b_.process_sample(plates.b);
            const float outer_pair = power_outer_a_.process_sample(plates.a)
                - power_outer_b_.process_sample(plates.b);
            const float power_tube_out = (inner_pair + outer_pair) * kSimulClassPairWeight
                * kPowerTubeToTransformerAttenuation;

            const float transformer_out = output_transformer_.process_sample(power_tube_out);
            tap_feedback_ = transformer_out;
            scratch_oversampled_[i] = sag_.process_sample(transformer_out);
        }
        oversampler_->downsample(scratch_oversampled_.data(), oversampled_frames, scratch_base_.data());

        // Cabinet convolution lives in the standalone sushidsp.cabinet node; this amp hands its
        // dry signal straight to output.
        for (std::uint32_t i = 0; i < frames; ++i)
        {
            output[i] = scratch_base_[i];
        }

        // Nothing upstream is normalised to the DAC's range, so a hot Lead Drive, Master 1 and
        // Output combination can still land outside [-1, 1]; the clamp is a safety rail, not a
        // tone stage.
        apply_output_safety_clamp(output, frames, output_gain);
    }

    std::uint32_t MesaMark2c::latency_samples() const
    {
        return oversampler_ ? oversampler_->latency_samples() : 0;
    }

    const char* MesaMark2c::type_id() const { return "sushidsp.mesa_mark2c"; }

    namespace
    {
        /// @brief Collects the module's single triodes in signal order, so the two probe
        /// accessors agree on the order and the count by construction.
        /// @return Pointers to V1A, V1B, V3B, V4A, V2A and V2B; the inverter is a pair, not a
        /// single triode, and answers for itself at the index past these.
        std::array<const TriodeStage*, 6> triode_stages_in_signal_order(
            const TriodeStage& v1a, const TriodeStage& v1b, const TriodeStage& v3b,
            const TriodeStage& v4a, const TriodeStage& v2a, const TriodeStage& v2b)
        {
            return { &v1a, &v1b, &v3b, &v4a, &v2a, &v2b };
        }
    }

    std::size_t MesaMark2c::triode_stage_count() const
    {
        return triode_stages_in_signal_order(v1a_, v1b_, v3b_, v4a_, v2a_, v2b_).size() + 1;
    }

    float MesaMark2c::grid_excursion_peak_volts(std::size_t stage) const
    {
        const std::array<const TriodeStage*, 6> stages =
            triode_stages_in_signal_order(v1a_, v1b_, v3b_, v4a_, v2a_, v2b_);
        if (stage == stages.size())
        {
            return phase_inverter_.peak_grid_excursion_volts();
        }
        if (stage > stages.size())
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
        volume_1_bright_.reset();
        v3b_coupling_.reset();
        v4a_interstage_shelf_.reset();
        v4a_cathode_shelf_.reset();
        v4a_coupling_.reset();
        lead_output_divider_.reset();
        v2b_cathode_shelf_.reset();
        v2b_loop_return_coupling_.reset();
        v2b_coupling_.reset();
        feedback_.reset();
        tap_feedback_ = 0.0f;
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

    void MesaMark2c::set_equalizer_mode(EqualizerMode mode) { equalizer_mode_ = mode; }

    void MesaMark2c::set_bright(bool engaged)
    {
        if (engaged == bright_)
        {
            return;
        }
        bright_ = engaged;
        tone_stack_.set_bright(engaged);
        // Forces the next control period to rebuild Volume 1's shelf at the new switch position.
        applied_volume_1_ = -1.0f;
    }

    void MesaMark2c::set_deep(bool engaged)
    {
        if (engaged == deep_)
        {
            return;
        }
        deep_ = engaged;
        // Only the one shelf moves, but it is rebuilt through the same path prepare() uses so the
        // two cannot drift apart.
        build_rate_dependent_filters(sample_rate_ * kOversamplingFactor);
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

    namespace
    {
        /// @brief Maps an automation value onto the equaliser switch's three positions.
        /// @param value 0 is Out, 1 is In, 2 is Auto; anything else is clamped.
        /// @return The position that value selects.
        MesaMark2c::EqualizerMode equalizer_mode_from(float value)
        {
            if (value < 0.5f)
            {
                return MesaMark2c::EqualizerMode::Out;
            }
            return value < 1.5f ? MesaMark2c::EqualizerMode::In : MesaMark2c::EqualizerMode::Auto;
        }
    }

    const ParameterTable<MesaMark2c, 18>& MesaMark2c::parameters()
    {
        // The order is a contract: a host addresses these by index and a saved preset reads them
        // back by index, so rows are appended, never reordered or removed.
        static const ParameterTable<MesaMark2c, 18> table{ {{
            { { "volume_1", "Volume 1", 0.0f, 1.0f, 0.5f, ParamKind::Linear, "" },
              [](MesaMark2c& n, float v) { n.set_volume_1(v); },
              [](const MesaMark2c& n) { return n.volume_1_.target(); } },
            { { "treble", "Treble", 0.0f, 1.0f, 0.5f, ParamKind::Linear, "" },
              [](MesaMark2c& n, float v) { n.set_treble(v); },
              [](const MesaMark2c& n) { return n.treble_.target(); } },
            { { "bass", "Bass", 0.0f, 1.0f, 0.5f, ParamKind::Linear, "" },
              [](MesaMark2c& n, float v) { n.set_bass(v); },
              [](const MesaMark2c& n) { return n.bass_.target(); } },
            { { "middle", "Middle", 0.0f, 1.0f, 0.5f, ParamKind::Linear, "" },
              [](MesaMark2c& n, float v) { n.set_middle(v); },
              [](const MesaMark2c& n) { return n.middle_.target(); } },
            { { "lead_drive", "Lead Drive", 0.0f, 1.0f, 0.5f, ParamKind::Linear, "" },
              [](MesaMark2c& n, float v) { n.set_lead_drive(v); },
              [](const MesaMark2c& n) { return n.lead_drive_.target(); } },
            { { "lead_master", "Lead Master", 0.0f, 1.0f, 0.5f, ParamKind::Linear, "" },
              [](MesaMark2c& n, float v) { n.set_lead_master(v); },
              [](const MesaMark2c& n) { return n.lead_master_.target(); } },
            { { "master_1", "Master 1", kMinMasterDb, kMaxMasterDb, 0.0f, ParamKind::Decibel, "dB" },
              [](MesaMark2c& n, float v) { n.set_master_1(v); },
              [](const MesaMark2c& n) { return n.master_1_db_; } },
            { { "presence", "Presence", 0.0f, 1.0f, 0.5f, ParamKind::Linear, "" },
              [](MesaMark2c& n, float v) { n.set_presence(v); },
              [](const MesaMark2c& n) { return n.presence_.target(); } },
            { { "eq_80", "EQ 80", -1.0f, 1.0f, 0.0f, ParamKind::Linear, "" },
              [](MesaMark2c& n, float v) { n.set_equalizer_band(0, v); },
              [](const MesaMark2c& n) { return n.equalizer_[0].target(); } },
            { { "eq_240", "EQ 240", -1.0f, 1.0f, 0.0f, ParamKind::Linear, "" },
              [](MesaMark2c& n, float v) { n.set_equalizer_band(1, v); },
              [](const MesaMark2c& n) { return n.equalizer_[1].target(); } },
            { { "eq_750", "EQ 750", -1.0f, 1.0f, 0.0f, ParamKind::Linear, "" },
              [](MesaMark2c& n, float v) { n.set_equalizer_band(2, v); },
              [](const MesaMark2c& n) { return n.equalizer_[2].target(); } },
            { { "eq_2200", "EQ 2200", -1.0f, 1.0f, 0.0f, ParamKind::Linear, "" },
              [](MesaMark2c& n, float v) { n.set_equalizer_band(3, v); },
              [](const MesaMark2c& n) { return n.equalizer_[3].target(); } },
            { { "eq_6600", "EQ 6600", -1.0f, 1.0f, 0.0f, ParamKind::Linear, "" },
              [](MesaMark2c& n, float v) { n.set_equalizer_band(4, v); },
              [](const MesaMark2c& n) { return n.equalizer_[4].target(); } },
            { { "input_trim", "Input Trim", kMinInputTrimDb, kMaxInputTrimDb, 0.0f,
                ParamKind::Decibel, "dB" },
              [](MesaMark2c& n, float v) { n.set_input_trim(v); },
              [](const MesaMark2c& n) { return n.input_trim_db_; } },
            { { "output", "Output", kMinOutputDb, kMaxOutputDb, 0.0f, ParamKind::Decibel, "dB" },
              [](MesaMark2c& n, float v) { n.set_output(v); },
              [](const MesaMark2c& n) { return n.output_db_; } },
            // The panel switches are appended rather than placed beside the knobs they sit next
            // to, because the index order is what a saved preset addresses these by.
            { { "bright", "Pull Bright", 0.0f, 1.0f, 0.0f, ParamKind::Boolean, "" },
              [](MesaMark2c& n, float v) { n.set_bright(v >= 0.5f); },
              [](const MesaMark2c& n) { return n.bright_ ? 1.0f : 0.0f; } },
            { { "deep", "Pull Deep", 0.0f, 1.0f, 0.0f, ParamKind::Boolean, "" },
              [](MesaMark2c& n, float v) { n.set_deep(v >= 0.5f); },
              [](const MesaMark2c& n) { return n.deep_ ? 1.0f : 0.0f; } },
            { { "eq_mode", "EQ Switch", 0.0f, 2.0f, 1.0f, ParamKind::Choice, "" },
              [](MesaMark2c& n, float v) { n.set_equalizer_mode(equalizer_mode_from(v)); },
              [](const MesaMark2c& n) { return static_cast<float>(n.equalizer_mode_); } },
        }} };
        return table;
    }

    float MesaMark2c::get_parameter(const char* id) const { return parameters().read(*this, id); }

    void MesaMark2c::set_parameter_for_control_thread(const char* id, float value)
    {
        parameters().write(*this, id, value);
    }

    void MesaMark2c::set_parameter_for_audio_thread(std::uint16_t index, float value)
    {
        parameters().write(*this, index, value);
    }

    namespace
    {
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
            MesaMark2c::parameters().descriptors(),
            MesaMark2c::parameters().count(),
            PortLayout{ 1, 1 },
            &make_mesa_mark2c,
        };
        return descriptor;
    }
}
