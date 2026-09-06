add_library(peavey5150 STATIC
    src/Peavey5150ToneStack.cpp
    src/Peavey5150.cpp
)
add_library(SushiDSP::peavey5150 ALIAS peavey5150)
target_link_libraries(peavey5150 PUBLIC SushiDSP::sushi_dsp)
sushidsp_apply_optimization_flags(peavey5150)
target_include_directories(peavey5150 PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
)
