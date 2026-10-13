# CMakeLists.txt
# SushiDSP - https://github.com/SushiSystems/SushiDSP
# Copyright (c) 2026-present Mustafa Garip & Sushi Systems
# Licensed under PolyForm Noncommercial 1.0.0. See LICENSE.
# Commercial use requires a licence from Sushi Systems.

cmake_minimum_required(VERSION 3.20)
project(SushiDSP VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Lets tooling type-check one file without a build: clang-tidy, editors, and agents running
# `clang -fsyntax-only` against a single translation unit.
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_CXX_EXTENSIONS OFF)

include(cmake/SushiDSPCompilerFlags.cmake)
include(cmake/CompilerWarnings.cmake)
include(cmake/Sanitizers.cmake)
include(cmake/SushiDSPLayers.cmake)

# The core areas are foundation modules. Each declares its own sources, and each same-tier
# dependency it has is listed in cmake/SushiDSPLayers.cmake.
add_subdirectory(modules/foundation/audio_format)
add_subdirectory(modules/foundation/circuits)
add_subdirectory(modules/foundation/core)
add_subdirectory(modules/foundation/io)
add_subdirectory(modules/foundation/log)
add_subdirectory(modules/foundation/math)

# sushi_dsp bundles the foundation for consumers. A consumer that needs one module links it
# directly instead.
add_library(sushi_dsp INTERFACE)
add_library(SushiDSP::sushi_dsp ALIAS sushi_dsp)
target_link_libraries(sushi_dsp INTERFACE
    SushiDSP::sushi_dsp_audio_format
    SushiDSP::sushi_dsp_circuits
    SushiDSP::sushi_dsp_core
    SushiDSP::sushi_dsp_io
    SushiDSP::sushi_dsp_log
    SushiDSP::sushi_dsp_math
    SushiDSP::sushi_dsp_device
)

# Products. Added unconditionally: the module has no SDL2 dependency, and the
# test suite links it regardless of SUSHIDSP_BUILD_SDL2.
add_subdirectory(modules/nodes/tubescreamer)
add_subdirectory(modules/nodes/jcm800)
add_subdirectory(modules/catalog/node_registry)
add_subdirectory(modules/nodes/routing)
add_subdirectory(modules/nodes/dd3)
add_subdirectory(modules/nodes/peavey5150)
add_subdirectory(modules/nodes/diezel_vh4)
add_subdirectory(modules/nodes/mesa_mark2c)
add_subdirectory(modules/nodes/capture)
add_subdirectory(modules/nodes/room)
add_subdirectory(modules/nodes/microphone)
add_subdirectory(modules/nodes/cabinet)

# Nothing in the test suite links SDL2, so this can be turned off to configure
# and run the tests without network access or an SDL2 build.
option(SUSHIDSP_BUILD_SDL2 "Build the SDL2 device backend and the standalone host app" ON)

if(SUSHIDSP_BUILD_SDL2)
    include(FetchContent)
    FetchContent_Declare(
        SDL2
        GIT_REPOSITORY https://github.com/libsdl-org/SDL.git
        GIT_TAG release-2.30.0
    )
    set(SDL_SHARED OFF CACHE BOOL "" FORCE)
    set(SDL_STATIC ON CACHE BOOL "" FORCE)
    set(SDL_TEST OFF CACHE BOOL "" FORCE)
    # SDL2 drives audio only; its Wayland protocol objects collide with GLFW's at link time.
    set(SDL_WAYLAND OFF CACHE BOOL "" FORCE)
    FetchContent_MakeAvailable(SDL2)
endif()

# The Steinberg ASIO SDK is proprietary (accept-the-license, dual Steinberg/GPLv3) and cannot
# be committed or fetched automatically - each developer vendors it themselves at
# third_party/asio/ (gitignored). Absent SDK or non-Windows platform: this target is silently
# skipped, exactly like a missing SUSHIDSP_BUILD_SDL2 dependency - CI never sets
# ASIO_SDK_DIR, so it is always skipped there, which is correct (Windows-only, no SDK in CI).
set(ASIO_SDK_DIR "${CMAKE_SOURCE_DIR}/third_party/asio" CACHE PATH "Path to the Steinberg ASIO SDK")
option(SUSHIDSP_BUILD_ASIO "Build the ASIO device backend" ON)

# The device module builds the SDL2 and ASIO backends; it reads the two options above.
add_subdirectory(modules/foundation/device)

if(SUSHIDSP_BUILD_SDL2)
    add_subdirectory(applications/host)
endif()

if(SUSHIDSP_ASIO_AVAILABLE)
    add_subdirectory(applications/host_asio)
else()
    message(STATUS "ASIO SDK not found at ${ASIO_SDK_DIR} (or not on Windows) - sushi_dsp_asio target skipped")
endif()

# nlohmann::json is a test dependency (tests/CMakeLists.txt links it and compiles
# applications/host_gui/{RigController,Preset}.cpp unconditionally), so it is fetched regardless of
# SUSHIDSP_BUILD_GUI - only glfw/imgui, which nothing outside the GUI needs, stay gated below.
include(FetchContent)
FetchContent_Declare(
    json
    GIT_REPOSITORY https://github.com/nlohmann/json.git
    GIT_TAG        v3.11.3
)
set(JSON_BuildTests OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(json)

# ImGui/GLFW are fetched only when the GUI is built; CI can disable this exactly like
# SUSHIDSP_BUILD_SDL2/SUSHIDSP_BUILD_ASIO to skip the extra network fetch and build time when
# the GUI isn't needed.
option(SUSHIDSP_BUILD_GUI "Build the ImGui rig GUI (applications/host_gui); needs SUSHIDSP_BUILD_SDL2" ON)

if(SUSHIDSP_BUILD_GUI AND NOT SUSHIDSP_BUILD_SDL2)
    message(STATUS "SUSHIDSP_BUILD_GUI needs the SDL2 device backend - GUI skipped")
endif()

if(SUSHIDSP_BUILD_GUI AND SUSHIDSP_BUILD_SDL2)
    FetchContent_Declare(
        glfw
        GIT_REPOSITORY https://github.com/glfw/glfw.git
        GIT_TAG        3.4
    )
    set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
    set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
    set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
    FetchContent_MakeAvailable(glfw)

    FetchContent_Declare(
        imgui
        GIT_REPOSITORY https://github.com/ocornut/imgui.git
        GIT_TAG        v1.90.9
    )
    FetchContent_MakeAvailable(imgui)

    add_library(imgui STATIC
        ${imgui_SOURCE_DIR}/imgui.cpp
        ${imgui_SOURCE_DIR}/imgui_draw.cpp
        ${imgui_SOURCE_DIR}/imgui_tables.cpp
        ${imgui_SOURCE_DIR}/imgui_widgets.cpp
        ${imgui_SOURCE_DIR}/imgui_demo.cpp
        ${imgui_SOURCE_DIR}/backends/imgui_impl_glfw.cpp
        ${imgui_SOURCE_DIR}/backends/imgui_impl_opengl3.cpp
    )
    target_include_directories(imgui PUBLIC
        ${imgui_SOURCE_DIR}
        ${imgui_SOURCE_DIR}/backends
    )
    target_link_libraries(imgui PUBLIC glfw)

    add_subdirectory(applications/host_gui)
endif()

add_subdirectory(modules/session/rig_preset)

enable_testing()
add_subdirectory(tests)
