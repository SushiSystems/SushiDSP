/**************************************************************************/
/* sushidsp_profile_names.hpp                                             */
/**************************************************************************/
/*                          This file is part of:                         */
/*                                SushiDSP                                */
/*                https://github.com/SushiSystems/SushiDSP                */
/*                         https://sushisystems.io                        */
/**************************************************************************/
/* Copyright (c) 2026-present Mustafa Garip & Sushi Systems               */
/*                                                                        */
/* Licensed under the PolyForm Noncommercial License 1.0.0 (the           */
/* "License"); you may not use this file except in compliance with the    */
/* License. You may obtain a copy of the License at                       */
/*                                                                        */
/*     https://polyformproject.org/licenses/noncommercial/1.0.0           */
/*                                                                        */
/* Noncommercial use is free. Commercial use requires a separate licence  */
/* from Sushi Systems; see COMMERCIAL.md. The software is provided        */
/* "as is", without warranty of any kind.                                 */
/**************************************************************************/

/**
 * @file sushidsp_profile_names.hpp
 * @brief Declares the names of the timelines, groups, zones and counters SushiDSP records.
 * @author Mustafa Garip
 * @see modules/foundation/profile/README.md
 */

#pragma once

/// @brief Holds the names SushiDSP's records and its checks files agree on.
namespace SushiDSP::profile::names
{
    /// @brief The timeline a device marks once per block, on arrival.
    inline constexpr const char* K_BLOCK_TIMELINE = "dsp.block";

    /// @brief The timeline the benchmark harness marks once per repetition.
    inline constexpr const char* K_BENCH_TIMELINE = "bench.block";

    /// @brief The zone around the work one block costs, in a session renderer or a host callback.
    inline constexpr const char* K_PROCESS_ZONE = "dsp.process";

    /// @brief The zone around one RigGraph::process call.
    inline constexpr const char* K_GRAPH_PROCESS_ZONE = "graph.process";

    /// @brief The zone around one node's process, named at run time by the node's type id.
    inline constexpr const char* K_NODE_ZONE = "dsp.node";

    /// @brief The zone around the fan-in and fan-out sums of a graph.
    inline constexpr const char* K_GATHER_ZONE = "dsp.gather";

    /// @brief The zone around the lane read of one track.
    inline constexpr const char* K_SESSION_LANE_ZONE = "session.lane";

    /// @brief The zone around the rig of one track.
    inline constexpr const char* K_SESSION_RIG_ZONE = "session.rig";

    /// @brief The zone around the strip gain and the sum into the master of one track.
    inline constexpr const char* K_SESSION_MIX_ZONE = "session.mix";

    /// @brief The zone around the recorder write.
    inline constexpr const char* K_SESSION_RECORDER_ZONE = "session.recorder";

    /// @brief The zone around the metronome.
    inline constexpr const char* K_SESSION_METRONOME_ZONE = "session.metronome";

    /// @brief The group of the zones of a graph pass.
    inline constexpr const char* K_GRAPH_GROUP = "Graph";

    /// @brief The group of the zones of a session renderer.
    inline constexpr const char* K_SESSION_GROUP = "Session";

    /// @brief The group of the zones inside a node.
    inline constexpr const char* K_NODE_GROUP = "Node";

    /// @brief The group of the counters of the real-time failures.
    inline constexpr const char* K_FAILURE_GROUP = "Failure";

    /// @brief The group of the zones inside the room node.
    inline constexpr const char* K_ROOM_GROUP = "Room";

    /// @brief The zone around the tail render of one source stage.
    inline constexpr const char* K_ROOM_TAIL_ZONE = "room.tail";

    /// @brief The zone around the modal render of one source stage.
    inline constexpr const char* K_ROOM_MODAL_ZONE = "room.modal";

    /// @brief The zone around the pair renders that feed the ears.
    inline constexpr const char* K_ROOM_PAIR_ZONE = "room.pair";

    /// @brief The zone around the ears output.
    inline constexpr const char* K_ROOM_EARS_ZONE = "room.ears";

    /// @brief The zone around the microphone outputs.
    inline constexpr const char* K_ROOM_MICROPHONES_ZONE = "room.microphones";

    /// @brief The unit of a count.
    inline constexpr const char* K_COUNT_UNIT = "count";

    /// @brief The counter of blocks that arrived more than 1.5 block periods after the last.
    inline constexpr const char* K_ARRIVAL_GAP_COUNTER = "device.arrival_gap";

    /// @brief The counter of blocks whose processing passed their block period.
    inline constexpr const char* K_OVERRUN_COUNTER = "device.overrun";

    /// @brief The counter of Newton solves that did not converge.
    inline constexpr const char* K_NON_CONVERGED_COUNTER = "solver.non_converged";

    /// @brief The counter of Newton solves that fell back to the last finite state.
    inline constexpr const char* K_NAN_FALLBACK_COUNTER = "solver.nan_fallback";

    /// @brief The counter of Newton solves.
    inline constexpr const char* K_SOLVES_COUNTER = "solver.solves";

    /// @brief The counter of the most iterations one solve took.
    inline constexpr const char* K_WORST_ITERATIONS_COUNTER = "solver.worst_iterations";

    /// @brief The counter of Newton solves of the tube stages.
    inline constexpr const char* K_NEWTON_SOLVES_COUNTER = "newton.solves";

    /// @brief The counter of Newton solves that stopped at their iteration cap.
    inline constexpr const char* K_NEWTON_CAPS_COUNTER = "newton.iteration_caps";

    /// @brief The counter of Newton solves that met a non-finite step.
    inline constexpr const char* K_NEWTON_NON_FINITE_COUNTER = "newton.non_finite_steps";

