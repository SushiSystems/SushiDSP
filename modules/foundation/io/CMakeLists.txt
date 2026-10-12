# CMakeLists.txt
# SushiDSP - https://github.com/SushiSystems/SushiDSP
# Copyright (c) 2026-present Mustafa Garip & Sushi Systems
# Licensed under PolyForm Noncommercial 1.0.0. See LICENSE.
# Commercial use requires a licence from Sushi Systems.

sushidsp_require_same_tier_edge(io math)

add_library(sushi_dsp_io STATIC
    source/ir_loader.cpp
    source/wav_file.cpp
    source/wav_writer.cpp
)
add_library(SushiDSP::sushi_dsp_io ALIAS sushi_dsp_io)
sushidsp_apply_optimization_flags(sushi_dsp_io)
sushidsp_apply_warning_flags(sushi_dsp_io)
sushidsp_apply_sanitizers(sushi_dsp_io)
target_include_directories(sushi_dsp_io PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
)
target_include_directories(sushi_dsp_io PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/source)
target_link_libraries(sushi_dsp_io PUBLIC SushiDSP::sushi_dsp_math)
