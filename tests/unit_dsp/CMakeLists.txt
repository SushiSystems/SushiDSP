include(FetchContent)
FetchContent_Declare(
    googletest
    GIT_REPOSITORY https://github.com/google/googletest.git
    GIT_TAG        v1.14.0
)
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)

add_executable(unit_dsp
    RigGraphTest.cpp
    BufferPoolTest.cpp
)
target_link_libraries(unit_dsp PRIVATE SushiDSP::sushi_dsp GTest::gtest_main)
target_include_directories(unit_dsp PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

include(GoogleTest)
gtest_discover_tests(unit_dsp)
