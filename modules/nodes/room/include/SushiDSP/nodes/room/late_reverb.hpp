/**************************************************************************/
/* late_reverb.hpp                                                        */
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
 * @file late_reverb.hpp
 * @brief The 16-line Hadamard feedback delay network that renders the late tail.
 * @author Mustafa Garip & Sushi Systems
 */

#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <vector>

#include <SushiDSP/nodes/room/cascade_filter.hpp>
#include <SushiDSP/nodes/room/fdn_design.hpp>

namespace SushiDSP::room
{
    /// @brief A 16-line feedback delay network with a Hadamard matrix, one fitted absorptive
    /// filter per line and Jot's tone correction. Audio thread. Spec §6.
    class LateReverb
    {
        public:
            /// @brief Counts the delay lines.
            static constexpr std::size_t K_LINE_COUNT = FdnDesign::K_LINE_COUNT;

            /// @brief Counts the frames a line-length change crossfades over.
            static constexpr std::uint32_t K_CROSSFADE_FRAMES = 512;

            /// @brief Allocates the lines and the input delay; the only allocation this class makes.
            /// @param max_line_frames Sized for the longest room the bounds allow: an RT60 of 3 s
            /// gives a mean line of 262.5 ms and a longest of 525 ms, 25200 frames at 48 kHz.
            /// @pre max_line_frames >= 1.
            void prepare(std::uint32_t max_line_frames);

            /// @brief Silences the lines, the input delay and every filter state.
            void reset() noexcept;

            /// @brief Renders one block. For the microphone listener the caller passes one buffer
            /// twice and reads it once.
            /// @pre prepare() has run; every line length lies in [1, max_line_frames].
            void process(const FdnDesign& design, const float* input, float* left, float* right,
                         std::uint32_t frames) noexcept;

            /// @brief Renders one block of the network alone: the input enters at unit gain with no delay
            /// and the two output sums leave before the tone correction and the coherence pair. Only the
            /// design's line lengths and absorption sections are read.
            /// @param sum_a Receives the A sum, the one the left ear and a microphone read.
            /// @param sum_b Receives the B sum, orthogonal to A.
            /// @pre prepare() has run; sum_a and sum_b do not alias each other or the input.
            void process_sums(const FdnDesign& design, const float* input, float* sum_a,
                              float* sum_b, std::uint32_t frames) noexcept;

            /// @brief Applies the orthonormal 16-point Walsh-Hadamard transform in place, 64
            /// additions and one scale by 1/4.
            static void hadamard_transform(std::array<double, K_LINE_COUNT>& values) noexcept;

        private:
            /// @brief Holds sixteen biquad sections as arrays over the lines, so one loop over the
            /// lines vectorises.
            struct SectionBank
            {
                    /// @brief Stores b0 of each line's section.
                    std::array<double, K_LINE_COUNT> b0;

                    /// @brief Stores b1 of each line's section.
                    std::array<double, K_LINE_COUNT> b1;

                    /// @brief Stores b2 of each line's section.
                    std::array<double, K_LINE_COUNT> b2;

                    /// @brief Stores a1 of each line's section.
                    std::array<double, K_LINE_COUNT> a1;

                    /// @brief Stores a2 of each line's section.
                    std::array<double, K_LINE_COUNT> a2;

                    /// @brief Stores each line's previous input.
                    std::array<double, K_LINE_COUNT> x1;

                    /// @brief Stores each line's input before the previous one.
                    std::array<double, K_LINE_COUNT> x2;

                    /// @brief Stores each line's previous output.
                    std::array<double, K_LINE_COUNT> y1;

                    /// @brief Stores each line's output before the previous one.
                    std::array<double, K_LINE_COUNT> y2;

                    /// @brief Advances every line's section by one sample, reading in and writing out.
                    void run(const double* in, double* out) noexcept;

                    /// @brief Copies one design section per line into the coefficient arrays.
                    void
                    load(const std::array<std::array<double, 5>, K_LINE_COUNT>& sections) noexcept;

                    /// @brief Zeroes the four state arrays.
                    void reset() noexcept;
            };

            /// @brief Names the fourth-order filter each output cascade runs through.
            using OutputFilter = Room::CascadeFilter;

            /// @brief Adopts the design's lengths, starting a crossfade when they differ from the
            /// lengths in use.
            void adopt_lengths(const FdnDesign& design) noexcept;

            /// @brief Loads the design's filters into the banks and the four output filters.
            void load_filters(const FdnDesign& design) noexcept;

            /// @brief Runs the network over a run of frames, blending two read heads while
            /// kFading and reading one otherwise; kTwoEars writes both ear outputs, its absence
            /// writes the plain A sum to left alone. kSums overrides both: the input enters at
            /// unit gain and the raw A and B sums go to left and right.
            template <bool kFading, bool kTwoEars, bool kSums>
            void render(const float* input, float* left, float* right,
                        std::uint32_t frames) noexcept;

            /// @brief Runs the fading part of a block, then the rest, through one render choice.
            template <bool kTwoEars, bool kSums>
            void render_block(const float* input, float* left, float* right,
                              std::uint32_t frames) noexcept;

            /// @brief Stores the sixteen lines back to back, K_LINE_COUNT * capacity_ floats.
            std::vector<float> lines_;

            /// @brief Stores the input as it waits for the crossover time.
            std::vector<float> input_delay_;

            /// @brief Counts the frames each line holds.
            std::uint32_t capacity_ = 0;

            /// @brief Indexes the frame written next in every line and in the input delay.
            std::uint32_t write_ = 0;

            /// @brief Stores the line lengths in use.
            std::array<std::uint32_t, K_LINE_COUNT> length_{};

            /// @brief Stores the line lengths a crossfade fades away from.
            std::array<std::uint32_t, K_LINE_COUNT> previous_length_{};

            /// @brief Counts the crossfade frames left; zero when no crossfade runs.
            std::uint32_t fade_remaining_ = 0;

            /// @brief Weights the new read head, rising from 0 to 1 across the crossfade.
            double fade_weight_ = 0.0;

            /// @brief Records whether length_ holds a design's lengths.
            bool lengths_known_ = false;

            /// @brief Stores the input delay in frames.
            std::uint32_t input_delay_frames_ = 0;

            /// @brief Stores the gain into the network.
            double input_gain_ = 0.0;

            /// @brief Runs each line's first absorption section.
            SectionBank section_a_{};

            /// @brief Runs each line's second absorption section.
            SectionBank section_b_{};

            /// @brief Applies Jot's tone correction to the left ear.
            OutputFilter tone_left_;

            /// @brief Applies Jot's tone correction to the right ear.
            OutputFilter tone_right_;

            /// @brief Applies the coherence filter gamma to the A sum on its way to the right ear.
            OutputFilter coherence_a_;

            /// @brief Applies sqrt(1 - gamma^2) to the B sum on its way to the right ear.
            OutputFilter coherence_b_;
    };
} // namespace SushiDSP::room
