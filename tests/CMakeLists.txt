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
    functional/unit/test_pan_node.cpp
    functional/unit/test_track_recorder.cpp
    functional/unit/test_track_player.cpp
    functional/unit/test_file_drop_queue.cpp
    functional/unit/test_transport.cpp
    functional/unit/test_take_library.cpp
    functional/unit/test_musical_clock.cpp
    functional/unit/test_rig_preset.cpp
    functional/unit/test_rt_graph_slot.cpp
    functional/unit/test_rt_swap_slot.cpp
    functional/unit/test_channel_selection.cpp
    functional/unit/test_node_registry.cpp
    functional/unit/test_spsc_ring.cpp

    # --- Circuits ---
    functional/unit/test_silicon_diode.cpp
    functional/unit/test_triode_12ax7.cpp
    functional/unit/test_pentode_el34.cpp
    functional/unit/test_dk_circuit.cpp
    functional/unit/test_dk_triode_stage.cpp
    functional/unit/test_dk_op_amp.cpp
    functional/unit/test_ts_clip_stage.cpp
    functional/unit/test_ts_tone_stage.cpp

    # --- Circuits: stages ---
    functional/unit/test_triode_stage.cpp
    functional/unit/test_pentode_stage.cpp
    functional/unit/test_output_transformer_stage.cpp
    functional/unit/test_power_supply_sag_stage.cpp
    functional/unit/test_resonance_control.cpp
    functional/unit/test_graphic_equalizer.cpp
    functional/unit/test_fmv_tone_stack.cpp

    # --- Products ---
    functional/unit/test_tube_screamer.cpp
    functional/unit/test_jcm_tone_stack.cpp
    functional/unit/test_jcm800.cpp
    functional/unit/test_peavey5150.cpp
    functional/unit/test_diezel_vh4.cpp
    functional/unit/test_mesa_mark2c.cpp

    # --- Math ---
    functional/unit/test_biquad.cpp
    functional/unit/test_fft.cpp
    functional/unit/test_newton_raphson_solver.cpp
    functional/unit/test_damped_newton_solver.cpp
    functional/unit/test_oversampler.cpp
    functional/unit/test_measure.cpp
    functional/unit/test_spice_csv.cpp
    functional/unit/test_partitioned_convolution.cpp
    functional/unit/test_streaming_convolution.cpp
    functional/unit/test_sinc_resampler.cpp
    functional/unit/test_resampler_coefficients.cpp
    functional/unit/test_resampler.cpp
    functional/unit/test_smoothed_parameter.cpp
    functional/unit/test_wdf.cpp

    # --- IO ---
    functional/unit/test_asio_sample_converter.cpp
    functional/unit/test_ir_loader.cpp
    functional/unit/test_wav_file.cpp
    functional/unit/test_wav_writer.cpp

    # --- Real-time safety ---
    functional/unit/test_rt_safety.cpp

    # --- Host diagnostics ---
    functional/unit/test_sine_tone_node.cpp
    ${CMAKE_SOURCE_DIR}/apps/host/SineToneNode.cpp
    functional/unit/test_rig_controller.cpp
    ${CMAKE_SOURCE_DIR}/apps/host_gui/RigController.cpp

    # --- host_gui view layer ---
    # Only the three ImGui-free units are compiled here: sushidsp_tests does not link imgui, and
    # the imgui target only exists when SUSHIDSP_BUILD_GUI is ON.
    functional/unit/test_knob_math.cpp
    functional/unit/test_panel_library.cpp
    functional/unit/test_chain_layout.cpp
    functional/unit/test_level_probe.cpp
    functional/unit/test_directory_listing.cpp
    ${CMAKE_SOURCE_DIR}/apps/host_gui/ui/KnobMath.cpp
    ${CMAKE_SOURCE_DIR}/apps/host_gui/ui/PanelLibrary.cpp
    ${CMAKE_SOURCE_DIR}/apps/host_gui/ui/ChainLayout.cpp
    ${CMAKE_SOURCE_DIR}/apps/host_gui/ui/DirectoryListing.cpp

    # --- DD-3 ---
    functional/unit/test_dd3_analog_filters.cpp
    functional/unit/test_ne570_compander.cpp
    functional/unit/test_dd3_converter.cpp
    functional/unit/test_dd3_clock.cpp
    functional/unit/test_dd3_delay.cpp
    functional/unit/test_dd3_acceptance.cpp

    # --- Regression ---
    functional/regression/test_dd3_regression.cpp
    functional/regression/test_tube_screamer_regression.cpp
    functional/regression/test_jcm800_regression.cpp
    functional/regression/test_peavey5150_regression.cpp
    functional/regression/test_diezel_vh4_regression.cpp
    functional/regression/test_mesa_mark2c_regression.cpp
)

target_include_directories(sushidsp_tests PRIVATE ${SD_TEST_COMMON_DIR})
# tests/measure/ and tests/golden/ are header-only and test-side only: never linked into a product.
target_include_directories(sushidsp_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_compile_definitions(sushidsp_tests PRIVATE
    SUSHIDSP_GOLDEN_DIR="${CMAKE_CURRENT_SOURCE_DIR}/golden"
    SUSHIDSP_FIXTURE_DIR="${CMAKE_CURRENT_SOURCE_DIR}/fixtures"
    SUSHIDSP_SPICE_DIR="${CMAKE_SOURCE_DIR}/docs/reference/spice"
)
target_include_directories(sushidsp_tests PRIVATE ${CMAKE_SOURCE_DIR}/apps/host)
target_include_directories(sushidsp_tests PRIVATE ${CMAKE_SOURCE_DIR}/apps/host_gui)
target_link_libraries(sushidsp_tests PRIVATE
    SushiDSP::sushi_dsp
    SushiDSP::tubescreamer
    SushiDSP::jcm800
    SushiDSP::peavey5150
    SushiDSP::diezel_vh4
    SushiDSP::mesa_mark2c
    SushiDSP::dd3
    SushiDSP::node_registry
    SushiDSP::routing
    SushiDSP::capture
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
