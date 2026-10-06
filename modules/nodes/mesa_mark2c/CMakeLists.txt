# CMakeLists.txt
# SushiDSP - https://github.com/SushiSystems/SushiDSP
# Copyright (c) 2026-present Mustafa Garip & Sushi Systems
# Licensed under PolyForm Noncommercial 1.0.0. See LICENSE.
# Commercial use requires a licence from Sushi Systems.

add_library(mesa_mark2c STATIC
    src/MesaMark2cToneStack.cpp
    src/MesaMark2c.cpp
)
add_library(SushiDSP::mesa_mark2c ALIAS mesa_mark2c)
target_link_libraries(mesa_mark2c PUBLIC SushiDSP::sushi_dsp)
sushidsp_apply_optimization_flags(mesa_mark2c)
target_include_directories(mesa_mark2c PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
)
