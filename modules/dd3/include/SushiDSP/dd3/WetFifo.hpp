/**************************************************************************/
/* WetFifo.hpp                                                            */
/**************************************************************************/
/*                         This file is part of:                          */
/*                                SushiDSP                                */
/*                https://github.com/SushiSystems/SushiDSP                */
/*                        https://sushisystems.io                         */
/**************************************************************************/
/* Copyright (c) 2026-present Mustafa Garip & Sushi Systems               */
/*                                                                        */
/* Licensed under the Apache License, Version 2.0 (the "License");        */
/* you may not use this file except in compliance with the License.       */
/* You may obtain a copy of the License at                                */
/*                                                                        */
/*     http://www.apache.org/licenses/LICENSE-2.0                         */
/*                                                                        */
/* Unless required by applicable law or agreed to in writing, software    */
/* distributed under the License is distributed on an "AS IS" BASIS,      */
/* WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or        */
/* implied. See the License for the specific language governing           */
/* permissions and limitations under the License.                         */
/**************************************************************************/

/**
 * @file WetFifo.hpp
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
    /// consumer needs a cushion. @ref pop returns silence until @ref kPrimeFrames samples have
    /// arrived and the FIFO then sits at that depth; priming on real samples rather than on
    /// zeros written at reset is what survives the resamplers' start-up lag, and
    /// docs/superpowers/specs/2026-09-05-dd3-spx90-design.md, Model corrections, has the
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
            if (depth() >= kCapacity)
            {
                ++read_;
            }
            samples_[write_ & kMask] = sample;
            ++write_;
        }

        /// @brief Takes the oldest sample once the prime has been reached.
        /// @return The sample, or 0 while priming or starved.
        float pop()
        {
            if (priming_ && depth() >= kPrimeFrames)
            {
                priming_ = false;
            }
            if (priming_ || depth() == 0u)
            {
                return 0.0f;
            }
            const float sample = samples_[read_ & kMask];
            ++read_;
            return sample;
        }

        /// @brief How many samples are waiting.
        /// @return The depth, never above kCapacity.
        std::uint32_t depth() const { return write_ - read_; }

        /// @brief Whether pop() is still withholding samples.
        /// @return True until the depth first reaches kPrimeFrames.
        bool priming() const { return priming_; }

        /// @brief Capacity in samples; a power of two, so free-running indices mask onto it.
        static constexpr std::uint32_t kCapacity = 64u;

        /// @brief Samples held back before pop() starts delivering.
        static constexpr std::uint32_t kPrimeFrames = 8u;

    private:
        /// @brief Wraps a free-running index onto kCapacity.
        static constexpr std::uint32_t kMask = kCapacity - 1u;

        std::array<float, kCapacity> samples_{};
        std::uint32_t write_ = 0u;          // free-running; kCapacity divides 2^32
        std::uint32_t read_ = 0u;
        bool priming_ = true;
    };
}