    /// @brief The counter of the most iterations one Newton solve took.
    inline constexpr const char* K_NEWTON_WORST_ITERATIONS_COUNTER = "newton.worst_iterations";

    /// @brief The counter of samples an output clamp cut or found non-finite.
    inline constexpr const char* K_CLAMPED_COUNTER = "output.clamped";
    /// @brief Holds the names of the regions of the JCM800 node.
    namespace jcm800
    {
        /// @brief The group of the JCM800 regions.
        inline constexpr const char* K_GROUP = "Jcm800";

        /// @brief The zone around the input scale and the stages before the oversampler.
        inline constexpr const char* K_INPUT_ZONE = "jcm800.input";

        /// @brief The zone around the upsampler.
        inline constexpr const char* K_UPSAMPLE_ZONE = "jcm800.upsample";

        /// @brief The zone around the oversampled loop of every stage.
        inline constexpr const char* K_LOOP_ZONE = "jcm800.oversampled_loop";

        /// @brief The zone around the decimator.
        inline constexpr const char* K_DOWNSAMPLE_ZONE = "jcm800.downsample";

        /// @brief The zone around the linear stages after the decimator.
        inline constexpr const char* K_POST_ZONE = "jcm800.post";

        /// @brief The zone around the output clamp.
        inline constexpr const char* K_CLAMP_ZONE = "jcm800.clamp";
    } // namespace jcm800

    /// @brief Holds the names of the regions of the Peavey 5150 node.
    namespace peavey5150
    {
        /// @brief The group of the Peavey 5150 regions.
        inline constexpr const char* K_GROUP = "Peavey5150";

        /// @brief The zone around the input scale and the stages before the oversampler.
        inline constexpr const char* K_INPUT_ZONE = "peavey5150.input";

        /// @brief The zone around the upsampler.
        inline constexpr const char* K_UPSAMPLE_ZONE = "peavey5150.upsample";

        /// @brief The zone around the oversampled loop of every stage.
        inline constexpr const char* K_LOOP_ZONE = "peavey5150.oversampled_loop";

        /// @brief The zone around the decimator.
        inline constexpr const char* K_DOWNSAMPLE_ZONE = "peavey5150.downsample";

        /// @brief The zone around the linear stages after the decimator.
        inline constexpr const char* K_POST_ZONE = "peavey5150.post";

        /// @brief The zone around the output clamp.
        inline constexpr const char* K_CLAMP_ZONE = "peavey5150.clamp";
    } // namespace peavey5150

    /// @brief Holds the names of the regions of the Mesa Mark IIC+ node.
    namespace mesa_mark2c
    {
        /// @brief The group of the Mesa Mark IIC+ regions.
        inline constexpr const char* K_GROUP = "MesaMark2c";

        /// @brief The zone around the input scale and the stages before the oversampler.
        inline constexpr const char* K_INPUT_ZONE = "mesa_mark2c.input";

        /// @brief The zone around the upsampler.
        inline constexpr const char* K_UPSAMPLE_ZONE = "mesa_mark2c.upsample";

        /// @brief The zone around the oversampled loop of every stage.
        inline constexpr const char* K_LOOP_ZONE = "mesa_mark2c.oversampled_loop";

        /// @brief The zone around the decimator.
        inline constexpr const char* K_DOWNSAMPLE_ZONE = "mesa_mark2c.downsample";

        /// @brief The zone around the linear stages after the decimator.
        inline constexpr const char* K_POST_ZONE = "mesa_mark2c.post";

        /// @brief The zone around the output clamp.
        inline constexpr const char* K_CLAMP_ZONE = "mesa_mark2c.clamp";
    } // namespace mesa_mark2c

    /// @brief Holds the names of the regions of the Diezel VH4 node.
    namespace diezel_vh4
    {
        /// @brief The group of the Diezel VH4 regions.
        inline constexpr const char* K_GROUP = "DiezelVh4";

        /// @brief The zone around the input scale and the stages before the oversampler.
        inline constexpr const char* K_INPUT_ZONE = "diezel_vh4.input";

        /// @brief The zone around the upsampler.
        inline constexpr const char* K_UPSAMPLE_ZONE = "diezel_vh4.upsample";

        /// @brief The zone around the oversampled loop of every stage.
        inline constexpr const char* K_LOOP_ZONE = "diezel_vh4.oversampled_loop";

        /// @brief The zone around the decimator.
        inline constexpr const char* K_DOWNSAMPLE_ZONE = "diezel_vh4.downsample";

        /// @brief The zone around the linear stages after the decimator.
        inline constexpr const char* K_POST_ZONE = "diezel_vh4.post";

        /// @brief The zone around the output clamp.
        inline constexpr const char* K_CLAMP_ZONE = "diezel_vh4.clamp";
    } // namespace diezel_vh4

    /// @brief Holds the names of the regions of the Tube Screamer node.
    namespace tubescreamer
    {
        /// @brief The group of the Tube Screamer regions.
        inline constexpr const char* K_GROUP = "TubeScreamer";

        /// @brief The zone around the input scale.
        inline constexpr const char* K_INPUT_ZONE = "tubescreamer.input";

        /// @brief The zone around the upsampler.
        inline constexpr const char* K_UPSAMPLE_ZONE = "tubescreamer.upsample";

        /// @brief The zone around the clip and tone blocks run per oversampled sample.
        inline constexpr const char* K_LOOP_ZONE = "tubescreamer.oversampled_loop";

        /// @brief The zone around the decimator.
        inline constexpr const char* K_DOWNSAMPLE_ZONE = "tubescreamer.downsample";

        /// @brief The zone around the level gain.
        inline constexpr const char* K_OUTPUT_ZONE = "tubescreamer.output";
    } // namespace tubescreamer
} // namespace SushiDSP::profile::names
