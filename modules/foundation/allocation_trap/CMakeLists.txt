# CMakeLists.txt
# SushiDSP - https://github.com/SushiSystems/SushiDSP
# Copyright (c) 2026-present Mustafa Garip & Sushi Systems
# Licensed under PolyForm Noncommercial 1.0.0. See LICENSE.
# Commercial use requires a licence from Sushi Systems.

# Internal module: test-side code that the test programs share. Its headers are never
# installed, and no product links it.

# An object library, so the replaced allocation functions reach every program that links it
# whether or not the program names a symbol of this file.
add_library(sushi_dsp_allocation_trap OBJECT
    source/allocation_trap.cpp
)
add_library(SushiDSP::sushi_dsp_allocation_trap ALIAS sushi_dsp_allocation_trap)
target_include_directories(sushi_dsp_allocation_trap PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
)
target_compile_features(sushi_dsp_allocation_trap PUBLIC cxx_std_17)
sushidsp_apply_sanitizers(sushi_dsp_allocation_trap)
