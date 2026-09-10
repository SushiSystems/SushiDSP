add_library(cabinet STATIC
    src/CabinetNode.cpp
)
add_library(SushiDSP::cabinet ALIAS cabinet)
target_link_libraries(cabinet PUBLIC SushiDSP::sushi_dsp)
sushidsp_apply_optimization_flags(cabinet)
target_include_directories(cabinet PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
)
