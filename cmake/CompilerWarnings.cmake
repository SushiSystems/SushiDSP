# CompilerWarnings.cmake
# SushiDSP - https://github.com/SushiSystems/SushiDSP
# Copyright (c) 2026 Sushi Systems
# Licensed under PolyForm Noncommercial 1.0.0. See LICENSE.
# Commercial use requires a licence from Sushi Systems.

# Warning policy for the DSP core and everything that links it.
#
# Applied per target through sushidsp_apply_warning_flags(), the same mechanism as
# sushidsp_apply_optimization_flags() in SushiDSPCompilerFlags.cmake, so the strict set below
# never leaks into the vendored SDL2/glfw/imgui sources pulled in through
# FetchContent_MakeAvailable() or into the directly-embedded imgui and ASIO SDK sources --
# code that was not written to this warning level and would drown real findings in noise.

function(sushidsp_apply_warning_flags target)
    if(CMAKE_CXX_COMPILER_ID MATCHES "Clang" OR CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
        target_compile_options(${target} PRIVATE -Wall -Wextra -Wpedantic -Wshadow -Wconversion -Wsign-conversion)
    elseif(MSVC)
        target_compile_options(${target} PRIVATE /W4)
    endif()
endfunction()
