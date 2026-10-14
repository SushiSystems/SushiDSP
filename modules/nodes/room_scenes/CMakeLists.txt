# CMakeLists.txt
# SushiDSP - https://github.com/SushiSystems/SushiDSP
# Copyright (c) 2026-present Mustafa Garip & Sushi Systems
# Licensed under PolyForm Noncommercial 1.0.0. See LICENSE.
# Commercial use requires a licence from Sushi Systems.

# Internal module: test-side code that the test programs share. Its headers are never
# installed, and no product links it.
sushidsp_require_same_tier_edge(room_scenes room)

add_library(room_scenes INTERFACE)
add_library(SushiDSP::room_scenes ALIAS room_scenes)
target_include_directories(room_scenes INTERFACE
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
)
target_link_libraries(room_scenes INTERFACE
    SushiDSP::room
)
