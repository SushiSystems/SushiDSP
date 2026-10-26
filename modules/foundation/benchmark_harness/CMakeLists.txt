# CMakeLists.txt
# SushiDSP - https://github.com/SushiSystems/SushiDSP
# Copyright (c) 2026-present Mustafa Garip & Sushi Systems
# Licensed under PolyForm Noncommercial 1.0.0. See LICENSE.
# Commercial use requires a licence from Sushi Systems.

# Internal module: test-side code that the test programs share. Its headers are never
# installed, and no product links it.

add_library(sushi_dsp_benchmark_harness INTERFACE)
add_library(SushiDSP::sushi_dsp_benchmark_harness ALIAS sushi_dsp_benchmark_harness)
target_include_directories(sushi_dsp_benchmark_harness INTERFACE
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
)
sushidsp_require_same_tier_edge(benchmark_harness core)
sushidsp_require_same_tier_edge(benchmark_harness profile)
target_link_libraries(sushi_dsp_benchmark_harness INTERFACE SushiDSP::sushi_dsp_core
    SushiDSP::sushi_dsp_profile)
