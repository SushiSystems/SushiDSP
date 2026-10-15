/**************************************************************************/
/* benchmark_main.cpp                                                     */
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
 * @file benchmark_main.cpp
 * @brief Runs every registered benchmark case and prints one table row per case.
 * @author Mustafa Garip
 */

#include <cstdio>

#include <SushiDSP/foundation/core/denormal_guard.hpp>
#include <SushiDSP/foundation/benchmark_harness/benchmark_harness.hpp>
#include <SushiDSP/foundation/benchmark_harness/benchmark_registry.hpp>

/**
 * @brief Runs the cases whose group holds one of the arguments, or every case without any.
 * @return 0 when at least one case ran, 1 when the arguments selected none.
 */
int main(int argc, char** argv)
{
    // Same FPU mode the device callbacks run under, so the numbers match the real thread.
    SushiDSP::DenormalGuard denormal_guard;
    std::printf("SushiDSP benchmark  (%g Hz, %u-frame blocks, best of 200)\n\n",
                SushiDSP::bench::K_SAMPLE_RATE, SushiDSP::bench::K_BLOCK_SIZE);
    return SushiDSP::bench::run_selected(argc, argv) > 0 ? 0 : 1;
}
