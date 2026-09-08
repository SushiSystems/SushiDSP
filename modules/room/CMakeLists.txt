add_library(room STATIC
    src/Materials.cpp
    src/AirAbsorption.cpp
    src/BandResponseFit.cpp
    src/CabinetDirectivity.cpp
    src/MicrophoneModel.cpp
    src/CrossoverPair.cpp
    src/HrtfSet.cpp
    src/AmbisonicEncoder.cpp
    src/RoomScene.cpp
    src/EarlyReflectionRenderer.cpp
    src/LateReverb.cpp
    src/ModalBank.cpp
    src/SphericalHeadHrtf.cpp
    src/HrtfSetLoader.cpp
    src/BinauralDecoder.cpp
)
add_library(SushiDSP::room ALIAS room)
target_link_libraries(room PUBLIC SushiDSP::sushi_dsp)
sushidsp_apply_optimization_flags(room)
target_include_directories(room PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
)
