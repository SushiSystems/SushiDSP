add_executable(sushidsp_host_gui
    main.cpp
    RigController.cpp
    Preset.cpp
)
target_link_libraries(sushidsp_host_gui PRIVATE
    SushiDSP::sushi_dsp
    SushiDSP::node_registry
    SushiDSP::tubescreamer
    SushiDSP::jcm800
    SushiDSP::sushi_dsp_sdl2
    imgui
    nlohmann_json::nlohmann_json
    opengl32
)
if(TARGET SushiDSP::sushi_dsp_asio)
    target_link_libraries(sushidsp_host_gui PRIVATE SushiDSP::sushi_dsp_asio)
    target_compile_definitions(sushidsp_host_gui PRIVATE SUSHIDSP_HAVE_ASIO=1)
endif()
