# CMakeLists.txt
# SushiDSP - https://github.com/SushiSystems/SushiDSP
# Copyright (c) 2026-present Mustafa Garip & Sushi Systems
# Licensed under PolyForm Noncommercial 1.0.0. See LICENSE.
# Commercial use requires a licence from Sushi Systems.

# The rig controller and the three bricks behind it are compiled from applications/host_gui by
# source list, the way tests/CMakeLists.txt compiles them; see README.md, "Host sources".
set(SUSHIDSP_PLUGIN_HOST_DIR "${CMAKE_SOURCE_DIR}/applications/host_gui")

add_library(sushidsp_clap MODULE
    clap_entry.cpp
    clap_plugin.cpp
    clap_extensions.cpp
    rig_adapter.cpp
    parameter_routing.cpp
    startup_rig.cpp
    ${SUSHIDSP_PLUGIN_HOST_DIR}/rig_controller.cpp
    ${SUSHIDSP_PLUGIN_HOST_DIR}/slot_graph.cpp
    ${SUSHIDSP_PLUGIN_HOST_DIR}/session_timeline.cpp
    ${SUSHIDSP_PLUGIN_HOST_DIR}/rig_persistence.cpp
    ${SUSHIDSP_PLUGIN_HOST_DIR}/rig_topology.cpp
)
# The host sources include each other by bare name and reach "ui/geometry.hpp" beside them, so
# the host's own directory has to be on the include path.
target_include_directories(sushidsp_clap PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${SUSHIDSP_PLUGIN_HOST_DIR}
)
target_compile_definitions(sushidsp_clap PRIVATE
    SUSHIDSP_PLUGIN_VERSION="${PROJECT_VERSION}"
)
target_link_libraries(sushidsp_clap PRIVATE
    SushiDSP::sushi_dsp
    SushiDSP::node_registry
    SushiDSP::tubescreamer
    SushiDSP::jcm800
    SushiDSP::rig_preset
    SushiDSP::capture
    SushiDSP::room
    SushiDSP::microphone
    nlohmann_json::nlohmann_json
    clap
)
# A CLAP host loads a file named <plugin>.clap; on every platform it is a shared library.
set_target_properties(sushidsp_clap PROPERTIES
    OUTPUT_NAME "SushiDSP"
    PREFIX ""
    SUFFIX ".clap"
)
sushidsp_apply_optimization_flags(sushidsp_clap)
