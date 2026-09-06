add_library(tubescreamer STATIC
    src/TubeScreamer.cpp
)
add_library(SushiDSP::tubescreamer ALIAS tubescreamer)
target_link_libraries(tubescreamer PUBLIC SushiDSP::sushi_dsp)
sushidsp_apply_optimization_flags(tubescreamer)
target_include_directories(tubescreamer PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
)
