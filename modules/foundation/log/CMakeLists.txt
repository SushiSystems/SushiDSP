# CMakeLists.txt
# SushiDSP - https://github.com/SushiSystems/SushiDSP
# Copyright (c) 2026-present Mustafa Garip & Sushi Systems
# Licensed under PolyForm Noncommercial 1.0.0. See LICENSE.
# Commercial use requires a licence from Sushi Systems.

# The SD_LOG front over SushiLog. A target whose SUSHIDSP_LOG_STATIC property is ON, such as a
# plug-in a host may load twice, links SushiLog::SushiLogStatic; everything else links the shared
# SushiLog::SushiLog, which a process loads once.
add_library(sushi_dsp_log STATIC
    source/log_capture.cpp
)
add_library(SushiDSP::sushi_dsp_log ALIAS sushi_dsp_log)
sushidsp_apply_optimization_flags(sushi_dsp_log)
sushidsp_apply_warning_flags(sushi_dsp_log)
sushidsp_apply_sanitizers(sushi_dsp_log)
target_include_directories(sushi_dsp_log PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
)
target_link_libraries(sushi_dsp_log PUBLIC
    $<IF:$<BOOL:$<TARGET_PROPERTY:SUSHIDSP_LOG_STATIC>>,SushiLog::SushiLogStatic,SushiLog::SushiLog>
)