# CMakeLists.txt
# SushiDSP - https://github.com/SushiSystems/SushiDSP
# Copyright (c) 2026-present Mustafa Garip & Sushi Systems
# Licensed under PolyForm Noncommercial 1.0.0. See LICENSE.
# Commercial use requires a licence from Sushi Systems.

# The SD_ZONE front over SushiProfiler. The build writes profile_config.hpp, which the public
# profile headers include; a level change rebuilds only the files that include it, because no
# SD_* definition is public. SushiProfiler::SushiProfiler is the shared target, so every image of
# a process reaches one collector.
if(SD_ENABLE_PROFILER)
    set(SD_ENABLE_PROFILER_VALUE 1)
else()
    set(SD_ENABLE_PROFILER_VALUE 0)
endif()

set(profile_generated "${CMAKE_CURRENT_BINARY_DIR}/include")
configure_file(profile_config.hpp.in
    "${profile_generated}/SushiDSP/foundation/profile/profile_config.hpp" @ONLY)

add_library(sushi_dsp_profile STATIC
    source/profile_record.cpp
    source/profile_runtime.cpp
    source/profile_session.cpp
)
add_library(SushiDSP::sushi_dsp_profile ALIAS sushi_dsp_profile)
sushidsp_apply_optimization_flags(sushi_dsp_profile)
sushidsp_apply_warning_flags(sushi_dsp_profile)
sushidsp_apply_sanitizers(sushi_dsp_profile)
target_include_directories(sushi_dsp_profile PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<BUILD_INTERFACE:${profile_generated}>
)
target_include_directories(sushi_dsp_profile PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/source)
if(SD_ENABLE_PROFILER)
    target_link_libraries(sushi_dsp_profile PUBLIC SushiProfiler::SushiProfiler)
endif()
