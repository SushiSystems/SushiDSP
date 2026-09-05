cmake_minimum_required(VERSION 3.20)
project(SushiDSP LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Lets tooling type-check one file without a build: clang-tidy, editors, and agents running
# `clang -fsyntax-only` against a single translation unit.
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_CXX_EXTENSIONS OFF)

include(cmake/SushiDSPCompilerFlags.cmake)

add_library(sushi_dsp STATIC
    src/circuits/DiodeClipperCircuit.cpp
    src/core/BufferPool.cpp
    src/core/RigGraph.cpp
    src/core/RtGraphSlot.cpp
    src/device/AsioSampleConverter.cpp
    src/device/ChannelSelection.cpp
    src/io/IrLoader.cpp
    src/io/WavFile.cpp
    src/math/Fft.cpp
    src/math/Biquad.cpp
    src/math/Oversampler.cpp
    src/math/SmoothedParameter.cpp
    src/math/PartitionedConvolution.cpp
    src/math/Resampler.cpp
    src/math/SincResampler.cpp
    src/math/StreamingConvolution.cpp
    src/math/Wdf.cpp
)
add_library(SushiDSP::sushi_dsp ALIAS sushi_dsp)
sushidsp_apply_optimization_flags(sushi_dsp)
target_include_directories(sushi_dsp PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
)

# Products. Added unconditionally: the module has no SDL2 dependency, and the
# test suite links it regardless of SUSHIDSP_BUILD_SDL2.
add_subdirectory(modules/tubescreamer)
add_subdirectory(modules/jcm800)
add_subdirectory(modules/node_registry)
add_subdirectory(modules/routing)
add_subdirectory(modules/dd3)

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
    FetchContent_MakeAvailable(SDL2)

    add_library(sushi_dsp_sdl2 STATIC
        src/device/Sdl2AudioDevice.cpp
    )
    add_library(SushiDSP::sushi_dsp_sdl2 ALIAS sushi_dsp_sdl2)
    sushidsp_apply_optimization_flags(sushi_dsp_sdl2)
    target_link_libraries(sushi_dsp_sdl2 PUBLIC SushiDSP::sushi_dsp PRIVATE SDL2-static)
    target_include_directories(sushi_dsp_sdl2 PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    )

    add_subdirectory(apps/host)
endif()

# The Steinberg ASIO SDK is proprietary (accept-the-license, dual Steinberg/GPLv3) and cannot
# be committed or fetched automatically - each developer vendors it themselves at
# third_party/asio/ (gitignored). Absent SDK or non-Windows platform: this target is silently
# skipped, exactly like a missing SUSHIDSP_BUILD_SDL2 dependency - CI never sets
# ASIO_SDK_DIR, so it is always skipped there, which is correct (Windows-only, no SDK in CI).
set(ASIO_SDK_DIR "${CMAKE_SOURCE_DIR}/third_party/asio" CACHE PATH "Path to the Steinberg ASIO SDK")
option(SUSHIDSP_BUILD_ASIO "Build the ASIO device backend" ON)

if(SUSHIDSP_BUILD_ASIO AND WIN32 AND EXISTS "${ASIO_SDK_DIR}/common/asio.h")
    # Only the SDK's host-side files belong here: common/asiodrvr.cpp, common/combase.cpp,
    # common/register.cpp and common/dllentry.cpp implement the *driver* side of ASIO (they
    # define DllMain and COM class-factory plumbing), and host/ASIOConvertSamples.cpp is
    # superseded by SushiDSP's own AsioSampleConverter.
    add_library(sushi_dsp_asio STATIC
        src/device/AsioAudioDevice.cpp
        ${ASIO_SDK_DIR}/common/asio.cpp
        ${ASIO_SDK_DIR}/host/asiodrivers.cpp
        ${ASIO_SDK_DIR}/host/pc/asiolist.cpp
    )
    add_library(SushiDSP::sushi_dsp_asio ALIAS sushi_dsp_asio)
    sushidsp_apply_optimization_flags(sushi_dsp_asio)
    target_include_directories(sushi_dsp_asio PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    )
    target_include_directories(sushi_dsp_asio PRIVATE
        ${ASIO_SDK_DIR}/common ${ASIO_SDK_DIR}/host ${ASIO_SDK_DIR}/host/pc)
    target_link_libraries(sushi_dsp_asio PUBLIC SushiDSP::sushi_dsp PRIVATE ole32 oleaut32)
    message(STATUS "ASIO SDK found at ${ASIO_SDK_DIR} - sushi_dsp_asio target enabled")
    add_subdirectory(apps/host_asio)
else()
    message(STATUS "ASIO SDK not found at ${ASIO_SDK_DIR} (or not on Windows) - sushi_dsp_asio target skipped")
endif()

# nlohmann::json is a test dependency (tests/CMakeLists.txt links it and compiles
# apps/host_gui/{RigController,Preset}.cpp unconditionally), so it is fetched regardless of
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
option(SUSHIDSP_BUILD_GUI "Build the ImGui rig GUI (apps/host_gui)" ON)

if(SUSHIDSP_BUILD_GUI)
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

    add_subdirectory(apps/host_gui)
endif()

add_subdirectory(modules/rig_preset)

enable_testing()
add_subdirectory(tests)
