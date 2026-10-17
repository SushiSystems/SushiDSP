# CMakeLists.txt
# SushiDSP - https://github.com/SushiSystems/SushiDSP
# Copyright (c) 2026-present Mustafa Garip & Sushi Systems
# Licensed under PolyForm Noncommercial 1.0.0. See LICENSE.
# Commercial use requires a licence from Sushi Systems.

add_executable(sushidsp_host_gui
    main.cpp
    rig_controller.cpp
    slot_graph.cpp
    session_timeline.cpp
    rig_persistence.cpp
    file_drop_queue.cpp
    mp3_decoder.cpp
    channel_resampler.cpp
    audio_import_cache.cpp
    audio_import_queue.cpp
    ui/knob_math.cpp
    ui/panel_library.cpp
    ui/rig_chain_view.cpp
    rig_topology.cpp
    cabinet_section_wiring.cpp
    topology_applier.cpp
    ui/divider.cpp
    ui/split_layout.cpp
    ui/divider_math.cpp
    ui/gear_filter.cpp
    ui/level_text.cpp
    ui/theme.cpp
    ui/widgets.cpp
    ui/stage_panel.cpp
    ui/gear_browser.cpp
    ui/meter_bar.cpp
    ui/top_bar.cpp
    ui/device_settings.cpp
    ui/directory_listing.cpp
    ui/file_browser.cpp
    ui/room_panel.cpp
    ui/scene_camera.cpp
    ui/scene_interaction.cpp
    ui/scene_solids.cpp
    ui/room_scene_view.cpp
    ui/mixer_model.cpp
    ui/mixer_panel.cpp
    ui/cabinet_section_pane.cpp
    ui/furnishing_list.cpp
    ui/patch_list.cpp
    ui/transport_bar.cpp
    ui/timeline_scale.cpp
    ui/arrangement_view.cpp
    ui/arrangement_layout.cpp
    ui/tap_tempo.cpp
    ui/musical_time_text.cpp
    session.cpp
    session_renderer.cpp
    session_file.cpp
    session_bounce.cpp
)
# ui/*.cpp include their own headers as "ui/<Name>.hpp" and reach rig_controller.hpp beside them,
# so the app directory itself has to be on the include path.
target_include_directories(sushidsp_host_gui PRIVATE ${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_SOURCE_DIR}/third_party)
find_package(OpenGL REQUIRED)
target_link_libraries(sushidsp_host_gui PRIVATE
    SushiDSP::sushi_dsp
    SushiDSP::node_registry
    SushiDSP::tubescreamer
    SushiDSP::jcm800
    SushiDSP::sushi_dsp_sdl2
    SushiDSP::rig_preset
    SushiDSP::capture
    SushiDSP::room
    SushiDSP::microphone
    imgui
    nlohmann_json::nlohmann_json
    OpenGL::GL
)
# The GUI links the DSP core, so it takes the same ISA and LTO policy as sushidsp_host. Without
# it, the link does not use ld.lld and the GNU linker rejects the LTO objects in the libraries.
sushidsp_apply_optimization_flags(sushidsp_host_gui)
if(TARGET SushiDSP::sushi_dsp_asio)
    target_link_libraries(sushidsp_host_gui PRIVATE SushiDSP::sushi_dsp_asio)
    target_compile_definitions(sushidsp_host_gui PRIVATE SUSHIDSP_HAVE_ASIO=1)
endif()

# The host resolves assets/ir/default_4x12.wav relative to its own executable, so the assets
# directory has to sit beside the binary in the build tree too. The file itself is optional
# (see assets/ir/README.md); the guard keeps a checkout without one from failing the build.
if(EXISTS "${CMAKE_SOURCE_DIR}/assets/ir")
    add_custom_command(TARGET sushidsp_host_gui POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory
            "${CMAKE_SOURCE_DIR}/assets/ir"
            "$<TARGET_FILE_DIR:sushidsp_host_gui>/assets/ir"
        COMMENT "Copying assets/ir next to sushidsp_host_gui"
    )
endif()

# The room's binaural path resolves assets/hrtf next to its own executable the same way. No
# measured set ships in the tree (see assets/hrtf/README.md); without one the node keeps the
# spherical-head fallback it builds for itself, so the guard costs a checkout nothing.
if(EXISTS "${CMAKE_SOURCE_DIR}/assets/hrtf")
    add_custom_command(TARGET sushidsp_host_gui POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory
            "${CMAKE_SOURCE_DIR}/assets/hrtf"
            "$<TARGET_FILE_DIR:sushidsp_host_gui>/assets/hrtf"
        COMMENT "Copying assets/hrtf next to sushidsp_host_gui"
    )
endif()

# MP3 export is LAME's, linked dynamically; its DLL has to sit beside the executable.
if(SUSHIDSP_LAME_AVAILABLE)
    target_sources(sushidsp_host_gui PRIVATE mp3_encoder.cpp)
    target_link_libraries(sushidsp_host_gui PRIVATE mp3lame)
    target_compile_definitions(sushidsp_host_gui PRIVATE SUSHIDSP_HAVE_LAME=1)
    add_custom_command(TARGET sushidsp_host_gui POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "$<TARGET_FILE:mp3lame>"
            "$<TARGET_FILE_DIR:sushidsp_host_gui>"
        COMMENT "Copying the LAME library next to sushidsp_host_gui")
endif()

# On Windows sushilog.dll has to sit beside the program.
sushilog_copy_runtime_files(sushidsp_host_gui)
