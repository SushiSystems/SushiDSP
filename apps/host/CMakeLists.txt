add_executable(sushidsp_host
    main.cpp
    SineToneNode.cpp
)
target_link_libraries(sushidsp_host PRIVATE SushiDSP::sushi_dsp_sdl2)
