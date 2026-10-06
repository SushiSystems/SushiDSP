# CMakeLists.txt
# SushiDSP - https://github.com/SushiSystems/SushiDSP
# Copyright (c) 2026-present Mustafa Garip & Sushi Systems
# Licensed under PolyForm Noncommercial 1.0.0. See LICENSE.
# Commercial use requires a licence from Sushi Systems.

add_library(routing STATIC
    src/SplitterNode.cpp
    src/MixerNode.cpp
    src/PanNode.cpp
)
add_library(SushiDSP::routing ALIAS routing)
target_link_libraries(routing PUBLIC SushiDSP::sushi_dsp)
sushidsp_apply_optimization_flags(routing)
target_include_directories(routing PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
)
