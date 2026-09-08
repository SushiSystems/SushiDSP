add_executable(sushidsp_host_gui
    main.cpp
    RigController.cpp
    FileDropQueue.cpp
    ui/KnobMath.cpp
    ui/PanelLibrary.cpp
    ui/ChainLayout.cpp
    ui/Theme.cpp
    ui/Widgets.cpp
    ui/StagePanel.cpp
    ui/ChainStrip.cpp
    ui/GearBrowser.cpp
    ui/MeterBar.cpp
    ui/TopBar.cpp
    ui/DeviceSettings.cpp
    ui/DirectoryListing.cpp
    ui/FileBrowser.cpp
    ui/TransportBar.cpp
    ui/TimelineScale.cpp
    ui/ArrangementView.cpp
    ui/ArrangementLayout.cpp
    ui/TapTempo.cpp
    ui/MusicalTimeText.cpp
)
# ui/*.cpp include their own headers as "ui/<Name>.hpp" and reach RigController.hpp beside them,
# so the app directory itself has to be on the include path.
target_include_directories(sushidsp_host_gui PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(sushidsp_host_gui PRIVATE
    SushiDSP::sushi_dsp
    SushiDSP::node_registry
    SushiDSP::tubescreamer
    SushiDSP::jcm800
    SushiDSP::sushi_dsp_sdl2
    SushiDSP::rig_preset
    SushiDSP::capture
    imgui
    nlohmann_json::nlohmann_json
    opengl32
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
