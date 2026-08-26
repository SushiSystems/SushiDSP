# ============================================================================
# SushiDSP test pipeline
#
# One binary, sushidsp_tests, covering functional/{unit,integration,
# regression}. Tests are grouped into CTest labels from the GTest suite-name
# prefix convention (Unit_* / Integration_* / Regression_*), matching
# sushiruntime/sushiengine/sushiblas, so `ctest -L unit` selects a sub-suite
# and new tests are picked up automatically as long as they follow it.
#
# "unit" and "regression" discovery calls exist here; the "integration" call
# arrives with the first such test file.
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
    functional/unit/test_denormal_guard.cpp
    functional/unit/test_rig_graph.cpp
    functional/unit/test_splitter_node.cpp
    functional/unit/test_mixer_node.cpp
    functional/unit/test_rig_preset.cpp
    functional/unit/test_rt_graph_slot.cpp
    functional/unit/test_rt_swap_slot.cpp
    functional/unit/test_channel_selection.cpp
    functional/unit/test_node_registry.cpp
    functional/unit/test_spsc_ring.cpp

    # --- Circuits ---
    functional/unit/test_diode_clipper_circuit.cpp

    # --- Products ---
    functional/unit/test_tube_screamer_clipping_stage.cpp
    functional/unit/test_tube_screamer.cpp
    functional/unit/test_triode_stage.cpp
    functional/unit/test_pentode_stage.cpp
    functional/unit/test_jcm_tone_stack.cpp
    functional/unit/test_output_transformer_stage.cpp
    functional/unit/test_power_supply_sag_stage.cpp
    functional/unit/test_jcm800.cpp

    # --- Math ---
    functional/unit/test_biquad.cpp
    functional/unit/test_fft.cpp
    functional/unit/test_newton_raphson_solver.cpp
    functional/unit/test_oversampler.cpp
    functional/unit/test_measure.cpp
    functional/unit/test_spice_csv.cpp
    functional/unit/test_partitioned_convolution.cpp
    functional/unit/test_streaming_convolution.cpp
    functional/unit/test_sinc_resampler.cpp
    functional/unit/test_smoothed_parameter.cpp
    functional/unit/test_wdf.cpp

    # --- IO ---
    functional/unit/test_asio_sample_converter.cpp
    functional/unit/test_ir_loader.cpp
    functional/unit/test_wav_file.cpp

    # --- Real-time safety ---
    functional/unit/test_rt_safety.cpp

    # --- Host diagnostics ---
    functional/unit/test_sine_tone_node.cpp
    ${CMAKE_SOURCE_DIR}/apps/host/SineToneNode.cpp
    functional/unit/test_rig_controller.cpp
    ${CMAKE_SOURCE_DIR}/apps/host_gui/RigController.cpp
    functional/unit/test_preset.cpp
    ${CMAKE_SOURCE_DIR}/apps/host_gui/Preset.cpp

    # --- Regression ---
    functional/regression/test_tube_screamer_regression.cpp
    functional/regression/test_jcm800_regression.cpp
)

target_include_directories(sushidsp_tests PRIVATE ${SD_TEST_COMMON_DIR})
# tests/measure/ and tests/golden/ are header-only and test-side only: never linked into a product.
target_include_directories(sushidsp_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_compile_definitions(sushidsp_tests PRIVATE
    SUSHIDSP_GOLDEN_DIR="${CMAKE_CURRENT_SOURCE_DIR}/golden"
    SUSHIDSP_FIXTURE_DIR="${CMAKE_CURRENT_SOURCE_DIR}/fixtures"
)
target_include_directories(sushidsp_tests PRIVATE ${CMAKE_SOURCE_DIR}/apps/host)
target_include_directories(sushidsp_tests PRIVATE ${CMAKE_SOURCE_DIR}/apps/host_gui)
target_link_libraries(sushidsp_tests PRIVATE
    SushiDSP::sushi_dsp
    SushiDSP::tubescreamer
    SushiDSP::jcm800
    SushiDSP::node_registry
    SushiDSP::routing
    SushiDSP::rig_preset
    GTest::gtest
    # Known limitation: nlohmann_json::nlohmann_json is only available here because
    # enable_testing()/add_subdirectory(tests) in the root CMakeLists.txt runs after the
    # SUSHIDSP_BUILD_GUI block, which FetchContent_MakeAvailable(json)s it. If a future
    # developer configures with SUSHIDSP_BUILD_GUI OFF, this target fails to configure.
    # Out of scope to fix here: every other opt-in module (SDL2, ASIO) already has the
    # same "tests assume it's on" characteristic, e.g. apps/host/SineToneNode.cpp being
    # compiled unconditionally into sushidsp_tests regardless of SUSHIDSP_BUILD_SDL2.
    nlohmann_json::nlohmann_json
)
sushidsp_apply_optimization_flags(sushidsp_tests)

# One label per group; the "functional" umbrella is expressed by the consumer
# as a label regex (`ctest -L 'unit|integration|regression'`) rather than a
# second baked-in label, which does not survive gtest_discover_tests on
# current CMake.
gtest_discover_tests(sushidsp_tests
    DISCOVERY_MODE PRE_TEST
    TEST_FILTER "Unit_*"
    PROPERTIES LABELS "unit"
)

gtest_discover_tests(sushidsp_tests
    DISCOVERY_MODE PRE_TEST
    TEST_FILTER "Regression_*"
    PROPERTIES LABELS "regression"
)

# The timing harness (bench/, see tests/benchmark/CMakeLists.txt for why it is a plain
# executable rather than a GTest binary): registered here, not from the root CMakeLists.txt,
# so it lives under the same "tests" umbrella `sd test` builds and ctest discovers everything
# under.
add_subdirectory(benchmark)
