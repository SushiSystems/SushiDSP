# CMakeLists.txt
# SushiDSP - https://github.com/SushiSystems/SushiDSP
# Copyright (c) 2026-present Mustafa Garip & Sushi Systems
# Licensed under PolyForm Noncommercial 1.0.0. See LICENSE.
# Commercial use requires a licence from Sushi Systems.

sushidsp_require_same_tier_edge(circuits log)
sushidsp_require_same_tier_edge(circuits math)

add_library(sushi_dsp_circuits STATIC
    source/fmv_tone_stack.cpp
    source/graphic_equalizer.cpp
    source/power_amp_feedback.cpp
    source/stages/long_tailed_pair.cpp
    source/stages/output_transformer_stage.cpp
    source/stages/pentode_stage.cpp
    source/stages/power_supply_sag_stage.cpp
    source/stages/resonance_control.cpp
    source/stages/triode_stage.cpp
)
add_library(SushiDSP::sushi_dsp_circuits ALIAS sushi_dsp_circuits)
sushidsp_apply_optimization_flags(sushi_dsp_circuits)
sushidsp_apply_warning_flags(sushi_dsp_circuits)
sushidsp_apply_sanitizers(sushi_dsp_circuits)
target_include_directories(sushi_dsp_circuits PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
)
target_include_directories(sushi_dsp_circuits PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/source)
target_link_libraries(sushi_dsp_circuits PUBLIC SushiDSP::sushi_dsp_log SushiDSP::sushi_dsp_math)
