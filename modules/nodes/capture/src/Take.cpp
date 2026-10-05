/****************************************************************/
/* Take.cpp                                                     */
/* SushiDSP - https://github.com/SushiSystems/SushiDSP          */
/* Copyright (c) 2026 Sushi Systems                             */
/* Licensed under PolyForm Noncommercial 1.0.0. See LICENSE.    */
/* Commercial use requires a licence from Sushi Systems.        */
/****************************************************************/

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
