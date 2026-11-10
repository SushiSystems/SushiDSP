/**************************************************************************/
/* remove_at_exit.hpp                                                     */
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
 * @file remove_at_exit.hpp
 * @brief Declares the guard that removes a file when a test ends.
 * @author Mustafa Garip
 */

#pragma once

#include <filesystem>
#include <string>
#include <system_error>
#include <utility>

namespace TestSupport
{
    /// @brief Removes a file when it goes out of scope, so a test of a disk adapter leaves no
    /// file behind.
    class RemoveAtExit
    {
        public:
            /// @param path The file to remove; it need not exist.
            explicit RemoveAtExit(std::string path) : path_(std::move(path))
            {
            }

            ~RemoveAtExit()
            {
                std::error_code ignored;
                std::filesystem::remove(path_, ignored);
            }

            RemoveAtExit(const RemoveAtExit&) = delete;
            RemoveAtExit& operator=(const RemoveAtExit&) = delete;

        private:
            /// @brief Holds the file removed when the guard goes out of scope.
            std::string path_;
    };
} // namespace TestSupport
