# ============================================================================
# SushiDSP test pipeline
#
# One binary, sushidsp_tests, covering functional/{unit,integration,
# regression}. Tests are grouped into CTest labels from the GTest suite-name
# prefix convention (Unit_* / Integration_* / Regression_*), matching
# sushiruntime/sushiengine/sushiblas, so `ctest -L unit` selects a sub-suite
# and new tests are picked up automatically as long as they follow it.
#
# D0 ships unit tests only, so only the "unit" discovery call exists here;
# integration/regression calls arrive with the first such test file.
#
# GoogleTest is sourced via FetchContent rather than vcpkg's
# find_package(GTest CONFIG REQUIRED) that sushiblas uses: SushiDSP has no
# vcpkg triplet wired up yet, and FetchContent keeps this repo buildable
# standalone.
# ============================================================================

cmake_minimum_required(VERSION 3.22) # gtest_discover_tests(TEST_FILTER ...)

include(FetchContent)
FetchContent_Declare(
    googletest
    GIT_REPOSITORY https://github.com/google/googletest.git
    GIT_TAG        v1.14.0
)
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)

include(GoogleTest)

set(SD_TEST_COMMON_DIR "${CMAKE_CURRENT_SOURCE_DIR}/common")

add_executable(sushidsp_tests
    common/test_main.cpp

    # --- Core ---
    functional/unit/test_audio_device.cpp
    functional/unit/test_buffer_pool.cpp
    functional/unit/test_rig_graph.cpp
    functional/unit/test_spsc_ring.cpp

    # --- Circuits ---
    functional/unit/test_diode_clipper_circuit.cpp

    # --- Tube Screamer (D3) ---
    # The module has no CMake target of its own yet (Task 4 adds
    # modules/tubescreamer/CMakeLists.txt); until then the source is compiled
    # into the test binary directly, the same stopgap apps/host/SineToneNode.cpp
    # used in D1.
    functional/unit/test_tube_screamer_clipping_stage.cpp
    ${CMAKE_SOURCE_DIR}/modules/tubescreamer/src/TubeScreamerClippingStage.cpp
    functional/unit/test_tube_screamer.cpp
    ${CMAKE_SOURCE_DIR}/modules/tubescreamer/src/TubeScreamer.cpp

    # --- Math ---
    functional/unit/test_biquad.cpp
    functional/unit/test_fft.cpp
    functional/unit/test_newton_raphson_solver.cpp
    functional/unit/test_oversampler.cpp
    functional/unit/test_partitioned_convolution.cpp
    functional/unit/test_wdf.cpp

    # --- Real-time safety ---
    functional/unit/test_rt_safety.cpp

    # --- Host diagnostics ---
    functional/unit/test_sine_tone_node.cpp
    ${CMAKE_SOURCE_DIR}/apps/host/SineToneNode.cpp
)

target_include_directories(sushidsp_tests PRIVATE ${SD_TEST_COMMON_DIR})
target_include_directories(sushidsp_tests PRIVATE ${CMAKE_SOURCE_DIR}/apps/host)
target_include_directories(sushidsp_tests PRIVATE ${CMAKE_SOURCE_DIR}/modules/tubescreamer/include)

target_link_libraries(sushidsp_tests PRIVATE
    SushiDSP::sushi_dsp
    GTest::gtest
)

# One label per group; the "functional" umbrella is expressed by the consumer
# as a label regex (`ctest -L 'unit|integration|regression'`) rather than a
# second baked-in label, which does not survive gtest_discover_tests on
# current CMake.
gtest_discover_tests(sushidsp_tests
    DISCOVERY_MODE PRE_TEST
    TEST_FILTER "Unit_*"
    PROPERTIES LABELS "unit"
)
