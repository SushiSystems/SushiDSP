add_library(diezel_vh4 STATIC
    src/DiezelVh4ToneStack.cpp
    src/DiezelVh4.cpp
)
add_library(SushiDSP::diezel_vh4 ALIAS diezel_vh4)
target_link_libraries(diezel_vh4 PUBLIC SushiDSP::sushi_dsp)
sushidsp_apply_optimization_flags(diezel_vh4)
target_include_directories(diezel_vh4 PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
)
