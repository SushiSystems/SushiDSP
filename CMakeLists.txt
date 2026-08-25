cmake_minimum_required(VERSION 3.20)
project(SushiDSP LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# TODO(Task 7): add src/core/RigGraph.cpp to the source list below.
add_library(sushi_dsp STATIC
    src/core/BufferPool.cpp
    src/math/Fft.cpp
    src/math/Biquad.cpp
)
add_library(SushiDSP::sushi_dsp ALIAS sushi_dsp)
target_include_directories(sushi_dsp PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
)

enable_testing()
add_subdirectory(tests/unit_dsp)
