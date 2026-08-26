add_library(routing STATIC
    src/SplitterNode.cpp
)
add_library(SushiDSP::routing ALIAS routing)
target_link_libraries(routing PUBLIC SushiDSP::sushi_dsp)
sushidsp_apply_optimization_flags(routing)
target_include_directories(routing PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
)
