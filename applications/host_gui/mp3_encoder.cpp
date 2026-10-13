/**************************************************************************/
/* mp3_encoder.cpp                                                        */
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
 *  @file mp3_encoder.cpp
 *  @brief Implements the MP3 writer over LAME's encoder.
 *  @author Mustafa Garip & Sushi Systems
 */

#include "mp3_encoder.hpp"

#include <cstddef>
#include <stdexcept>

#include <lame.h>

namespace SushiDSP::host
{
    namespace
    {
        /// @brief Returns the bytes LAME may write for a run of frames: its own worst case.
        std::size_t worst_case_bytes(std::uint32_t frames) noexcept
        {
            return static_cast<std::size_t>(frames) * 5u / 4u + 7200u;
        }
    } // namespace

    Mp3Encoder::Mp3Encoder(const std::string& path, double sample_rate, int kilobits_per_second)
    {
        lame_global_flags* const lame = lame_init();
        if (lame == nullptr)
        {
            throw std::runtime_error("Mp3Encoder: LAME would not start");
        }
        lame_ = lame;
        lame_set_in_samplerate(lame, static_cast<int>(sample_rate + 0.5));
        lame_set_num_channels(lame, 2);
        lame_set_mode(lame, JOINT_STEREO);
        lame_set_brate(lame, kilobits_per_second);
        lame_set_quality(lame, 2);
        if (lame_init_params(lame) < 0)
        {
            lame_close(lame);
            lame_ = nullptr;
            throw std::runtime_error("Mp3Encoder: LAME refused " +
                                     std::to_string(kilobits_per_second) + " kbit/s at " +
                                     std::to_string(static_cast<int>(sample_rate)) + " Hz");
        }
        file_ = std::fopen(path.c_str(), "wb");
        if (file_ == nullptr)
        {
            lame_close(lame);
            lame_ = nullptr;
            throw std::runtime_error("Mp3Encoder: cannot write " + path);
        }
    }

    Mp3Encoder::~Mp3Encoder()
    {
        close();
    }

    void Mp3Encoder::append(const float* left, const float* right, std::uint32_t frames)
    {
        if (lame_ == nullptr || file_ == nullptr || frames == 0)
        {
            return;
        }
        encoded_.resize(worst_case_bytes(frames));
        const int written = lame_encode_buffer_ieee_float(
            static_cast<lame_global_flags*>(lame_), left, right, static_cast<int>(frames),
            encoded_.data(), static_cast<int>(encoded_.size()));
        if (written < 0)
        {
            throw std::runtime_error("Mp3Encoder: LAME failed to encode a block");
        }
        const auto bytes = static_cast<std::size_t>(written);
        if (std::fwrite(encoded_.data(), 1, bytes, file_) != bytes)
        {
            throw std::runtime_error("Mp3Encoder: the file would not take the block");
        }
    }

    void Mp3Encoder::close()
    {
        if (lame_ != nullptr && file_ != nullptr)
        {
            encoded_.resize(worst_case_bytes(0));
            const int written =
                lame_encode_flush(static_cast<lame_global_flags*>(lame_), encoded_.data(),
                                  static_cast<int>(encoded_.size()));
            if (written > 0)
            {
                std::fwrite(encoded_.data(), 1, static_cast<std::size_t>(written), file_);
            }
        }
        if (lame_ != nullptr)
        {
            lame_close(static_cast<lame_global_flags*>(lame_));
            lame_ = nullptr;
        }
        if (file_ != nullptr)
        {
            std::fclose(file_);
            file_ = nullptr;
        }
    }
} // namespace SushiDSP::host
