/**************************************************************************/
/* profile.hpp                                                            */
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
 * @file profile.hpp
 * @brief Declares the SD_ZONE macros, SushiDSP's front over SushiProfiler.
 * @author Mustafa Garip
 * @see modules/foundation/profile/README.md
 */

#pragma once

// The build generates profile_config.hpp, which defines SD_ENABLE_PROFILER and SD_PROFILE_LEVEL.
#include <SushiDSP/foundation/profile/profile_config.hpp>

#if SD_ENABLE_PROFILER

#include <SushiProfiler/zones/macros.hpp>

/// @brief Records the rest of the scope as a stage zone.
#define SD_ZONE(name) SP_ZONE_AT(SD_PROFILE_LEVEL, 1, name, nullptr)

/// @brief Records the rest of the scope as a stage zone charged to @p group.
#define SD_ZONE_GROUP(name, group) SP_ZONE_AT(SD_PROFILE_LEVEL, 1, name, group)

/// @brief Records the rest of the scope as a stage zone named by the interned @p name_id.
#define SD_ZONE_NAMED(name, group, name_id)                                                        \
    SP_ZONE_NAMED_AT(SD_PROFILE_LEVEL, 1, name, group, name_id)

/// @brief Records the rest of the scope as a fine zone, level 2.
#define SD_ZONE_FINE(name) SP_ZONE_AT(SD_PROFILE_LEVEL, 2, name, nullptr)

/// @brief Records @p value at a counter folded per period by CounterOp @p op.
#define SD_COUNTER(name, unit, op, value) SP_COUNTER_AT(SD_PROFILE_LEVEL, 1, name, unit, op, value)

/// @brief Records an occurrence now.
#define SD_INSTANT(name) SP_INSTANT_AT(SD_PROFILE_LEVEL, 1, name)

/// @brief Puts the rest of the scope in lane @p lane_id, an interned name.
#define SD_LANE(lane_id) SP_LANE_AT(SD_PROFILE_LEVEL, lane_id)

#else

/// @brief Compiles to nothing.
#define SD_ZONE(name) static_cast<void>(0)

/// @brief Compiles to nothing.
#define SD_ZONE_GROUP(name, group) static_cast<void>(0)

/// @brief Compiles to nothing, without evaluating @p name_id.
#define SD_ZONE_NAMED(name, group, name_id) static_cast<void>(sizeof(name_id))

/// @brief Compiles to nothing.
#define SD_ZONE_FINE(name) static_cast<void>(0)

/// @brief Compiles to nothing, without evaluating @p value.
#define SD_COUNTER(name, unit, op, value) static_cast<void>(sizeof(value))

/// @brief Compiles to nothing.
#define SD_INSTANT(name) static_cast<void>(0)

/// @brief Compiles to nothing, without evaluating @p lane_id.
#define SD_LANE(lane_id) static_cast<void>(sizeof(lane_id))

#endif
