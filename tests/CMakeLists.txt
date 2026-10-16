# CMakeLists.txt
# SushiDSP - https://github.com/SushiSystems/SushiDSP
# Copyright (c) 2026-present Mustafa Garip & Sushi Systems
# Licensed under PolyForm Noncommercial 1.0.0. See LICENSE.
# Commercial use requires a licence from Sushi Systems.

# ============================================================================
# SushiDSP test pipeline
#
# One binary, sushidsp_tests, built from the test files listed below. Each test
# is one CTest test labelled with its size, small, medium or large, read from
# its suite-name prefix, which also sets its timeout. `sd test --label <size>`
# selects a size. The tests are discovered from the built binary before each
# run, so a new test needs no edit here.
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

add_executable(sushidsp_tests
    main.cpp
    # --- Unit tests written for the units that had none ---
    nodes/room/test_room_log_category.cpp
    nodes/room/test_fade_window.cpp
    nodes/room/test_stage_limits.cpp
    nodes/room/test_delay_ring.cpp
    nodes/room/test_frame_delay.cpp
    nodes/room/test_fir_filter.cpp
    nodes/room/test_diffuse_field.cpp
    nodes/room/test_cabinet_geometry.cpp
    nodes/room/test_early_tap_set.cpp
    nodes/room/test_fdn_design.cpp
    nodes/room/test_modal_design.cpp
    nodes/room/test_room_host.cpp
    nodes/room/test_pair_stage.cpp
    nodes/room/test_source_stage.cpp
    nodes/room/test_direct_path.cpp
    nodes/room/test_cascade_filter.cpp
    nodes/room/test_receiver_output.cpp
    # --- Unit tests written for the units that had none ---
    nodes/capture/test_capture_log_category.cpp
    nodes/capture/test_recording_bus.cpp
    nodes/capture/test_take.cpp
    nodes/dd3/test_wet_fifo.cpp
    nodes/diezel_vh4/test_diezel_vh4_tone_stack.cpp
    nodes/mesa_mark2c/test_mesa_mark2c_tone_stack.cpp
    nodes/peavey5150/test_peavey5150_tone_stack.cpp
    nodes/room_scenes/test_room_fixtures.cpp
    nodes/room_scenes/test_room_ports.cpp
    # --- Unit tests written for the units that had none ---
    foundation/circuit_blocks/test_dk_block_node.cpp
    foundation/circuit_blocks/test_jcm_v1a_netlist.cpp
    foundation/spice_reference/test_spice_block_compare.cpp
    # --- Unit tests written for the units that had none ---
    foundation/signal_measure/test_render.cpp
    foundation/signal_measure/test_window.cpp
    foundation/signal_measure/test_spectrum.cpp
    foundation/signal_measure/test_thd.cpp
    foundation/signal_measure/test_frequency_response.cpp
    foundation/signal_measure/test_alias_floor.cpp
    foundation/signal_measure/test_null_test.cpp
    # --- Unit tests written for the units that had none ---
    foundation/circuits/models/test_soft_transition.cpp
    foundation/circuits/stages/test_plate_swing.cpp
    foundation/circuits/stages/test_grid_conduction_law.cpp
    foundation/circuits/stages/test_antiderivative_antialiaser.cpp
    foundation/circuits/stages/test_power_tube_presets.cpp
    # --- Unit tests written for the units that had none ---
    foundation/math/dk/test_dense_lu.cpp
    foundation/math/dk/test_dk_netlist.cpp
    foundation/math/dk/test_linear_port.cpp
    foundation/math/test_halfband_coefficients.cpp
    # --- Unit tests written for the units that had none ---
    foundation/io/test_ir.cpp
    foundation/regression_test/test_regression_test.cpp
    foundation/timing_gate/test_timing_gate.cpp
    foundation/golden_compare/test_golden_compare_log_category.cpp
    foundation/golden_compare/test_golden_file.cpp
    # --- Unit tests written for the units that had none ---
    foundation/core/test_cabinet_host.cpp
    foundation/core/test_calibration.cpp
    foundation/core/test_node_descriptor.cpp
    foundation/core/test_parameter_change.cpp
    foundation/core/test_parameter_table.cpp
    foundation/core/test_port_layout.cpp
    # --- Unit tests written for the units that had none ---
    foundation/log/test_log_level.cpp
    foundation/log/test_log_category.cpp
    foundation/log/test_log_argument.cpp
    foundation/log/test_log_log_category.cpp
    foundation/log/test_log_record.cpp
    foundation/log/test_log_record_sink.cpp
    foundation/log/test_log_sink.cpp
    foundation/log/test_delegate.cpp
    foundation/log/test_log_project.cpp
    foundation/log/test_sushidsp_log_project.cpp
    foundation/log/test_sushi_log_record.cpp
    foundation/log/test_scoped_log_sink.cpp
    foundation/log/test_stderr_log_sink.cpp
    foundation/log/test_default_logger.cpp
    foundation/log/test_capturing_log_sink.cpp
    foundation/log/test_log_delivery.cpp
    foundation/log/test_log_level_setting.cpp
    foundation/log/test_log_level_table.cpp
    foundation/log/test_log_line.cpp
    foundation/log/test_log_site.cpp
    foundation/log/test_log_thread.cpp

    # --- Core ---
    foundation/core/test_audio_device.cpp
    foundation/core/test_buffer_pool.cpp
    foundation/core/test_denormal_guard.cpp
    foundation/core/test_rig_graph/test_behaviour.cpp
    foundation/core/test_rig_graph/test_output_ports.cpp
    nodes/routing/test_sample_delay_line.cpp
    nodes/routing/test_channel_strip_node.cpp
    nodes/routing/test_stereo_strip_node.cpp
    nodes/routing/test_bus_node.cpp
    nodes/microphone/test_microphone_node.cpp
    nodes/routing/test_splitter_node.cpp
    nodes/routing/test_mixer_node.cpp
    nodes/routing/test_pan_node.cpp
    nodes/capture/test_track_recorder/test_behaviour.cpp
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
    foundation/log/test_log.cpp
    foundation/log/test_logger.cpp
    foundation/log/test_log_line_queue.cpp
    foundation/log/test_async_log_sink.cpp
    nodes/capture/test_track_recorder/test_logging.cpp
    nodes/room/test_room_node/test_logging.cpp
    applications/host_gui/ui/test_directory_listing/test_logging.cpp

    # --- Circuits ---
    foundation/circuits/models/test_silicon_diode.cpp
    foundation/circuits/models/test_triode12_ax7.cpp
    foundation/circuits/models/test_pentode_el34.cpp
    foundation/math/dk/test_dk_circuit/test_behaviour.cpp
    foundation/circuits/stages/test_triode_stage/test_dk_netlist.cpp
    foundation/math/dk/test_dk_circuit/test_op_amp.cpp
    nodes/tubescreamer/test_tube_screamer_circuits/test_clip_stage.cpp
    nodes/tubescreamer/test_tube_screamer_circuits/test_tone_stage.cpp

    # --- Circuits: stages ---
    foundation/circuits/stages/test_triode_stage/test_behaviour.cpp
    foundation/circuits/stages/test_pentode_stage/test_behaviour.cpp
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
    nodes/tubescreamer/test_tube_screamer/test_behaviour.cpp
    nodes/jcm800/test_jcm_tone_stack.cpp
    nodes/jcm800/test_jcm800/test_behaviour.cpp
    nodes/cabinet/test_cabinet_node.cpp
    nodes/peavey5150/test_peavey5150/test_behaviour.cpp
    nodes/diezel_vh4/test_diezel_vh4/test_behaviour.cpp
    nodes/mesa_mark2c/test_mesa_mark2c/test_behaviour.cpp

    # --- Math ---
    foundation/math/test_biquad.cpp
    foundation/math/test_fft.cpp
    foundation/math/test_newton_raphson_solver.cpp
    foundation/math/dk/test_damped_newton_solver.cpp
    foundation/math/test_oversampler.cpp
    foundation/spice_reference/test_spice_csv.cpp
    foundation/math/test_partitioned_convolution.cpp
    foundation/math/test_streaming_convolution.cpp
    foundation/math/test_sinc_resampler.cpp
    foundation/math/test_resampler_design.cpp
    foundation/math/test_resampler.cpp
    foundation/math/test_smoothed_parameter.cpp
    foundation/math/test_wdf.cpp

    # --- IO ---
    foundation/audio_format/test_asio_sample_converter.cpp
    foundation/io/test_ir_loader.cpp
    foundation/io/test_wav_file.cpp
    foundation/io/test_wav_writer.cpp

    # --- Real-time safety ---
    foundation/allocation_trap/test_allocation_trap.cpp
    foundation/benchmark_harness/test_benchmark_harness.cpp
    foundation/benchmark_harness/test_benchmark_registry.cpp
    foundation/benchmark_harness/test_node_benchmark.cpp
    foundation/benchmark_harness/test_reference_tone.cpp
    foundation/benchmark_harness/test_solver_counters.cpp
    foundation/core/test_audio_node.cpp
    foundation/core/test_rig_graph/test_rt_safety.cpp
    foundation/math/dk/test_dk_circuit/test_rt_safety.cpp
    nodes/tubescreamer/test_tube_screamer/test_rt_safety.cpp
    nodes/jcm800/test_jcm800/test_rt_safety.cpp
    nodes/peavey5150/test_peavey5150/test_rt_safety.cpp
    nodes/mesa_mark2c/test_mesa_mark2c/test_rt_safety.cpp
    nodes/diezel_vh4/test_diezel_vh4/test_rt_safety.cpp
    nodes/dd3/test_dd3_delay/test_rt_safety.cpp
    nodes/capture/test_track_recorder/test_rt_safety.cpp
    nodes/room/test_room_node/test_rt_safety.cpp

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
    applications/host_gui/ui/test_directory_listing/test_behaviour.cpp
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
    nodes/dd3/test_dd3_delay/test_behaviour.cpp
    nodes/room/test_band_response.cpp
    nodes/room/test_materials.cpp
    nodes/room/test_furniture.cpp
    nodes/room/test_air_absorption.cpp
    nodes/room/test_medium_absorption.cpp
    nodes/room/test_band_response_fit.cpp
    nodes/room/test_cabinet_directivity.cpp
    nodes/room/test_microphone_model.cpp
    nodes/room/test_room_scene/test_records.cpp
    nodes/room/test_crossover_pair.cpp
    nodes/room/test_hrtf_set.cpp
    nodes/room/test_ambisonic_encoder.cpp
    nodes/room/test_room_scene/test_behaviour.cpp
    nodes/room/test_room_scene/test_furnishings.cpp
    nodes/room/test_starter_scene.cpp
    applications/host_gui/ui/test_furnishing_list.cpp
    applications/host_gui/ui/test_patch_list.cpp
    nodes/room/test_built_room_loss/test_motus_reference.cpp
    nodes/room/test_built_room_loss/test_bras_reference.cpp
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
    nodes/room/test_room_node/test_behaviour.cpp
    nodes/room/test_environment_render_set.cpp
    nodes/room/test_room_arrangement.cpp
    nodes/room/test_driver_field.cpp
    nodes/room/test_furnishing_field.cpp
    applications/host_gui/test_channel_resampler.cpp
    applications/host_gui/test_audio_import_cache.cpp
    applications/host_gui/test_audio_import_queue.cpp
    applications/host_gui/ui/test_arrangement_layout.cpp
    applications/host_gui/ui/test_tap_tempo.cpp
    applications/host_gui/ui/test_musical_time_text.cpp
    nodes/dd3/test_dd3_delay/test_acceptance.cpp

    # --- Regression ---
    nodes/dd3/test_dd3_delay/test_output.cpp
    nodes/tubescreamer/test_tube_screamer/test_output.cpp
    nodes/jcm800/test_jcm800/test_output.cpp
    nodes/peavey5150/test_peavey5150/test_output.cpp
    nodes/diezel_vh4/test_diezel_vh4/test_output.cpp
    nodes/mesa_mark2c/test_mesa_mark2c/test_output.cpp
    foundation/circuits/stages/test_triode_stage/test_output.cpp
    foundation/circuits/stages/test_pentode_stage/test_output.cpp
)

