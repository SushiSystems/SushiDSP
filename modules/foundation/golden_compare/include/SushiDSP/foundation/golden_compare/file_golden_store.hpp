/**************************************************************************/
/* file_golden_store.hpp                                                  */
/**************************************************************************/
/*                          This file is part of:                         */
/*                                SushiDSP                                */
/*                https://github.com/SushiSystems/SushiDSP                */
/*                         https://sushisystems.io                        */
/**************************************************************************/
/* Copyright (c) 2026-present Mustafa Garip & Sushi Systems               */
/*                                                                        */
/* Licensed under the Apache License, Version 2.0 (the "License"); you    */
/* may not use this file except in compliance with the License. You may   */
/* obtain a copy of the License at                                        */
/*                                                                        */
/*     https://www.apache.org/licenses/LICENSE-2.0                        */
/*                                                                        */
/* Unless required by applicable law or agreed to in writing, software    */
/* distributed under the License is distributed on an "AS IS" BASIS,      */
/* WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or        */
/* implied. See the License for the specific language governing           */
/* permissions and limitations under the License.                         */
/**************************************************************************/

/**
 * @file file_golden_store.hpp
 * @brief Declares the GoldenStore that keeps its reference samples in raw 32-bit float files.
 * @author Mustafa Garip
 * @see modules/foundation/golden_compare/README.md
 */

#pragma once

#include <cstddef>
#include <cstdio>
#include <string>
#include <vector>

#include <SushiDSP/foundation/golden_compare/golden_store.hpp>

namespace SushiDSP::golden
{
    /// @brief A GoldenStore over files: one raw float32 file per path.
    class FileGoldenStore final : public GoldenStore
    {
        public:
            /// @brief Reads up to @p wanted floats from the file at @p path.
            [[nodiscard]] bool read(const std::string& path, std::vector<float>& samples,
                                    std::size_t wanted) const override
            {
                std::FILE* file = std::fopen(path.c_str(), "rb");
                if (file == nullptr)
                {
                    return false;
                }
                samples.assign(wanted, 0.0f);
                const std::size_t count = std::fread(samples.data(), sizeof(float), wanted, file);
                std::fclose(file);
                samples.resize(count);
                return true;
            }

            /// @brief Writes @p samples to the file at @p path.
            [[nodiscard]] bool write(const std::string& path, const std::vector<float>& samples) override
            {
                std::FILE* file = std::fopen(path.c_str(), "wb");
                if (file == nullptr)
                {
                    return false;
                }
                std::fwrite(samples.data(), sizeof(float), samples.size(), file);
                std::fclose(file);
                return true;
            }
    };
} // namespace SushiDSP::golden
