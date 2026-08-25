# Timing harness for the audio-thread hot paths. Standard library only, no benchmark
# framework: it reports the best-of-N cost per output sample and how much of one core that is
# at 48 kHz, which is the only number the audio thread cares about. Run it via `sd bench`.
add_executable(sushidsp_bench
    main.cpp
)
target_link_libraries(sushidsp_bench PRIVATE
    SushiDSP::sushi_dsp
    SushiDSP::tubescreamer
    SushiDSP::jcm800
)
