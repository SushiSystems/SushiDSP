cmake_minimum_required(VERSION 3.20)
project(SushiDSP LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# NOTE: No .cpp sources exist yet (Tasks 2-6 add them). add_library() requires
# at least one source, so this is temporarily an INTERFACE library. Switch to
# a STATIC library and add the sources below once they exist:
#     src/core/BufferPool.cpp   # Task 2
#     src/core/RigGraph.cpp     # Task 6
#     src/math/Fft.cpp          # Task 4
#     src/math/Biquad.cpp       # Task 5
add_library(sushi_dsp INTERFACE)
add_library(SushiDSP::sushi_dsp ALIAS sushi_dsp)
target_include_directories(sushi_dsp INTERFACE
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
)

enable_testing()
add_subdirectory(tests/unit_dsp)
