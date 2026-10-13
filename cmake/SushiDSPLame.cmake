# SushiDSPLame.cmake
# SushiDSP - https://github.com/SushiSystems/SushiDSP
# Copyright (c) 2026-present Mustafa Garip & Sushi Systems
# Licensed under PolyForm Noncommercial 1.0.0. See LICENSE.
# Commercial use requires a licence from Sushi Systems.

# LAME is LGPL, so it is built as a shared library and linked dynamically, never statically.
# LAME ships no CMake build; this file fetches the release tarball and lists its sources. Only
# the Windows configuration header LAME ships is used, so the target exists on Windows alone and
# SUSHIDSP_LAME_AVAILABLE says whether it does.
set(SUSHIDSP_LAME_AVAILABLE OFF)
if(NOT WIN32)
    message(STATUS "LAME is built on Windows only - MP3 export skipped")
    return()
endif()

enable_language(C)
include(FetchContent)
FetchContent_Declare(
    lame
    URL      https://downloads.sourceforge.net/project/lame/lame/3.100/lame-3.100.tar.gz
    URL_HASH SHA256=ddfe36cab873794038ae2c1210557ad34857a4b6bdc515785d1da9e175b1da1e
)
FetchContent_MakeAvailable(lame)

# LAME reads its settings from "config.h"; configMS.h is the one it ships for this platform.
set(SUSHIDSP_LAME_CONFIG_DIR "${CMAKE_BINARY_DIR}/lame_config")
configure_file("${lame_SOURCE_DIR}/configMS.h" "${SUSHIDSP_LAME_CONFIG_DIR}/config.h" COPYONLY)

set(_lame_sources
    libmp3lame/VbrTag.c
    libmp3lame/bitstream.c
    libmp3lame/encoder.c
    libmp3lame/fft.c
    libmp3lame/gain_analysis.c
    libmp3lame/id3tag.c
    libmp3lame/lame.c
    libmp3lame/mpglib_interface.c
    libmp3lame/newmdct.c
    libmp3lame/presets.c
    libmp3lame/psymodel.c
    libmp3lame/quantize.c
    libmp3lame/quantize_pvt.c
    libmp3lame/reservoir.c
    libmp3lame/set_get.c
    libmp3lame/tables.c
    libmp3lame/takehiro.c
    libmp3lame/util.c
    libmp3lame/vbrquantize.c
    libmp3lame/version.c
    libmp3lame/vector/xmm_quantize_sub.c
    mpglib/common.c
    mpglib/dct64_i386.c
    mpglib/decode_i386.c
    mpglib/interface.c
    mpglib/layer1.c
    mpglib/layer2.c
    mpglib/layer3.c
    mpglib/tabinit.c
)
list(TRANSFORM _lame_sources PREPEND "${lame_SOURCE_DIR}/")

add_library(mp3lame SHARED ${_lame_sources} "${lame_SOURCE_DIR}/include/lame.def")
target_include_directories(mp3lame
    PUBLIC  "${lame_SOURCE_DIR}/include"
    PRIVATE "${SUSHIDSP_LAME_CONFIG_DIR}"
            "${lame_SOURCE_DIR}"
            "${lame_SOURCE_DIR}/libmp3lame"
            "${lame_SOURCE_DIR}/mpglib")
# The export file names the decoder, so the decoder is built in.
target_compile_definitions(mp3lame PRIVATE HAVE_CONFIG_H HAVE_MPGLIB)
# LAME is third-party C of its own vintage; its warnings are not ours to fix.
target_compile_options(mp3lame PRIVATE -w)
set_target_properties(mp3lame PROPERTIES OUTPUT_NAME "libmp3lame")
set(SUSHIDSP_LAME_AVAILABLE ON)
