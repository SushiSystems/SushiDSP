/**************************************************************************/
/* Take.cpp                                                               */
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
 * @file Take.cpp
 * @brief Implements the in-memory recording and its loading from a WAV file.
 * @author Mustafa Garip
 */

#include "SushiDSP/capture/Take.hpp"

#include <algorithm>

#include "SushiDSP/io/WavFile.hpp"

namespace SushiDSP
{
    Take::Take(std::uint32_t channels, double sample_rate, std::size_t frames)
        : samples_(std::max<std::uint32_t>(channels, 1u))
        , sample_rate_(sample_rate)
        , frames_(frames)
        , channels_(std::max<std::uint32_t>(channels, 1u))
    {
        for (std::vector<float>& channel_samples : samples_)
        {
            channel_samples.assign(frames_, 0.0f);
        }
    }

    Take Take::load(const std::string& path)
    {
        double                                sample_rate = 0.0;
        const std::vector<std::vector<float>> channels = WavFile::load_channels(path, &sample_rate);

        Take take(static_cast<std::uint32_t>(channels.size()), sample_rate, channels[0].size());
        for (std::size_t channel = 0; channel < channels.size(); ++channel)
        {
            std::copy(channels[channel].begin(), channels[channel].end(),
                      take.samples_[channel].begin());
        }
        return take;
    }

    const float* Take::channel(std::size_t index) const
    {
        if (index >= samples_.size())
        {
            return nullptr;
        }
        return samples_[index].data();
    }

    float* Take::mutable_channel(std::size_t index)
    {
        if (index >= samples_.size())
        {
            return nullptr;
        }
        return samples_[index].data();
    }
}
