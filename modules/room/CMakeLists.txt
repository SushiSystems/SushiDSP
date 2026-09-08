add_library(room INTERFACE)
add_library(SushiDSP::room ALIAS room)
target_link_libraries(room INTERFACE SushiDSP::sushi_dsp)
target_include_directories(room INTERFACE
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
)
