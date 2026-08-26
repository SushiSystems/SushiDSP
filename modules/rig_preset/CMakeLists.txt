add_library(rig_preset STATIC
    src/RigPreset.cpp
)
add_library(SushiDSP::rig_preset ALIAS rig_preset)
target_link_libraries(rig_preset PUBLIC SushiDSP::sushi_dsp SushiDSP::node_registry PRIVATE nlohmann_json::nlohmann_json)
sushidsp_apply_optimization_flags(rig_preset)
target_include_directories(rig_preset PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
)
