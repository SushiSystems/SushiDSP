# CMakeLists.txt
# SushiDSP - https://github.com/SushiSystems/SushiDSP
# Copyright (c) 2026-present Mustafa Garip & Sushi Systems
# Licensed under PolyForm Noncommercial 1.0.0. See LICENSE.
# Commercial use requires a licence from Sushi Systems.

# ============================================================================
# SushiDSP test pipeline
#
# One binary, sushidsp_tests, built from the test files listed below. Each test
# is one CTest test, labelled by the macro it is written with: TEST is a `unit`
# test, SUSHIDSP_REGRESSION_TEST (modules/foundation/regression_test/include/SushiDSP/foundation/regression_test/regression_test.hpp) a
# `regression`. The labels are what `ctest -L` selects, so `sd test -s unit`
# and `sd test -s regression` keep their meaning.
#
# The labels come from the test sources at configure time (see
# sushidsp_register_tests below), so a test that is added to an existing file
# is registered the next time CMake runs. Test names follow the suite and test
# names in the source, and they must be unique across the binary.
#
# GoogleTest is sourced via FetchContent rather than vcpkg's
# find_package(GTest CONFIG REQUIRED) that sushiblas uses: SushiDSP has no
# vcpkg triplet wired up yet, and FetchContent keeps this repo buildable
# standalone.
# ============================================================================

cmake_minimum_required(VERSION 3.22)

include(FetchContent)
FetchContent_Declare(
    googletest
    GIT_REPOSITORY https://github.com/google/googletest.git
    GIT_TAG        v1.14.0
)
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)

# Registers every TEST and SUSHIDSP_REGRESSION_TEST of one test source as a CTest test.
# The source is read as text: each case is one line of the form TEST(Suite, Name). A case
# written any other way is missed, so every case keeps that form.
function(sushidsp_register_tests target source)
    set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/${source}")
    file(STRINGS "${CMAKE_CURRENT_SOURCE_DIR}/${source}" _cases
        REGEX "^[ \t]*(TEST|TEST_F|SUSHIDSP_REGRESSION_TEST)[ \t]*\\(")
    foreach(_case IN LISTS _cases)
        if(_case MATCHES "^[ \t]*(TEST|TEST_F|SUSHIDSP_REGRESSION_TEST)[ \t]*\\([ \t]*([A-Za-z0-9_]+)[ \t]*,[ \t]*([A-Za-z0-9_]+)[ \t]*\\)")
            set(_macro "${CMAKE_MATCH_1}")
            set(_test "${CMAKE_MATCH_2}.${CMAKE_MATCH_3}")
            if(_macro STREQUAL "SUSHIDSP_REGRESSION_TEST")
                set(_label "regression")
            else()
                set(_label "unit")
            endif()
            add_test(NAME "${_test}" COMMAND ${target} "--gtest_filter=${_test}")
            set_tests_properties("${_test}" PROPERTIES LABELS "${_label}")
        endif()
    endforeach()
endfunction()

