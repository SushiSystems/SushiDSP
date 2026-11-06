/**************************************************************************/
/* regression_test.hpp                                                    */
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
 * @file regression_test.hpp
 * @brief Declares the macro that writes a regression test.
 * @author Mustafa Garip
 */

#pragma once

#include <gtest/gtest.h>

/**
 * @brief Declares a regression test: a case that keeps a fixed defect fixed.
 *
 * Expands to TEST, and is registered with its file's size like any TEST.
 */
#define SUSHIDSP_REGRESSION_TEST(suite, name) TEST(suite, name)
