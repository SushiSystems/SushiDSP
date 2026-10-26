# CMakeLists.txt
# SushiDSP - https://github.com/SushiSystems/SushiDSP
# Copyright (c) 2026-present Mustafa Garip & Sushi Systems
# Licensed under PolyForm Noncommercial 1.0.0. See LICENSE.
# Commercial use requires a licence from Sushi Systems.

sushidsp_require_same_tier_edge(device core)
sushidsp_require_same_tier_edge(device audio_format)
sushidsp_require_same_tier_edge(device log)
sushidsp_require_same_tier_edge(device profile)

add_library(sushi_dsp_device INTERFACE)
add_library(SushiDSP::sushi_dsp_device ALIAS sushi_dsp_device)
target_include_directories(sushi_dsp_device INTERFACE
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
)
target_link_libraries(sushi_dsp_device INTERFACE SushiDSP::sushi_dsp_core SushiDSP::sushi_dsp_audio_format
    SushiDSP::sushi_dsp_log SushiDSP::sushi_dsp_profile)

# The SDL2 backend needs SDL2 and the ASIO backend needs the Steinberg SDK; each is built only when
# its option is on and its dependency is present, and the root CMakeLists.txt reads the result.
if(SUSHIDSP_BUILD_SDL2)
    add_library(sushi_dsp_sdl2 STATIC
        source/sdl2_audio_device.cpp
    )
    add_library(SushiDSP::sushi_dsp_sdl2 ALIAS sushi_dsp_sdl2)
    sushidsp_apply_optimization_flags(sushi_dsp_sdl2)
    sushidsp_apply_warning_flags(sushi_dsp_sdl2)
    sushidsp_apply_sanitizers(sushi_dsp_sdl2)
    target_include_directories(sushi_dsp_sdl2 PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/source)
    target_link_libraries(sushi_dsp_sdl2 PUBLIC SushiDSP::sushi_dsp PRIVATE SDL2-static)
endif()

# The Steinberg ASIO SDK is proprietary and each developer vendors it at third_party/asio/ (gitignored).
# Absent SDK or non-Windows platform, this target is skipped and SUSHIDSP_ASIO_AVAILABLE stays unset.
if(SUSHIDSP_BUILD_ASIO AND WIN32 AND EXISTS "${ASIO_SDK_DIR}/common/asio.h")
    add_library(sushi_dsp_asio STATIC
        source/asio_audio_device.cpp
        ${ASIO_SDK_DIR}/common/asio.cpp
        ${ASIO_SDK_DIR}/host/asiodrivers.cpp
        ${ASIO_SDK_DIR}/host/pc/asiolist.cpp
    )
    add_library(SushiDSP::sushi_dsp_asio ALIAS sushi_dsp_asio)
    sushidsp_apply_optimization_flags(sushi_dsp_asio)
    sushidsp_apply_warning_flags(sushi_dsp_asio)
    sushidsp_apply_sanitizers(sushi_dsp_asio)
    target_include_directories(sushi_dsp_asio PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/source
        ${ASIO_SDK_DIR}/common ${ASIO_SDK_DIR}/host ${ASIO_SDK_DIR}/host/pc
    )
    target_link_libraries(sushi_dsp_asio PUBLIC SushiDSP::sushi_dsp PRIVATE ole32 oleaut32)
    message(STATUS "ASIO SDK found at ${ASIO_SDK_DIR} - sushi_dsp_asio target enabled")
    set(SUSHIDSP_ASIO_AVAILABLE ON PARENT_SCOPE)
endif()
