add_library(jcm800 STATIC
    src/TriodeStage.cpp
    src/PentodeStage.cpp
    src/JcmToneStack.cpp
    src/OutputTransformerStage.cpp
    src/PowerSupplySagStage.cpp
    src/Jcm800.cpp
)
add_library(SushiDSP::jcm800 ALIAS jcm800)
target_link_libraries(jcm800 PUBLIC SushiDSP::sushi_dsp)
sushidsp_apply_optimization_flags(jcm800)
target_include_directories(jcm800 PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
)
