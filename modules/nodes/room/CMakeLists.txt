# CMakeLists.txt
# SushiDSP - https://github.com/SushiSystems/SushiDSP
# Copyright (c) 2026-present Mustafa Garip & Sushi Systems
# Licensed under PolyForm Noncommercial 1.0.0. See LICENSE.
# Commercial use requires a licence from Sushi Systems.

add_library(room STATIC
    src/Materials.cpp
    src/Furniture.cpp
    src/AirAbsorption.cpp
    src/MediumAbsorption.cpp
    src/BandResponseFit.cpp
    src/CabinetDirectivity.cpp
    src/MicrophoneModel.cpp
    src/CrossoverPair.cpp
    src/HrtfSet.cpp
    src/AmbisonicEncoder.cpp
    src/RoomScene.cpp
    src/StarterScene.cpp
    src/EarlyReflectionRenderer.cpp
    src/LateReverb.cpp
    src/ModalBank.cpp
    src/SphericalHeadHrtf.cpp
    src/HrtfSetLoader.cpp
    src/BinauralDecoder.cpp
    src/EyringDecay.cpp
    src/ImageSourceSolver.cpp
    src/LateReverbDesign.cpp
    src/ModalSolver.cpp
    src/RoomRenderSet.cpp
    src/FadeWindow.cpp
    src/FirFilter.cpp
    src/FrameDelay.cpp
    src/DelayRing.cpp
    src/DirectPath.cpp
    src/ModalPath.cpp
    src/GeometricPath.cpp
    src/BinauralStage.cpp
    src/RoomNode.cpp
)
add_library(SushiDSP::room ALIAS room)
target_link_libraries(room PUBLIC SushiDSP::sushi_dsp)
sushidsp_apply_optimization_flags(room)
target_include_directories(room PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
)
