# CMakeLists.txt
# SushiDSP - https://github.com/SushiSystems/SushiDSP
# Copyright (c) 2026-present Mustafa Garip & Sushi Systems
# Licensed under PolyForm Noncommercial 1.0.0. See LICENSE.
# Commercial use requires a licence from Sushi Systems.

add_library(room STATIC
    source/materials.cpp
    source/furniture.cpp
    source/air_absorption.cpp
    source/medium_absorption.cpp
    source/band_response_fit.cpp
    source/cabinet_directivity.cpp
    source/cabinet_geometry.cpp
    source/driver_field.cpp
    source/furnishing_field.cpp
    source/microphone_model.cpp
    source/crossover_pair.cpp
    source/hrtf_set.cpp
    source/ambisonic_encoder.cpp
    source/room_scene.cpp
    source/starter_scene.cpp
    source/early_reflection_renderer.cpp
    source/late_reverb.cpp
    source/modal_bank.cpp
    source/spherical_head_hrtf.cpp
    source/hrtf_set_loader.cpp
    source/binaural_decoder.cpp
    source/eyring_decay.cpp
    source/image_source_solver.cpp
    source/late_reverb_design.cpp
    source/modal_solver.cpp
    source/room_render_set.cpp
    source/fade_window.cpp
    source/fir_filter.cpp
    source/frame_delay.cpp
    source/delay_ring.cpp
    source/direct_path.cpp
    source/room_arrangement.cpp
    source/environment_render_set.cpp
    source/source_stage.cpp
    source/pair_stage.cpp
    source/receiver_output.cpp
    source/room_node.cpp
)
add_library(SushiDSP::room ALIAS room)
target_link_libraries(room PUBLIC SushiDSP::sushi_dsp_core SushiDSP::sushi_dsp_log SushiDSP::sushi_dsp_math)
sushidsp_apply_optimization_flags(room)
sushidsp_apply_sanitizers(room)
target_include_directories(room PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
)