target_compile_definitions(sushidsp_tests PRIVATE
    SUSHIDSP_SPICE_DIR="${CMAKE_SOURCE_DIR}/docs/reference/spice"
)
target_include_directories(sushidsp_tests PRIVATE ${CMAKE_SOURCE_DIR}/applications/host)
target_include_directories(sushidsp_tests PRIVATE ${CMAKE_SOURCE_DIR}/applications/host_gui)
target_include_directories(sushidsp_tests PRIVATE ${CMAKE_SOURCE_DIR}/applications/plugin)
target_include_directories(sushidsp_tests PRIVATE ${CMAKE_SOURCE_DIR}/third_party)
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
    SushiDSP::sushi_dsp_allocation_trap
    SushiDSP::sushi_dsp_benchmark_harness
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
sushidsp_apply_sanitizers(sushidsp_tests)

# Each regression case names its golden folder; the folder sits beside the unit's tests.
set_source_files_properties(nodes/dd3/test_dd3_delay/test_output.cpp PROPERTIES COMPILE_DEFINITIONS
    "SUSHIDSP_GOLDEN_DIR=\"${CMAKE_CURRENT_SOURCE_DIR}/nodes/dd3/goldens/dd3_delay\"")
set_source_files_properties(nodes/diezel_vh4/test_diezel_vh4/test_output.cpp PROPERTIES COMPILE_DEFINITIONS
    "SUSHIDSP_GOLDEN_DIR=\"${CMAKE_CURRENT_SOURCE_DIR}/nodes/diezel_vh4/goldens/diezel_vh4\"")
