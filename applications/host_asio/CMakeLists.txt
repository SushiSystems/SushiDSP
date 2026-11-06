# CMakeLists.txt
# SushiDSP - https://github.com/SushiSystems/SushiDSP
# Copyright (c) 2026-present Mustafa Garip & Sushi Systems
# Licensed under the Apache License 2.0. See LICENSE.

add_executable(sushidsp_host_asio
    main.cpp
)
target_link_libraries(sushidsp_host_asio PRIVATE SushiDSP::sushi_dsp_asio SushiDSP::tubescreamer SushiDSP::jcm800 SushiDSP::cabinet)
