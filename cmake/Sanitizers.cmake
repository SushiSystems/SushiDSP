# Sanitizers.cmake
# SushiDSP - https://github.com/SushiSystems/SushiDSP
# Copyright (c) 2026 Sushi Systems
# Licensed under PolyForm Noncommercial 1.0.0. See LICENSE.
# Commercial use requires a licence from Sushi Systems.

# Opt-in sanitizer instrumentation for the DSP core and its consumers.
#
# Applied per target through sushidsp_apply_sanitizers(), the same mechanism as
# sushidsp_apply_optimization_flags() and sushidsp_apply_warning_flags() in
# SushiDSPCompilerFlags.cmake / CompilerWarnings.cmake. Unlike those two, it is called at
# only three sites in the root CMakeLists.txt -- sushi_dsp, sushi_dsp_sdl2 and
# sushi_dsp_asio -- not at the test, benchmark, host-app or product-module call sites.
# A global add_compile_options()/add_link_options() would leak ASan/UBSan into
# the vendored SDL2/glfw/imgui sources pulled in through FetchContent_MakeAvailable()
# and into the directly-embedded imgui and ASIO SDK sources -- code not built for
# consistent instrumentation, where ASan/UBSan can produce link errors or spurious
# reports rather than a working diagnostic.
#
# Off by default; Clang only. When off, or on any other compiler, the function is a
# deliberate no-op rather than an error, so the option is safe to leave untouched on
# MSVC/GNU builds.

option(SUSHIDSP_ENABLE_SANITIZERS "Build with ASan+UBSan" OFF)

function(sushidsp_apply_sanitizers target)
    if(SUSHIDSP_ENABLE_SANITIZERS AND CMAKE_CXX_COMPILER_ID MATCHES "Clang")
        target_compile_options(${target} PRIVATE -fsanitize=address,undefined)
        target_link_options(${target} PRIVATE -fsanitize=address,undefined)
    endif()
endfunction()
