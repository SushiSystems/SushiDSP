/**************************************************************************/
/* wet_fifo.hpp                                                           */
/**************************************************************************/
/*                          This file is part of:                         */
/*                                SushiDSP                                */
/*                https://github.com/SushiSystems/SushiDSP                */
/*                         https://sushisystems.io                        */
/**************************************************************************/
/* Copyright (c) 2026-present Mustafa Garip & Sushi Systems               */
/*                                                                        */
/* Licensed under the PolyForm Noncommercial License 1.0.0 (the           */
/* "License"); you may not use this file except in compliance with the    */
/* License. You may obtain a copy of the License at                       */
/*                                                                        */
/*     https://polyformproject.org/licenses/noncommercial/1.0.0           */
/*                                                                        */
/* Noncommercial use is free. Commercial use requires a separate licence  */
/* from Sushi Systems; see COMMERCIAL.md. The software is provided        */
/* "as is", without warranty of any kind.                                 */
/**************************************************************************/

/**
 * @file wet_fifo.hpp
 * @brief Fixed-capacity elastic FIFO that primes before it delivers.
 * @author Mustafa Garip & Sushi Systems
 */

#pragma once

#include <array>
#include <cstdint>

namespace SushiDSP::dd3
{
    /// @brief Hands a bursty producer's samples to a one-per-sample consumer.
    ///
    /// A base-rate push through the DD-3's two conversions returns 0 to 3 samples, so the
    /// consumer needs a cushion. @ref pop returns silence until @ref K_PRIME_FRAMES samples have
    /// arrived and the FIFO then sits at that depth; priming on real samples rather than on
    /// zeros written at reset is what survives the resamplers' start-up lag, and
    /// docs/archive/superpowers/specs/2026-09-05-dd3-spx90-design.md, Model corrections, has the
    /// measurement. Every call is allocation-, lock- and throw-free.
    class WetFifo
    {
        public:
            /// @brief Empties the FIFO and re-arms the prime.
            void reset()
            {
                samples_.fill(0.0f);
                write_ = 0u;
                read_ = 0u;
                priming_ = true;
            }

            /// @brief Appends one sample, dropping the oldest if the FIFO is already full.
            /// @param sample The sample to append.
            void push(float sample)
            {
                if (depth() >= K_CAPACITY)
                {
                    ++read_;
                }
                samples_[write_ & K_MASK] = sample;
                ++write_;
            }

            /// @brief Takes the oldest sample once the prime has been reached.
            /// @return The sample, or 0 while priming or starved.
            float pop()
            {
                if (priming_ && depth() >= K_PRIME_FRAMES)
                {
                    priming_ = false;
                }
                if (priming_ || depth() == 0u)
                {
                    return 0.0f;
                }
                const float sample = samples_[read_ & K_MASK];
                ++read_;
                return sample;
            }

            /// @brief How many samples are waiting.
            /// @return The depth, never above K_CAPACITY.
            std::uint32_t depth() const
            {
                return write_ - read_;
            }

            /// @brief Whether pop() is still withholding samples.
            /// @return True until the depth first reaches K_PRIME_FRAMES.
            bool priming() const
            {
                return priming_;
            }

            /// @brief Capacity in samples; a power of two, so free-running indices mask onto it.
            static constexpr std::uint32_t K_CAPACITY = 64u;

            /// @brief Samples held back before pop() starts delivering.
            static constexpr std::uint32_t K_PRIME_FRAMES = 8u;

        private:
            /// @brief Wraps a free-running index onto K_CAPACITY.
            static constexpr std::uint32_t K_MASK = K_CAPACITY - 1u;

            std::array<float, K_CAPACITY> samples_{};
            std::uint32_t write_ = 0u; // free-running; K_CAPACITY divides 2^32
            std::uint32_t read_ = 0u;
            bool priming_ = true;
    };
} // namespace SushiDSP::dd3
