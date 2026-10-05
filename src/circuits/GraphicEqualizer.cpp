/****************************************************************/
/* GraphicEqualizer.cpp                                         */
/* SushiDSP - https://github.com/SushiSystems/SushiDSP          */
/* Copyright (c) 2026 Sushi Systems                             */
/* Licensed under PolyForm Noncommercial 1.0.0. See LICENSE.    */
/* Commercial use requires a licence from Sushi Systems.        */
/****************************************************************/

/**
 * @file GraphicEqualizer.cpp
 * @brief Implements the bank of peaking bands in series.
 * @author Mustafa Garip
 */

#include "SushiDSP/circuits/GraphicEqualizer.hpp"

#include <algorithm>

namespace SushiDSP
{
    GraphicEqualizer::GraphicEqualizer(double sample_rate,
                                        std::initializer_list<float> centre_frequencies_hz,
                                        float band_q, float maximum_gain_db)
        : sample_rate_(sample_rate)
        , maximum_gain_db_(maximum_gain_db)
    {
        for (const float centre_hz : centre_frequencies_hz)
        {
            if (band_count_ >= kMaximumBands)
            {
                break;
            }
            centres_hz_[band_count_] = centre_hz;
            positions_[band_count_] = 0.0f;
            band_q_[band_count_] = band_q;
            bands_[band_count_].set_peaking(sample_rate_, centre_hz, 0.0f, band_q_[band_count_]);
            ++band_count_;
        }
    }

    GraphicEqualizer::GraphicEqualizer(double sample_rate,
                                        std::initializer_list<float> centre_frequencies_hz,
                                        std::initializer_list<float> band_q, float maximum_gain_db)
        : sample_rate_(sample_rate)
        , maximum_gain_db_(maximum_gain_db)
    {
        auto band_q_iterator = band_q.begin();
        for (const float centre_hz : centre_frequencies_hz)
        {
            if (band_count_ >= kMaximumBands || band_q_iterator == band_q.end())
            {
                break;
            }
            centres_hz_[band_count_] = centre_hz;
            positions_[band_count_] = 0.0f;
            band_q_[band_count_] = *band_q_iterator;
            bands_[band_count_].set_peaking(sample_rate_, centre_hz, 0.0f, band_q_[band_count_]);
            ++band_q_iterator;
            ++band_count_;
        }
    }

    void GraphicEqualizer::set_band(std::size_t band, float position)
    {
        if (band >= band_count_)
        {
            return;
        }
        const float clamped_position = std::clamp(position, -1.0f, 1.0f);
        positions_[band] = clamped_position;
        bands_[band].set_peaking(sample_rate_, centres_hz_[band], clamped_position * maximum_gain_db_,
                                  band_q_[band]);
    }

    float GraphicEqualizer::process_sample(float input)
    {
        float sample = input;
        for (std::size_t band = 0; band < band_count_; ++band)
        {
            sample = bands_[band].process_sample(sample);
        }
        return sample;
    }

    void GraphicEqualizer::reset()
    {
        for (std::size_t band = 0; band < band_count_; ++band)
        {
            bands_[band].reset();
        }
    }

    std::size_t GraphicEqualizer::band_count() const
    {
        return band_count_;
    }
}
