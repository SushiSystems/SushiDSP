# SushiDSPLayers.cmake
# SushiDSP - https://github.com/SushiSystems/SushiDSP
# Copyright (c) 2026-present Mustafa Garip & Sushi Systems
# Licensed under PolyForm Noncommercial 1.0.0. See LICENSE.
# Commercial use requires a licence from Sushi Systems.

# The tier order, lowest first, and the dependencies between modules of one tier. The order is
# stated once here; tools/layering/check_layering.py keeps a copy, and
# tools/tests/test_layering_table.py fails when the two disagree.
set(SUSHIDSP_TIER_ORDER foundation nodes catalog session)

# Each entry is "<module>:<dependency>" for a dependency between two modules of one tier. A
# module that depends on a sibling it does not list here fails to configure.
set(SUSHIDSP_SAME_TIER_EDGES
    circuits:math
    io:math
    core:io
    device:core
    device:audio_format
)

# Refuses a same-tier dependency that SUSHIDSP_SAME_TIER_EDGES does not list.
function(sushidsp_require_same_tier_edge module dependency)
    if(NOT "${module}:${dependency}" IN_LIST SUSHIDSP_SAME_TIER_EDGES)
        message(FATAL_ERROR
            "${module} depends on ${dependency}, a module of its own tier, and "
            "cmake/SushiDSPLayers.cmake does not list that edge")
    endif()
endfunction()