set_source_files_properties(nodes/jcm800/test_jcm800/test_output.cpp PROPERTIES COMPILE_DEFINITIONS
    "SUSHIDSP_GOLDEN_DIR=\"${CMAKE_CURRENT_SOURCE_DIR}/nodes/jcm800/goldens/jcm800\"")
set_source_files_properties(nodes/mesa_mark2c/test_mesa_mark2c/test_output.cpp PROPERTIES COMPILE_DEFINITIONS
    "SUSHIDSP_GOLDEN_DIR=\"${CMAKE_CURRENT_SOURCE_DIR}/nodes/mesa_mark2c/goldens/mesa_mark2c\"")
set_source_files_properties(nodes/peavey5150/test_peavey5150/test_output.cpp PROPERTIES COMPILE_DEFINITIONS
    "SUSHIDSP_GOLDEN_DIR=\"${CMAKE_CURRENT_SOURCE_DIR}/nodes/peavey5150/goldens/peavey5150\"")
set_source_files_properties(nodes/tubescreamer/test_tube_screamer/test_output.cpp PROPERTIES COMPILE_DEFINITIONS
    "SUSHIDSP_GOLDEN_DIR=\"${CMAKE_CURRENT_SOURCE_DIR}/nodes/tubescreamer/goldens/tube_screamer\"")

