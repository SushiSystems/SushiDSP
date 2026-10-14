# CMakeLists.txt
# SushiDSP - https://github.com/SushiSystems/SushiDSP
# Copyright (c) 2026-present Mustafa Garip & Sushi Systems
# Licensed under PolyForm Noncommercial 1.0.0. See LICENSE.
# Commercial use requires a licence from Sushi Systems.

# Internal module: test-side code that the test programs share. Its headers are never
# installed, and no product links it.

add_library(sushi_dsp_regression_test INTERFACE)
add_library(SushiDSP::sushi_dsp_regression_test ALIAS sushi_dsp_regression_test)
target_include_directories(sushi_dsp_regression_test INTERFACE
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
)
# The header includes gtest.h, so a test that includes it links GoogleTest itself.