add_executable(sushidsp_tests
    main.cpp

    # --- Core ---
    foundation/core/test_audio_device.cpp
    foundation/core/test_buffer_pool.cpp
    foundation/core/test_denormal_guard.cpp
    foundation/core/test_rig_graph.cpp
    functional/unit/test_rig_graph_output_ports.cpp
    nodes/routing/test_sample_delay_line.cpp
    nodes/routing/test_channel_strip_node.cpp
    nodes/routing/test_stereo_strip_node.cpp
    nodes/routing/test_bus_node.cpp
    nodes/microphone/test_microphone_node.cpp
    nodes/routing/test_splitter_node.cpp
    nodes/routing/test_mixer_node.cpp
    nodes/routing/test_pan_node.cpp
    nodes/capture/test_track_recorder.cpp
    nodes/capture/test_track_player.cpp
    applications/host_gui/test_file_drop_queue.cpp
    nodes/capture/test_transport.cpp
    nodes/capture/test_take_library.cpp
    nodes/capture/test_musical_clock.cpp
    nodes/capture/test_vari_rate_reader.cpp
    nodes/capture/test_metronome.cpp
    nodes/capture/test_track_timeline.cpp
    applications/host_gui/ui/test_timeline_scale.cpp
    session/rig_preset/test_rig_preset.cpp
    foundation/core/test_rt_graph_slot.cpp
    foundation/core/test_rt_swap_slot.cpp
    foundation/audio_format/test_channel_selection.cpp
    catalog/node_registry/test_node_registry.cpp
    foundation/core/test_spsc_ring.cpp

    # --- Log ---
    foundation/log/test_log_formatter.cpp
    functional/unit/test_log_compile_time_cut.cpp
    foundation/log/test_logger.cpp
    foundation/log/test_log_line_queue.cpp
    foundation/log/test_async_log_sink.cpp
    functional/unit/test_track_recorder_logging.cpp
    functional/unit/test_room_node_logging.cpp
    functional/unit/test_directory_listing_logging.cpp

    # --- Circuits ---
    foundation/circuits/models/test_silicon_diode.cpp
    functional/unit/test_triode_12ax7.cpp
    foundation/circuits/models/test_pentode_el34.cpp
    foundation/math/dk/test_dk_circuit.cpp
    functional/unit/test_dk_triode_stage.cpp
    functional/unit/test_dk_op_amp.cpp
    functional/unit/test_ts_clip_stage.cpp
    functional/unit/test_ts_tone_stage.cpp

    # --- Circuits: stages ---
    foundation/circuits/stages/test_triode_stage.cpp
    foundation/circuits/stages/test_pentode_stage.cpp
    foundation/circuits/stages/test_output_transformer_stage.cpp
    foundation/circuits/stages/test_power_supply_sag_stage.cpp
    foundation/circuits/stages/test_resonance_control.cpp
    foundation/circuits/test_graphic_equalizer.cpp
    foundation/circuits/test_fmv_tone_stack.cpp
    foundation/circuits/stages/test_long_tailed_pair.cpp
    foundation/circuits/test_power_amp_feedback.cpp
    foundation/circuits/stages/test_miller_input_capacitance.cpp
    foundation/circuits/stages/test_phase_inverter_presets.cpp
    foundation/circuits/test_output_safety_clamp.cpp
    foundation/circuits/test_audio_taper_pot.cpp

    # --- Products ---
    nodes/tubescreamer/test_tube_screamer.cpp
    nodes/jcm800/test_jcm_tone_stack.cpp
    nodes/jcm800/test_jcm800.cpp
    nodes/cabinet/test_cabinet_node.cpp
    nodes/peavey5150/test_peavey5150.cpp
    nodes/diezel_vh4/test_diezel_vh4.cpp
    nodes/mesa_mark2c/test_mesa_mark2c.cpp

    # --- Math ---
    foundation/math/test_biquad.cpp
    foundation/math/test_fft.cpp
    foundation/math/test_newton_raphson_solver.cpp
    foundation/math/dk/test_damped_newton_solver.cpp
    foundation/math/test_oversampler.cpp
    functional/unit/test_measure.cpp
    functional/unit/test_spice_csv.cpp
    foundation/math/test_partitioned_convolution.cpp
    foundation/math/test_streaming_convolution.cpp
    foundation/math/test_sinc_resampler.cpp
    functional/unit/test_resampler_coefficients.cpp
    foundation/math/test_resampler.cpp
    foundation/math/test_smoothed_parameter.cpp
    foundation/math/test_wdf.cpp

    # --- IO ---
    foundation/audio_format/test_asio_sample_converter.cpp
    foundation/io/test_ir_loader.cpp
    foundation/io/test_wav_file.cpp
    foundation/io/test_wav_writer.cpp

    # --- Real-time safety ---
    functional/unit/test_rt_safety.cpp

    # --- Host diagnostics ---
    applications/host/test_sine_tone_node.cpp
    ${CMAKE_SOURCE_DIR}/applications/host/sine_tone_node.cpp
    applications/host_gui/test_rig_controller.cpp
    ${CMAKE_SOURCE_DIR}/applications/host_gui/rig_controller.cpp
    ${CMAKE_SOURCE_DIR}/applications/host_gui/slot_graph.cpp
    ${CMAKE_SOURCE_DIR}/applications/host_gui/session_timeline.cpp
    ${CMAKE_SOURCE_DIR}/applications/host_gui/rig_persistence.cpp
    applications/plugin/test_rig_adapter.cpp
    ${CMAKE_SOURCE_DIR}/applications/plugin/rig_adapter.cpp
    ${CMAKE_SOURCE_DIR}/applications/plugin/parameter_routing.cpp
    applications/host_gui/test_session.cpp
    ${CMAKE_SOURCE_DIR}/applications/host_gui/session.cpp
    ${CMAKE_SOURCE_DIR}/applications/host_gui/session_renderer.cpp
    ${CMAKE_SOURCE_DIR}/applications/host_gui/session_file.cpp
    ${CMAKE_SOURCE_DIR}/applications/host_gui/session_bounce.cpp
    ${CMAKE_SOURCE_DIR}/applications/host_gui/file_drop_queue.cpp
    ${CMAKE_SOURCE_DIR}/applications/host_gui/mp3_decoder.cpp
    ${CMAKE_SOURCE_DIR}/applications/host_gui/channel_resampler.cpp
    ${CMAKE_SOURCE_DIR}/applications/host_gui/audio_import_cache.cpp
    ${CMAKE_SOURCE_DIR}/applications/host_gui/audio_import_queue.cpp
    applications/host_gui/test_rig_topology.cpp
    applications/host_gui/test_cabinet_section_wiring.cpp
    ${CMAKE_SOURCE_DIR}/applications/host_gui/cabinet_section_wiring.cpp
    ${CMAKE_SOURCE_DIR}/applications/host_gui/ui/mixer_model.cpp
    applications/host_gui/ui/test_scene_camera.cpp
    ${CMAKE_SOURCE_DIR}/applications/host_gui/ui/scene_camera.cpp
    ${CMAKE_SOURCE_DIR}/applications/host_gui/ui/scene_interaction.cpp
    applications/host_gui/ui/test_scene_solids.cpp
    ${CMAKE_SOURCE_DIR}/applications/host_gui/ui/scene_solids.cpp
    ${CMAKE_SOURCE_DIR}/applications/host_gui/rig_topology.cpp
    applications/host_gui/test_topology_applier.cpp
    ${CMAKE_SOURCE_DIR}/applications/host_gui/topology_applier.cpp

    # --- host_gui view layer ---
    # Only the three ImGui-free units are compiled here: sushidsp_tests does not link imgui, and
    # the imgui target only exists when SUSHIDSP_BUILD_GUI is ON.
    applications/host_gui/ui/test_knob_math.cpp
    applications/host_gui/ui/test_panel_library.cpp
    applications/host_gui/ui/test_level_probe.cpp
    applications/host_gui/ui/test_directory_listing.cpp
    ${CMAKE_SOURCE_DIR}/applications/host_gui/ui/knob_math.cpp
    ${CMAKE_SOURCE_DIR}/applications/host_gui/ui/panel_library.cpp
    ${CMAKE_SOURCE_DIR}/applications/host_gui/ui/directory_listing.cpp
    ${CMAKE_SOURCE_DIR}/applications/host_gui/ui/timeline_scale.cpp
    ${CMAKE_SOURCE_DIR}/applications/host_gui/ui/arrangement_layout.cpp
    ${CMAKE_SOURCE_DIR}/applications/host_gui/ui/tap_tempo.cpp
    ${CMAKE_SOURCE_DIR}/applications/host_gui/ui/musical_time_text.cpp
    applications/host_gui/ui/test_split_layout.cpp
    applications/host_gui/ui/test_divider_math.cpp
    applications/host_gui/ui/test_gear_filter.cpp
    applications/host_gui/ui/test_level_text.cpp
    ${CMAKE_SOURCE_DIR}/applications/host_gui/ui/split_layout.cpp
    ${CMAKE_SOURCE_DIR}/applications/host_gui/ui/divider_math.cpp
    ${CMAKE_SOURCE_DIR}/applications/host_gui/ui/gear_filter.cpp
    ${CMAKE_SOURCE_DIR}/applications/host_gui/ui/level_text.cpp
    ${CMAKE_SOURCE_DIR}/applications/host_gui/ui/furnishing_list.cpp
    ${CMAKE_SOURCE_DIR}/applications/host_gui/ui/patch_list.cpp

    # --- DD-3 ---
    nodes/dd3/test_dd3_analog_filters.cpp
    nodes/dd3/test_ne570_compander.cpp
    nodes/dd3/test_dd3_converter.cpp
    nodes/dd3/test_dd3_clock.cpp
    nodes/dd3/test_dd3_delay.cpp
    nodes/room/test_band_response.cpp
    functional/unit/test_room_materials.cpp
    functional/unit/test_room_furniture.cpp
    nodes/room/test_air_absorption.cpp
    nodes/room/test_medium_absorption.cpp
    nodes/room/test_band_response_fit.cpp
    nodes/room/test_cabinet_directivity.cpp
    nodes/room/test_microphone_model.cpp
    functional/unit/test_room_records.cpp
    nodes/room/test_crossover_pair.cpp
    nodes/room/test_hrtf_set.cpp
    nodes/room/test_ambisonic_encoder.cpp
    nodes/room/test_room_scene.cpp
    functional/unit/test_room_scene_furnishings.cpp
    functional/unit/test_room_starter_scene.cpp
    applications/host_gui/ui/test_furnishing_list.cpp
    applications/host_gui/ui/test_patch_list.cpp
    functional/unit/test_room_motus.cpp
    functional/unit/test_room_bras.cpp
    nodes/room/test_early_reflection_renderer.cpp
    nodes/room/test_late_reverb.cpp
    nodes/room/test_modal_bank.cpp
    nodes/room/test_spherical_head_hrtf.cpp
    nodes/room/test_hrtf_set_loader.cpp
    nodes/room/test_binaural_decoder.cpp
    nodes/room/test_eyring_decay.cpp
    nodes/room/test_image_source_solver.cpp
    nodes/room/test_late_reverb_design.cpp
    nodes/room/test_modal_solver.cpp
    nodes/room/test_room_render_set.cpp
    nodes/room/test_room_node.cpp
    functional/unit/test_room_environment.cpp
    nodes/room/test_room_arrangement.cpp
    nodes/room/test_driver_field.cpp
    nodes/room/test_furnishing_field.cpp
    applications/host_gui/test_channel_resampler.cpp
    applications/host_gui/test_audio_import_cache.cpp
    applications/host_gui/test_audio_import_queue.cpp
    applications/host_gui/ui/test_arrangement_layout.cpp
    applications/host_gui/ui/test_tap_tempo.cpp
    applications/host_gui/ui/test_musical_time_text.cpp
    functional/unit/test_dd3_acceptance.cpp

    # --- Regression ---
    functional/regression/test_dd3_regression.cpp
    functional/regression/test_tube_screamer_regression.cpp
    functional/regression/test_jcm800_regression.cpp
    functional/regression/test_peavey5150_regression.cpp
    functional/regression/test_diezel_vh4_regression.cpp
    functional/regression/test_mesa_mark2c_regression.cpp
)

