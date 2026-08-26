/**************************************************************************/
/* test_ir_loader.cpp                                                     */
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

#include <vector>

#include <gtest/gtest.h>

#include "SushiDSP/io/IrLoader.hpp"

TEST(Unit_Dsp, IrLoaderTrimsAndFadesToMaxLength)
{
    std::vector<float> samples(48000, 1.0f);
    const auto ir = SushiDSP::IrLoader::prepare(samples, 48000.0, 48000.0, SushiDSP::IrLoadOptions{ 200.0f, 5.0f, false });

    EXPECT_EQ(ir.samples.size(), 9600u);
    EXPECT_NEAR(ir.samples[9599], 0.0f, 1e-3f);
    EXPECT_NEAR(ir.samples[9600 - 240 - 1], 1.0f, 1e-3f);
}

TEST(Unit_Dsp, IrLoaderNormalisesBandAverageToZeroDb)
{
    std::vector<float> samples(4096, 0.0f);
    samples[0] = 0.1f;

    const auto ir = SushiDSP::IrLoader::prepare(samples, 48000.0, 48000.0);

    EXPECT_NEAR(ir.samples[0], 1.0f, 1e-3f);
    EXPECT_DOUBLE_EQ(ir.sample_rate, 48000.0);
}
