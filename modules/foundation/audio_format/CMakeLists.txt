# CMakeLists.txt
# SushiDSP - https://github.com/SushiSystems/SushiDSP
# Copyright (c) 2026-present Mustafa Garip & Sushi Systems
# Licensed under PolyForm Noncommercial 1.0.0. See LICENSE.
# Commercial use requires a licence from Sushi Systems.

add_library(sushi_dsp_audio_format STATIC
    source/asio_sample_converter.cpp
    source/channel_selection.cpp
)
add_library(SushiDSP::sushi_dsp_audio_format ALIAS sushi_dsp_audio_format)
sushidsp_apply_optimization_flags(sushi_dsp_audio_format)
sushidsp_apply_warning_flags(sushi_dsp_audio_format)
sushidsp_apply_sanitizers(sushi_dsp_audio_format)
target_include_directories(sushi_dsp_audio_format PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
)
target_include_directories(sushi_dsp_audio_format PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/source)