target_compile_definitions(sushidsp_tests PRIVATE
    SUSHIDSP_GOLDEN_DIR="${CMAKE_CURRENT_SOURCE_DIR}/golden"
    SUSHIDSP_FIXTURE_DIR="${CMAKE_CURRENT_SOURCE_DIR}/fixtures"
    SUSHIDSP_SPICE_DIR="${CMAKE_SOURCE_DIR}/docs/reference/spice"
)
target_include_directories(sushidsp_tests PRIVATE ${CMAKE_SOURCE_DIR}/applications/host)
target_include_directories(sushidsp_tests PRIVATE ${CMAKE_SOURCE_DIR}/applications/host_gui)
target_include_directories(sushidsp_tests PRIVATE ${CMAKE_SOURCE_DIR}/applications/plugin)
target_include_directories(sushidsp_tests PRIVATE ${CMAKE_SOURCE_DIR}/third_party)
# The generated resampler kernel is private to the math module; this test checks the table itself.
target_include_directories(sushidsp_tests PRIVATE ${CMAKE_SOURCE_DIR}/modules/foundation/math/source)
# Lets the coefficient tests reach headers the library keeps private, such as the generated
# resampler kernel table.
target_include_directories(sushidsp_tests PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(sushidsp_tests PRIVATE
    SushiDSP::sushi_dsp
    SushiDSP::tubescreamer
    SushiDSP::jcm800
    SushiDSP::cabinet
    SushiDSP::peavey5150
    SushiDSP::diezel_vh4
    SushiDSP::mesa_mark2c
    SushiDSP::dd3
    SushiDSP::node_registry
    SushiDSP::routing
    SushiDSP::capture
    SushiDSP::room
    SushiDSP::microphone
    SushiDSP::rig_preset
    SushiDSP::sushi_dsp_signal_measure
    SushiDSP::sushi_dsp_spice_reference
    SushiDSP::sushi_dsp_circuit_blocks
    SushiDSP::sushi_dsp_golden_compare
    SushiDSP::sushi_dsp_timing_gate
    SushiDSP::sushi_dsp_regression_test
    SushiDSP::room_scenes
    GTest::gtest
    # Known limitation: nlohmann_json::nlohmann_json is only available here because
    # enable_testing()/add_subdirectory(tests) in the root CMakeLists.txt runs after the
    # SUSHIDSP_BUILD_GUI block, which FetchContent_MakeAvailable(json)s it. If a future
    # developer configures with SUSHIDSP_BUILD_GUI OFF, this target fails to configure.
    # Out of scope to fix here: every other opt-in module (SDL2, ASIO) already has the
    # same "tests assume it's on" characteristic, e.g. applications/host/sine_tone_node.cpp being
    # compiled unconditionally into sushidsp_tests regardless of SUSHIDSP_BUILD_SDL2.
    nlohmann_json::nlohmann_json
)
sushidsp_apply_optimization_flags(sushidsp_tests)

# Every test file of the binary is registered. A test file is named test_<name>.cpp; the
# application and support sources compiled into the binary are not.
get_target_property(_sushidsp_test_sources sushidsp_tests SOURCES)
foreach(_source IN LISTS _sushidsp_test_sources)
    if(_source MATCHES "(^|/)test_[^/]*\\.cpp$")
        sushidsp_register_tests(sushidsp_tests "${_source}")
    endif()
endforeach()

# The timing harness (bench/, see tests/benchmark/CMakeLists.txt for why it is a plain
# executable rather than a GTest binary): registered here, not from the root CMakeLists.txt,
# so it lives under the same "tests" umbrella `sd test` builds and ctest discovers everything
# under.
add_subdirectory(benchmark)
