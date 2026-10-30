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

# The static libraries are linked into the CLAP module, a shared library, and into the PIE test
# programs. Those links fail on any object that is not position-independent.
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

# Debug links what Release links: the release C runtime and no iterator debugging, so a Debug tree
# shares one heap and one iterator layout with the Release packages it imports (SushiLog,
# SushiProfiler). CMP0091 is NEW from cmake_minimum_required 3.15 on, so the runtime variable
# is read. Memory errors are the sanitizer trees' job.
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreadedDLL")
set(CMAKE_MAP_IMPORTED_CONFIG_DEBUG Release RelWithDebInfo MinSizeRel "")
if(CMAKE_CXX_COMPILER_ID STREQUAL "MSVC" OR CMAKE_CXX_SIMULATE_ID STREQUAL "MSVC")
    add_compile_definitions("$<$<COMPILE_LANGUAGE:CXX>:_ITERATOR_DEBUG_LEVEL=0>")
endif()

# Debug is the development build: unoptimised, with line tables only, which give a stack trace its
# files and lines at a fraction of full debug information's compile memory, object size and link
# time. CMake states -g for Debug; the later -gline-tables-only overrides it.
if(CMAKE_CXX_COMPILER_ID MATCHES "Clang" AND NOT CMAKE_CXX_COMPILER_FRONTEND_VARIANT STREQUAL "MSVC")
    add_compile_options("$<$<AND:$<CONFIG:Debug>,$<COMPILE_LANGUAGE:CXX>>:-gline-tables-only>")
endif()

include(cmake/SushiDSPCompilerFlags.cmake)
include(cmake/CompilerWarnings.cmake)
include(cmake/Sanitizers.cmake)
include(cmake/SushiDSPLayers.cmake)

# SushiLog, the one logger of the process. The CLI builds or reuses its package and passes
# SushiLog_DIR; nothing is built in this tree and nothing is downloaded.
find_package(SushiLog CONFIG REQUIRED)
message(STATUS "Found SushiLog ${SushiLog_VERSION}: ${SushiLog_DIR}")

# SushiProfiler, which the zones and counters of foundation/profile record into. The CLI builds or
# reuses its package and passes SushiProfiler_DIR. With the option off no SushiProfiler header is
# included and every zone compiles to nothing.
option(SD_ENABLE_PROFILER "Record zones, counters and device spans through SushiProfiler" ON)
set(SD_PROFILE_LEVEL 1 CACHE STRING
    "Deepest zone level compiled in: 0 none, 1 stages, 2 fine zones, 3 per-element zones")
set_property(CACHE SD_PROFILE_LEVEL PROPERTY STRINGS 0 1 2 3)
if(NOT SD_PROFILE_LEVEL MATCHES "^[0-3]$")
    message(FATAL_ERROR "SD_PROFILE_LEVEL is '${SD_PROFILE_LEVEL}'; it must be 0, 1, 2 or 3")
endif()
if(SD_ENABLE_PROFILER)
    find_package(SushiProfiler CONFIG REQUIRED)
    message(STATUS "Found SushiProfiler ${SushiProfiler_VERSION}: ${SushiProfiler_DIR}")
endif()

# The core areas are foundation modules. Each declares its own sources, and each same-tier
# dependency it has is listed in cmake/SushiDSPLayers.cmake.
add_subdirectory(modules/foundation/audio_format)
add_subdirectory(modules/foundation/circuits)
add_subdirectory(modules/foundation/core)
add_subdirectory(modules/foundation/io)
add_subdirectory(modules/foundation/log)
add_subdirectory(modules/foundation/log_capture)
add_subdirectory(modules/foundation/profile)
add_subdirectory(modules/foundation/math)
add_subdirectory(modules/foundation/signal_measure)
add_subdirectory(modules/foundation/spice_reference)
add_subdirectory(modules/foundation/circuit_blocks)
add_subdirectory(modules/foundation/golden_compare)
add_subdirectory(modules/foundation/regression_test)
add_subdirectory(modules/foundation/allocation_trap)
add_subdirectory(modules/foundation/benchmark_harness)

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
    SushiDSP::sushi_dsp_profile
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
add_subdirectory(modules/nodes/room_scenes)
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
# The installed package is the first choice (cli/sushistack.deps.toml declares it, so `hub install`
# and `sd setup` provide it); the fetch is the fallback for a machine that has none.
find_package(nlohmann_json 3.11 QUIET)
if(NOT nlohmann_json_FOUND)
    FetchContent_Declare(
        json
        GIT_REPOSITORY https://github.com/nlohmann/json.git
        GIT_TAG        v3.11.3
    )
    set(JSON_BuildTests OFF CACHE BOOL "" FORCE)
    FetchContent_MakeAvailable(json)
endif()

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

    # The host's MP3 export; see cmake/SushiDSPLame.cmake for why LAME is a shared library.
    include(cmake/SushiDSPLame.cmake)

    add_subdirectory(applications/host_gui)
endif()

add_subdirectory(modules/session/rig_preset)

# The plugin. CLAP is headers only, MIT, and private to applications/plugin.
option(SUSHIDSP_BUILD_PLUGIN "Build the CLAP plugin (applications/plugin)" ON)
if(SUSHIDSP_BUILD_PLUGIN)
    FetchContent_Declare(
        clap
        GIT_REPOSITORY https://github.com/free-audio/clap.git
        GIT_TAG        1.2.2
    )
    FetchContent_MakeAvailable(clap)
    add_subdirectory(applications/plugin)
endif()

enable_testing()
add_subdirectory(tests)