# Each test that reads a fixture names the folder of its owner's fixtures.
set_source_files_properties(nodes/room/test_built_room_loss/test_bras_reference.cpp PROPERTIES COMPILE_DEFINITIONS
    "SUSHIDSP_FIXTURE_DIR=\"${CMAKE_CURRENT_SOURCE_DIR}/nodes/room/fixtures/built_room_loss\"")
set_source_files_properties(nodes/room/test_built_room_loss/test_motus_reference.cpp PROPERTIES COMPILE_DEFINITIONS
    "SUSHIDSP_FIXTURE_DIR=\"${CMAKE_CURRENT_SOURCE_DIR}/nodes/room/fixtures/built_room_loss\"")
set_source_files_properties(applications/host_gui/test_rig_controller.cpp PROPERTIES COMPILE_DEFINITIONS
    "SUSHIDSP_FIXTURE_DIR=\"${CMAKE_CURRENT_SOURCE_DIR}/nodes/room/fixtures/hrtf_set_loader\"")
set_source_files_properties(nodes/room/test_hrtf_set_loader.cpp PROPERTIES COMPILE_DEFINITIONS
    "SUSHIDSP_FIXTURE_DIR=\"${CMAKE_CURRENT_SOURCE_DIR}/nodes/room/fixtures/hrtf_set_loader\"")
set_source_files_properties(nodes/room/test_room_node/test_behaviour.cpp PROPERTIES COMPILE_DEFINITIONS
    "SUSHIDSP_FIXTURE_DIR=\"${CMAKE_CURRENT_SOURCE_DIR}/nodes/room/fixtures/hrtf_set_loader\"")
set_source_files_properties(nodes/room/test_room_node/test_logging.cpp PROPERTIES COMPILE_DEFINITIONS
    "SUSHIDSP_FIXTURE_DIR=\"${CMAKE_CURRENT_SOURCE_DIR}/nodes/room/fixtures/hrtf_set_loader\"")
