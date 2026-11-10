/**************************************************************************/
/* test_cabinet_geometry.cpp                                              */
/**************************************************************************/
/*                          This file is part of:                         */
/*                                SushiDSP                                */
/*                https://github.com/SushiSystems/SushiDSP                */
/*                         https://sushisystems.io                        */
/**************************************************************************/
/* Copyright (c) 2026-present Mustafa Garip & Sushi Systems               */
/*                                                                        */
/* Licensed under the Apache License, Version 2.0 (the "License"); you    */
/* may not use this file except in compliance with the License. You may   */
/* obtain a copy of the License at                                        */
/*                                                                        */
/*     https://www.apache.org/licenses/LICENSE-2.0                        */
/*                                                                        */
/* Unless required by applicable law or agreed to in writing, software    */
/* distributed under the License is distributed on an "AS IS" BASIS,      */
/* WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or        */
/* implied. See the License for the specific language governing           */
/* permissions and limitations under the License.                         */
/**************************************************************************/

/**
 * @file test_cabinet_geometry.cpp
 * @brief Tests that every shipped cabinet fits the driver limit the near-field model is sized by.
 * @author Mustafa Garip
 */

#include <gtest/gtest.h>

#include <SushiDSP/nodes/room/cabinet_geometry.hpp>
#include <SushiDSP/nodes/room/driver_field.hpp>

#include <string>

namespace
{
    using SushiDSP::room::CabinetGeometry;

    /// @brief Holds one shipped geometry.
    struct GeometryRow
    {
            const char* name;
            CabinetGeometry geometry;
    };

    /// @brief Names a row for gtest.
    std::string row_name(const ::testing::TestParamInfo<GeometryRow>& info)
    {
        return info.param.name;
    }

    class Small_CabinetGeometry : public ::testing::TestWithParam<GeometryRow>
    {
    };
} // namespace

TEST_P(Small_CabinetGeometry, EveryShippedCabinetFitsTheDriverLimit)
{
    EXPECT_LE(GetParam().geometry.driver_count(), CabinetGeometry::K_MAX_DRIVERS);
}

TEST_P(Small_CabinetGeometry, EveryShippedCabinetsElementsFitAFieldOfArrivals)
{
    const SushiDSP::room::DriverField field(GetParam().geometry);

    EXPECT_LE(field.element_count(), SushiDSP::room::DriverField::K_MAX_ELEMENTS);
}

INSTANTIATE_TEST_SUITE_P(Cabinets, Small_CabinetGeometry,
                         ::testing::Values(GeometryRow{"OneByTwelve", SushiDSP::room::K_CABINET1X12},
                                           GeometryRow{"TwoByTwelve", SushiDSP::room::K_CABINET2X12},
                                           GeometryRow{"FourByTwelve", SushiDSP::room::K_CABINET4X12}),
                         row_name);
