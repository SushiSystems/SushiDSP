add_executable(sushidsp_host_asio
    main.cpp
)
target_link_libraries(sushidsp_host_asio PRIVATE SushiDSP::sushi_dsp_asio SushiDSP::tubescreamer SushiDSP::jcm800)
