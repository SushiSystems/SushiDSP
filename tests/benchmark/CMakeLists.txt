# Timing harness for the audio-thread hot paths. Standard library only, no benchmark
# framework: it reports the best-of-N cost per output sample and how much of one core that is
# at 48 kHz, which is the only number the audio thread cares about. Run it via
# `sd test --suite benchmark` (or `sushidsp_bench [filter...]` directly for repeated runs
# during a perf pass -- ctest's per-invocation overhead is unwelcome there).
#
# Not a gtest_discover_tests case: this harness has no pass/fail assertion, it prints a
# timing table, so it is registered as a single ctest case instead of one GTest per row.
add_executable(sushidsp_bench
    main.cpp
)
# tests/circuits/ is header-only and test-side only, never linked into a product: the benchmark
# builds the same jcm_v1a block the unit tests do, from the one description of it.
target_include_directories(sushidsp_bench PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/..)
target_link_libraries(sushidsp_bench PRIVATE
    SushiDSP::sushi_dsp
    SushiDSP::tubescreamer
    SushiDSP::jcm800
    SushiDSP::capture
    SushiDSP::dd3
    SushiDSP::room
)
sushidsp_apply_optimization_flags(sushidsp_bench)

add_test(NAME Benchmark.AudioPath COMMAND sushidsp_bench)
set_tests_properties(Benchmark.AudioPath PROPERTIES LABELS "benchmark")
