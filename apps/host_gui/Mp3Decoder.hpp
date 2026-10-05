/****************************************************************/
/* Mp3Decoder.hpp                                               */
/* SushiDSP - https://github.com/SushiSystems/SushiDSP          */
/* Copyright (c) 2026 Sushi Systems                             */
/* Licensed under PolyForm Noncommercial 1.0.0. See LICENSE.    */
/* Commercial use requires a licence from Sushi Systems.        */
/****************************************************************/

/**
 * @file Mp3Decoder.hpp
 * @brief Declares the MPEG audio decode the host runs before a dropped song becomes a clip.
 * @author Mustafa Garip & Sushi Systems
 */

#pragma once

#include <string>
#include <vector>

namespace SushiDSP::host
{
    /// @brief Holds one decoded file: the channels kept apart, and the rate they carry.
    struct DecodedAudio
    {
        /// @brief Holds one vector of samples per channel, in the file's own channel order.
        std::vector<std::vector<float>> channels;

        /// @brief Holds the rate the file was encoded at, in Hz.
        double sample_rate = 0.0;
    };

    /// @brief Decodes a whole MPEG audio file into float samples in [-1, 1]. The only unit in
    /// the tree that knows what an mp3 is. Control thread only: it allocates, reads the
    /// filesystem and throws.
    /// @param path Filesystem path to the mp3.
    /// @return The decoded audio, at the file's own rate and channel count.
    /// @throws std::runtime_error when the file cannot be opened, carries no frames, or fails
    /// to decode.
    DecodedAudio decode_mp3(const std::string& path);
}
