/**************************************************************************/
/* golden_store.hpp                                                       */
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
 * @file golden_store.hpp
 * @brief Declares the port through which a golden comparison reads and writes its reference samples.
 * @author Mustafa Garip
 * @see modules/foundation/golden_compare/README.md
 */

#pragma once

#include <cstddef>
#include <string>
#include <vector>

namespace SushiDSP::golden
{
    /// @brief Where the reference samples of a golden comparison live.
    class GoldenStore
    {
        public:
            virtual ~GoldenStore() = default;

            /// @brief Reads the samples stored at @p path.
            /// @param path The reference's path.
            /// @param samples Receives up to @p wanted samples, in order.
            /// @param wanted The number of samples to read.
            /// @return False when nothing is stored at @p path; true otherwise, with samples.size() the count read.
            [[nodiscard]] virtual bool read(const std::string& path, std::vector<float>& samples,
                                            std::size_t wanted) const = 0;

            /// @brief Stores @p samples at @p path, replacing what was there.
            /// @return False when the path cannot be written.
            [[nodiscard]] virtual bool write(const std::string& path, const std::vector<float>& samples) = 0;
    };
} // namespace SushiDSP::golden
