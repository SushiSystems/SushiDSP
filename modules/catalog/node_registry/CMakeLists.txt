# CMakeLists.txt
# SushiDSP - https://github.com/SushiSystems/SushiDSP
# Copyright (c) 2026-present Mustafa Garip & Sushi Systems
# Licensed under PolyForm Noncommercial 1.0.0. See LICENSE.
# Commercial use requires a licence from Sushi Systems.

add_library(node_registry STATIC
    source/node_registry.cpp
)
add_library(SushiDSP::node_registry ALIAS node_registry)
target_link_libraries(node_registry PUBLIC SushiDSP::sushi_dsp_core SushiDSP::tubescreamer SushiDSP::jcm800 SushiDSP::peavey5150 SushiDSP::mesa_mark2c SushiDSP::diezel_vh4 SushiDSP::routing SushiDSP::dd3 SushiDSP::room SushiDSP::microphone SushiDSP::cabinet)
sushidsp_apply_optimization_flags(node_registry)
sushidsp_apply_sanitizers(node_registry)
target_include_directories(node_registry PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
)
