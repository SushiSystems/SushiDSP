/**************************************************************************/
/* GraphicEqualizer.hpp                                                   */
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

#pragma once

#include <array>
#include <cstddef>
#include <initializer_list>

#include "SushiDSP/math/Biquad.hpp"

namespace SushiDSP
{
    /// @brief A bank of fixed-centre peaking bands, each ±maximum_gain_db, in series.
    class GraphicEqualizer
    {
    public:
        static constexpr std::size_t kMaximumBands = 8;

        /// @param sample_rate The rate the bank runs at, in hertz.
        /// @param centre_frequencies_hz Band centres, ascending; at most kMaximumBands, extras are ignored.
        /// @param band_q Peaking Q shared by every band.
        /// @param maximum_gain_db Gain at slider ±1.
        GraphicEqualizer(double sample_rate, std::initializer_list<float> centre_frequencies_hz,
                          float band_q, float maximum_gain_db);

        /// @brief Sets one slider.
        /// @param band Band index; out-of-range indices are ignored.
        /// @param position In [-1, 1]; 0 is flat. Values outside are clamped.
        void set_band(std::size_t band, float position);

        /// @brief Processes one sample through every band in series. Audio-thread safe.
        float process_sample(float input);

        /// @brief Clears every band's state.
        void reset();

        /// @brief Returns the number of active bands.
        std::size_t band_count() const;

    private:
        double sample_rate_;
        float band_q_;
        float maximum_gain_db_;
        std::size_t band_count_ = 0;
        std::array<float, kMaximumBands> centres_hz_{};
        std::array<float, kMaximumBands> positions_{};
        std::array<Biquad, kMaximumBands> bands_{};
    };
}
