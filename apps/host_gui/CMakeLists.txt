# CMakeLists.txt
# SushiDSP - https://github.com/SushiSystems/SushiDSP
# Copyright (c) 2026-present Mustafa Garip & Sushi Systems
# Licensed under PolyForm Noncommercial 1.0.0. See LICENSE.
# Commercial use requires a licence from Sushi Systems.

add_executable(sushidsp_host_gui
    main.cpp
    RigController.cpp
    SlotGraph.cpp
    SessionTimeline.cpp
    RigPersistence.cpp
    FileDropQueue.cpp
    Mp3Decoder.cpp
    ChannelResampler.cpp
    AudioImportCache.cpp
    AudioImportQueue.cpp
    ui/KnobMath.cpp
    ui/PanelLibrary.cpp
    ui/RigChainView.cpp
    RigTopology.cpp
    TopologyApplier.cpp
    ui/Divider.cpp
    ui/SplitLayout.cpp
    ui/DividerMath.cpp
    ui/GearFilter.cpp
    ui/LevelText.cpp
    ui/Theme.cpp
    ui/Widgets.cpp
    ui/StagePanel.cpp
    ui/GearBrowser.cpp
    ui/MeterBar.cpp
    ui/TopBar.cpp
    ui/DeviceSettings.cpp
    ui/DirectoryListing.cpp
    ui/FileBrowser.cpp
    ui/RoomPanel.cpp
    ui/FurnishingList.cpp
    ui/PatchList.cpp
    ui/FurnishingList.cpp
    ui/PatchList.cpp
    ui/TransportBar.cpp
    ui/TimelineScale.cpp
    ui/ArrangementView.cpp
    ui/ArrangementLayout.cpp
    ui/TapTempo.cpp
    ui/MusicalTimeText.cpp
)
# ui/*.cpp include their own headers as "ui/<Name>.hpp" and reach RigController.hpp beside them,
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
    imgui
    nlohmann_json::nlohmann_json
    OpenGL::GL
)
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
