/****************************************************************/
/* Fft.hpp                                                      */
/* SushiDSP - https://github.com/SushiSystems/SushiDSP          */
/* Copyright (c) 2026 Sushi Systems                             */
/* Licensed under PolyForm Noncommercial 1.0.0. See LICENSE.    */
/* Commercial use requires a licence from Sushi Systems.        */
/****************************************************************/

#pragma once

#include <cstdint>
#include <vector>

namespace SushiDSP
{
    /// @brief Power-of-two real FFT. Precomputes twiddle factors and bit-reversal in the constructor.
    ///
    /// Transforms are out-of-place: the three buffers passed to forward() or inverse() must be
    /// distinct and must not alias each other.
    class Fft
    {
    public:
        /// @brief Constructs an FFT plan for the given power-of-two size.
        /// @param size Transform size; must be a power of two.
        explicit Fft(std::uint32_t size);

        /// @brief Computes the forward DFT of a real time-domain signal.
        /// @param time_domain Input, size() samples.
        /// @param real_out Output real part, size() bins.
        /// @param imag_out Output imaginary part, size() bins.
        /// @note The three buffers must not alias each other.
        void forward(const float* time_domain, float* real_out, float* imag_out) const;

        /// @brief Computes the inverse DFT back to a real time-domain signal.
        /// @param real_in Input real part, size() bins.
        /// @param imag_in Input imaginary part, size() bins.
        /// @param time_domain_out Output, size() samples.
        /// @note The three buffers must not alias each other.
        /// @note Uses internal scratch buffers, so concurrent calls on one plan are not supported.
        void inverse(const float* real_in, const float* imag_in, float* time_domain_out) const;

        /// @brief The transform size this plan was constructed for.
        /// @return The FFT size.
        std::uint32_t size() const;

    private:
        std::uint32_t              size_;
        std::vector<std::uint32_t> bit_reversal_;

        // Every butterfly stage's twiddles laid end to end, stage span at offset span - 1.
        std::vector<float>         stage_twiddle_real_;
        std::vector<float>         stage_twiddle_imag_;

        // Sized once in the constructor so inverse() can stay const and allocation-free.
        mutable std::vector<float> real_scratch_;
        mutable std::vector<float> imag_scratch_;
    };
}
