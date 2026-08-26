add_library(node_registry STATIC
    src/NodeRegistry.cpp
)
add_library(SushiDSP::node_registry ALIAS node_registry)
target_link_libraries(node_registry PUBLIC SushiDSP::sushi_dsp SushiDSP::tubescreamer SushiDSP::jcm800 SushiDSP::routing)
sushidsp_apply_optimization_flags(node_registry)
target_include_directories(node_registry PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
)
