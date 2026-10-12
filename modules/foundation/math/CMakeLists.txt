# CMakeLists.txt
# SushiDSP - https://github.com/SushiSystems/SushiDSP
# Copyright (c) 2026-present Mustafa Garip & Sushi Systems
# Licensed under PolyForm Noncommercial 1.0.0. See LICENSE.
# Commercial use requires a licence from Sushi Systems.

add_library(sushi_dsp_math STATIC
    source/biquad.cpp
    source/fft.cpp
    source/oversampler.cpp
    source/partitioned_convolution.cpp
    source/resampler.cpp
    source/sinc_resampler.cpp
    source/smoothed_parameter.cpp
    source/streaming_convolution.cpp
    source/wdf.cpp
)
add_library(SushiDSP::sushi_dsp_math ALIAS sushi_dsp_math)
sushidsp_apply_optimization_flags(sushi_dsp_math)
sushidsp_apply_warning_flags(sushi_dsp_math)
sushidsp_apply_sanitizers(sushi_dsp_math)
target_include_directories(sushi_dsp_math PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
)
target_include_directories(sushi_dsp_math PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/source)
