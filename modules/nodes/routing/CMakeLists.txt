# CMakeLists.txt
# SushiDSP - https://github.com/SushiSystems/SushiDSP
# Copyright (c) 2026-present Mustafa Garip & Sushi Systems
# Licensed under the Apache License 2.0. See LICENSE.

add_library(routing STATIC
    source/splitter_node.cpp
    source/mixer_node.cpp
    source/pan_node.cpp
    source/sample_delay_line.cpp
    source/channel_strip_node.cpp
    source/stereo_strip_node.cpp
    source/bus_node.cpp
)
add_library(SushiDSP::routing ALIAS routing)
target_link_libraries(routing PUBLIC SushiDSP::sushi_dsp_core SushiDSP::sushi_dsp_math)
sushidsp_apply_optimization_flags(routing)
sushidsp_apply_sanitizers(routing)
target_include_directories(routing PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
)
