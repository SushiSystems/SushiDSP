# Sanitizers.cmake
# SushiDSP - https://github.com/SushiSystems/SushiDSP
# Copyright (c) 2026-present Mustafa Garip & Sushi Systems
# Licensed under PolyForm Noncommercial 1.0.0. See LICENSE.
# Commercial use requires a licence from Sushi Systems.

# Opt-in sanitizer instrumentation for the project's own targets.
#
# Applied per target through sushidsp_apply_sanitizers(), beside each call of
# sushidsp_apply_optimization_flags(): every module library, the allocation trap and the test
# program. A global add_compile_options()/add_link_options() would leak the instrumentation into
# the vendored SDL2/glfw/imgui sources pulled in through FetchContent_MakeAvailable() and into
# the embedded ASIO SDK, code not built for consistent instrumentation.
#
# SUSHIDSP_SANITIZE is empty by default. `address` builds with AddressSanitizer and
# UndefinedBehaviorSanitizer, `thread` with ThreadSanitizer; `sd build --sanitize` sets it.
# The two cannot share a build. On a compiler other than Clang or GCC the function is a
# deliberate no-op, so MSVC builds are untouched.

set(SUSHIDSP_SANITIZE "" CACHE STRING "Sanitizer build: empty, address (ASan+UBSan) or thread (TSan)")
set_property(CACHE SUSHIDSP_SANITIZE PROPERTY STRINGS "" address thread)

if(SUSHIDSP_SANITIZE STREQUAL "address")
    set(SUSHIDSP_SANITIZER_FLAGS -fsanitize=address,undefined -fno-omit-frame-pointer
        -fno-sanitize-recover=undefined)
elseif(SUSHIDSP_SANITIZE STREQUAL "thread")
    set(SUSHIDSP_SANITIZER_FLAGS -fsanitize=thread)
    # The ThreadSanitizer C++ runtime defines operator new, which duplicates the allocation trap's
    # replacement in sushidsp_tests. This Clang flag leaves that runtime out of the link.
    if(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
        set(SUSHIDSP_SANITIZER_LINK_FLAGS -fno-sanitize-link-c++-runtime)
    endif()
elseif(NOT SUSHIDSP_SANITIZE STREQUAL "")
    message(FATAL_ERROR "SUSHIDSP_SANITIZE is '${SUSHIDSP_SANITIZE}'; it must be empty, address or thread")
endif()

function(sushidsp_apply_sanitizers target)
    if(SUSHIDSP_SANITIZER_FLAGS AND CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
        target_compile_options(${target} PRIVATE ${SUSHIDSP_SANITIZER_FLAGS})
        target_link_options(${target} PRIVATE
            ${SUSHIDSP_SANITIZER_FLAGS} ${SUSHIDSP_SANITIZER_LINK_FLAGS})
    endif()
endfunction()
