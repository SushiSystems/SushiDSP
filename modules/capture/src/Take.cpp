/**************************************************************************/
/* Take.cpp                                                               */
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
