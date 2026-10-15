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
