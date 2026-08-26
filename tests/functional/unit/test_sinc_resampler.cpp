/**************************************************************************/
/* test_sinc_resampler.cpp                                               */
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

// _USE_MATH_DEFINES must precede every include: it is what makes MSVC's <cmath> declare M_PI,
// and <cmath> may otherwise be pulled in transitively before this point.
#define _USE_MATH_DEFINES

#include <cmath>
#include <cstddef>
#include <vector>

#include <gtest/gtest.h>

#include "SushiDSP/math/SincResampler.hpp"

TEST(Unit_Dsp, SincResamplerPreservesAOneKilohertzToneWithinMinusEightyDb)
{
    std::vector<float> in(44100);
    for (std::size_t i = 0; i < in.size(); ++i)
    {
        in[i] = static_cast<float>(std::sin(2.0 * M_PI * 1000.0 * static_cast<double>(i) / 44100.0));
    }

    const auto out = SushiDSP::SincResampler::resample(in, 44100.0, 48000.0);
    ASSERT_NEAR(static_cast<double>(out.size()), 48000.0, 2.0);

    double error  = 0.0;
    double energy = 0.0;
    for (std::size_t i = 1000; i + 1000 < out.size(); ++i)
    {
        const double expected = std::sin(2.0 * M_PI * 1000.0 * static_cast<double>(i) / 48000.0);
        error += (out[i] - expected) * (out[i] - expected);
        energy += expected * expected;
    }

    EXPECT_LT(10.0 * std::log10(error / energy), -80.0);
}
