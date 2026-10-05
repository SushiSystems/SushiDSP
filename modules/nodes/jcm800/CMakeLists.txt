# CMakeLists.txt
# SushiDSP - https://github.com/SushiSystems/SushiDSP
# Copyright (c) 2026 Sushi Systems
# Licensed under PolyForm Noncommercial 1.0.0. See LICENSE.
# Commercial use requires a licence from Sushi Systems.

add_library(jcm800 STATIC
    src/JcmToneStack.cpp
    src/Jcm800.cpp
)
add_library(SushiDSP::jcm800 ALIAS jcm800)
target_link_libraries(jcm800 PUBLIC SushiDSP::sushi_dsp)
sushidsp_apply_optimization_flags(jcm800)
target_include_directories(jcm800 PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
)
