add_library(capture STATIC
    src/Take.cpp
    src/TrackRecorder.cpp
    src/RecordingBus.cpp
    src/Transport.cpp
    src/TrackPlayer.cpp
    src/TakeLibrary.cpp
)
add_library(SushiDSP::capture ALIAS capture)
target_link_libraries(capture PUBLIC SushiDSP::sushi_dsp)
sushidsp_apply_optimization_flags(capture)
target_include_directories(capture PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
)
