# Optimization policy for the DSP core and everything that links it.
#
# Applied per target through sushidsp_apply_optimization_flags() so that every target
# carrying core code — the library, the product modules, the bench, the tests — compiles with
# the same ISA and inlining policy; mixing AVX2 and non-AVX2 objects that inline the same
# header code is a silent way to get two different answers from one function.
#
# Debug is left entirely alone. Release and RelWithDebInfo get:
#   - the toolchain's full optimizer (CMake's own /O2 or -O3 defaults are kept, not overridden);
#   - AVX2 + FMA code generation, behind SUSHIDSP_ENABLE_AVX2;
#   - link-time optimization when the toolchain supports it, behind SUSHIDSP_ENABLE_LTO,
#     so the per-sample stage calls across translation units can inline.
#
# Deliberately absent: -ffast-math / /fp:fast. Reassociation would move the regression
# snapshots and make the Newton residuals depend on the compiler. FMA contraction is likewise
# not forced (-ffp-contract stays at the compiler default); AVX2 merely makes it available.

include(CheckIPOSupported)

option(SUSHIDSP_ENABLE_AVX2 "Compile optimized configurations for AVX2 + FMA" ON)
option(SUSHIDSP_ENABLE_LTO "Enable link-time optimization in optimized configurations" ON)

check_ipo_supported(RESULT _sushidsp_ipo_supported OUTPUT _sushidsp_ipo_message LANGUAGES CXX)
if(SUSHIDSP_ENABLE_LTO AND NOT _sushidsp_ipo_supported)
    message(STATUS "SushiDSP: LTO requested but unsupported by this toolchain: ${_sushidsp_ipo_message}")
endif()

set(_sushidsp_optimized "$<OR:$<CONFIG:Release>,$<CONFIG:RelWithDebInfo>>")

function(sushidsp_apply_optimization_flags target)
    if(SUSHIDSP_ENABLE_AVX2)
        if(MSVC)
            target_compile_options(${target} PRIVATE "$<${_sushidsp_optimized}:/arch:AVX2>")
        else()
            target_compile_options(${target} PRIVATE "$<${_sushidsp_optimized}:-mavx2;-mfma>")
        endif()
    endif()
    if(SUSHIDSP_ENABLE_LTO AND _sushidsp_ipo_supported)
        set_target_properties(${target} PROPERTIES
            INTERPROCEDURAL_OPTIMIZATION_RELEASE ON
            INTERPROCEDURAL_OPTIMIZATION_RELWITHDEBINFO ON)
    endif()
endfunction()