set_source_files_properties(foundation/spice_reference/test_spice_csv.cpp PROPERTIES COMPILE_DEFINITIONS
    "SUSHIDSP_FIXTURE_DIR=\"${CMAKE_CURRENT_SOURCE_DIR}/foundation/spice_reference/fixtures/spice_csv\"")

# The repository assets folder, for the tests that read a shipped asset.
target_compile_definitions(sushidsp_tests PRIVATE SUSHIDSP_ASSETS_DIR="${CMAKE_SOURCE_DIR}/assets")

# Suite.Case names of the intermittent tests, each with its entry in
# docs/reference/KNOWN_ISSUES.md. A quarantined test keeps its size and gains the quarantined
# label, which leaves it out of every gating run.
set(SUSHIDSP_QUARANTINED_TESTS "")

# A test's size is its GoogleTest suite-name prefix: Small_*, Medium_* and Large_*, each
# registered with its own label and timeout. main.cpp refuses a suite with no prefix, so the
# discovery of every group fails and no test of the binary runs until it is named.
include(GoogleTest)
foreach(_group IN ITEMS "Small_|small|60" "Medium_|medium|300" "Large_|large|900")
    string(REPLACE "|" ";" _parts "${_group}")
    list(GET _parts 0 _prefix)
    list(GET _parts 1 _label)
    list(GET _parts 2 _timeout)

    # A GoogleTest filter after '-' excludes, so the quarantined tests register apart.
    set(_quarantined "")
    foreach(_name IN LISTS SUSHIDSP_QUARANTINED_TESTS)
        if(_name MATCHES "^${_prefix}")
            list(APPEND _quarantined "${_name}")
        endif()
    endforeach()
    list(JOIN _quarantined ":" _quarantined_filter)

    set(_filter "${_prefix}*")
    if(_quarantined_filter)
        string(APPEND _filter "-${_quarantined_filter}")
        gtest_discover_tests(sushidsp_tests
            DISCOVERY_MODE PRE_TEST
            TEST_FILTER "${_quarantined_filter}"
            PROPERTIES LABELS "${_label}\;quarantined" TIMEOUT ${_timeout})
    endif()
    gtest_discover_tests(sushidsp_tests
        DISCOVERY_MODE PRE_TEST
        TEST_FILTER "${_filter}"
        PROPERTIES LABELS "${_label}" TIMEOUT ${_timeout})
endforeach()
# The benchmark program: one executable built from every unit's benchmark_<name>.cpp, run by
# `sd bench` and never by `sd test`. Each file registers its own cases; benchmark_main.cpp
# runs them.
add_executable(sushidsp_bench
    benchmark_main.cpp
    foundation/circuits/stages/benchmark_triode_stage.cpp
    foundation/math/benchmark_oversampler.cpp
    foundation/math/benchmark_partitioned_convolution.cpp
    foundation/math/benchmark_resampler.cpp
    foundation/math/dk/benchmark_dk_circuit.cpp
    nodes/capture/benchmark_track_player.cpp
    nodes/dd3/benchmark_dd3_delay.cpp
    nodes/diezel_vh4/benchmark_diezel_vh4.cpp
    nodes/jcm800/benchmark_jcm800.cpp
    nodes/mesa_mark2c/benchmark_mesa_mark2c.cpp
    nodes/peavey5150/benchmark_peavey5150.cpp
    nodes/room/benchmark_binaural_decoder.cpp
    nodes/room/benchmark_early_reflection_renderer.cpp
    nodes/room/benchmark_modal_bank.cpp
    nodes/room/benchmark_room_node.cpp
    nodes/tubescreamer/benchmark_tube_screamer.cpp
    nodes/tubescreamer/benchmark_tube_screamer_circuits.cpp
)
target_link_libraries(sushidsp_bench PRIVATE
    SushiDSP::sushi_dsp_benchmark_harness
    SushiDSP::sushi_dsp
    SushiDSP::tubescreamer
    SushiDSP::jcm800
    SushiDSP::mesa_mark2c
    SushiDSP::peavey5150
    SushiDSP::diezel_vh4
    SushiDSP::capture
    SushiDSP::dd3
    SushiDSP::room
    SushiDSP::sushi_dsp_circuit_blocks
    SushiDSP::room_scenes
)
sushidsp_apply_optimization_flags(sushidsp_bench)
