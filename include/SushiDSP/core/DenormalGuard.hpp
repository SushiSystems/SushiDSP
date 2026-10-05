/****************************************************************/
/* DenormalGuard.hpp                                            */
/* SushiDSP - https://github.com/SushiSystems/SushiDSP          */
/* Copyright (c) 2026 Sushi Systems                             */
/* Licensed under PolyForm Noncommercial 1.0.0. See LICENSE.    */
/* Commercial use requires a licence from Sushi Systems.        */
/****************************************************************/

/**
 * @file DenormalGuard.hpp
 * @brief Defines a scope guard that flushes denormals to zero on the calling thread.
 * @author Mustafa Garip
 */

#pragma once

#include <cstdint>

#if defined(__SSE__) || defined(_M_X64) || (defined(_M_IX86_FP) && _M_IX86_FP >= 1)
#define SUSHIDSP_DENORMAL_GUARD_SSE 1
#include <xmmintrin.h>
#elif defined(__aarch64__) || defined(_M_ARM64)
#define SUSHIDSP_DENORMAL_GUARD_AARCH64 1
#endif

namespace SushiDSP
{
    /// @brief Puts the calling thread's FPU into flush-to-zero / denormals-are-zero mode for the
    /// guard's lifetime, restoring the previous mode on destruction.
    ///
    /// One guard is engaged at the top of every device callback; the mode is per thread. The
    /// guard is a control-register read and write, with no allocation, lock or syscall
    /// (include/SushiDSP/README.md).
    class DenormalGuard
    {
    public:
        DenormalGuard()
        {
#if defined(SUSHIDSP_DENORMAL_GUARD_SSE)
            saved_ = _mm_getcsr();
            _mm_setcsr(saved_ | 0x8040u);
#elif defined(SUSHIDSP_DENORMAL_GUARD_AARCH64)
            std::uint64_t fpcr;
            asm volatile("mrs %0, fpcr" : "=r"(fpcr));
            saved_ = fpcr;
            asm volatile("msr fpcr, %0" : : "r"(fpcr | (1ull << 24)));
#endif
        }

        ~DenormalGuard()
        {
#if defined(SUSHIDSP_DENORMAL_GUARD_SSE)
            _mm_setcsr(saved_);
#elif defined(SUSHIDSP_DENORMAL_GUARD_AARCH64)
            asm volatile("msr fpcr, %0" : : "r"(saved_));
#endif
        }

        DenormalGuard(const DenormalGuard&) = delete;
        DenormalGuard& operator=(const DenormalGuard&) = delete;

        /// @brief Whether this build can actually change the FPU mode. False on targets with
        /// neither SSE nor AArch64 FPCR, where the guard is a no-op.
        /// @return True when flush-to-zero is engaged while a guard is alive.
        static constexpr bool is_supported()
        {
#if defined(SUSHIDSP_DENORMAL_GUARD_SSE) || defined(SUSHIDSP_DENORMAL_GUARD_AARCH64)
            return true;
#else
            return false;
#endif
        }

    private:
#if defined(SUSHIDSP_DENORMAL_GUARD_SSE)
        unsigned int saved_ = 0;
#elif defined(SUSHIDSP_DENORMAL_GUARD_AARCH64)
        std::uint64_t saved_ = 0;
#endif
    };
}
