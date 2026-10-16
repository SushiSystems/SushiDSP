# CMakeLists.txt
# SushiDSP - https://github.com/SushiSystems/SushiDSP
# Copyright (c) 2026-present Mustafa Garip & Sushi Systems
# Licensed under PolyForm Noncommercial 1.0.0. See LICENSE.
# Commercial use requires a licence from Sushi Systems.

add_executable(sushidsp_host
    main.cpp
    sine_tone_node.cpp
)
target_link_libraries(sushidsp_host PRIVATE SushiDSP::sushi_dsp_sdl2 SushiDSP::tubescreamer SushiDSP::jcm800)
sushidsp_apply_optimization_flags(sushidsp_host)

# On Windows sushilog.dll has to sit beside the program.
sushilog_copy_runtime_files(sushidsp_host)
