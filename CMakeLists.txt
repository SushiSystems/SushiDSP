cmake_minimum_required(VERSION 3.20)
project(SushiDSP LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_library(sushi_dsp STATIC
    src/core/BufferPool.cpp
    src/core/RigGraph.cpp
    src/math/Fft.cpp
    src/math/Biquad.cpp
)
add_library(SushiDSP::sushi_dsp ALIAS sushi_dsp)
target_include_directories(sushi_dsp PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
)

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
target_link_libraries(sushi_dsp_sdl2 PUBLIC SushiDSP::sushi_dsp PRIVATE SDL2-static)
target_include_directories(sushi_dsp_sdl2 PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
)

add_subdirectory(apps/host)

enable_testing()
add_subdirectory(tests)
