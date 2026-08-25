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
target_link_libraries(sushidsp_bench PRIVATE
    SushiDSP::sushi_dsp
    SushiDSP::tubescreamer
    SushiDSP::jcm800
)
sushidsp_apply_optimization_flags(sushidsp_bench)

add_test(NAME Benchmark.AudioPath COMMAND sushidsp_bench)
set_tests_properties(Benchmark.AudioPath PROPERTIES LABELS "benchmark")
