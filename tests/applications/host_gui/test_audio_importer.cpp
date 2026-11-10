/**************************************************************************/
/* test_audio_importer.cpp                                                */
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
 * @file test_audio_importer.cpp
 * @brief Covers the in-memory adapter of the audio importer port.
 * @author Mustafa Garip & Sushi Systems
 */

#include <stdexcept>

#include <gtest/gtest.h>

#include "audio_importer.hpp"

TEST(Small_AudioImporter, InMemoryAudioImporterNamesTheWavAfterTheSource)
{
    const SushiDSP::host::InMemoryAudioImporter importer;

    EXPECT_EQ(importer.import("a.mp3", 48000.0), "a.mp3.wav");
}

TEST(Small_AudioImporter, InMemoryAudioImporterThrowsForAFailingPath)
{
    SushiDSP::host::InMemoryAudioImporter importer;
    importer.fail("bad.mp3", "cannot decode");

    EXPECT_THROW(static_cast<void>(importer.import("bad.mp3", 48000.0)), std::runtime_error);
}
