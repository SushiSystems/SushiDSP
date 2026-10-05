# CMakeLists.txt
# SushiDSP - https://github.com/SushiSystems/SushiDSP
# Copyright (c) 2026 Sushi Systems
# Licensed under PolyForm Noncommercial 1.0.0. See LICENSE.
# Commercial use requires a licence from Sushi Systems.

add_library(node_registry STATIC
    src/NodeRegistry.cpp
)
add_library(SushiDSP::node_registry ALIAS node_registry)
target_link_libraries(node_registry PUBLIC SushiDSP::sushi_dsp SushiDSP::tubescreamer SushiDSP::jcm800 SushiDSP::peavey5150 SushiDSP::mesa_mark2c SushiDSP::diezel_vh4 SushiDSP::routing SushiDSP::dd3 SushiDSP::room SushiDSP::cabinet)
sushidsp_apply_optimization_flags(node_registry)
target_include_directories(node_registry PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
)
