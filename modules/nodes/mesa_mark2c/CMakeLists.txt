# CMakeLists.txt
# SushiDSP - https://github.com/SushiSystems/SushiDSP
# Copyright (c) 2026-present Mustafa Garip & Sushi Systems
# Licensed under PolyForm Noncommercial 1.0.0. See LICENSE.
# Commercial use requires a licence from Sushi Systems.

add_library(mesa_mark2c STATIC
    source/mesa_mark2c_tone_stack.cpp
    source/mesa_mark2c.cpp
)
add_library(SushiDSP::mesa_mark2c ALIAS mesa_mark2c)
target_link_libraries(mesa_mark2c PUBLIC SushiDSP::sushi_dsp_circuits SushiDSP::sushi_dsp_core SushiDSP::sushi_dsp_math)
sushidsp_apply_optimization_flags(mesa_mark2c)
sushidsp_apply_sanitizers(mesa_mark2c)
target_include_directories(mesa_mark2c PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
)
