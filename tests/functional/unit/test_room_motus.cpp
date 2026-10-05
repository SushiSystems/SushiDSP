/****************************************************************/
/* test_room_motus.cpp                                          */
/* SushiDSP - https://github.com/SushiSystems/SushiDSP          */
/* Copyright (c) 2026 Sushi Systems                             */
/* Licensed under PolyForm Noncommercial 1.0.0. See LICENSE.    */
/* Commercial use requires a licence from Sushi Systems.        */
/****************************************************************/

/**
 * @file test_room_motus.cpp
 * @brief Tests for EyringDecay against the decay times measured in the Motus concrete room.
 * @author Mustafa Garip & Sushi Systems
 */

#include <gtest/gtest.h>

#include <array>
#include <cmath>
#include <cstddef>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <limits>
#include <sstream>
#include <string>
#include <vector>

#include "SushiDSP/room/EyringDecay.hpp"
#include "SushiDSP/room/RoomScene.hpp"

namespace
{
    using SushiDSP::room::EyringDecay;
    using SushiDSP::room::kBandCentresHz;
    using SushiDSP::room::kBandCount;
    using SushiDSP::room::MaterialId;
    using SushiDSP::room::RoomScene;
    using SushiDSP::room::Surface;

    /// @brief Counts the rows the committed CSV holds: three measurements times six bands.
    constexpr std::size_t kExpectedRowCount = 18;

    /// @brief Names the first band of the mid-band check, 250 Hz, as an index into the bands.
    constexpr std::size_t kFirstMidBand = 1;

    /// @brief Names the last band of the mid-band check, 2 kHz, as an index into the bands.
    constexpr std::size_t kLastMidBand = 4;

    /// @brief Names the first band of the carpet check, 1 kHz, as an index into the bands.
    constexpr std::size_t kFirstCarpetBand = 3;

    /// @brief Names the last band of the carpet check, 4 kHz, as an index into the bands.
    constexpr std::size_t kLastCarpetBand = 5;

    /// @brief Names the first band of the built-room check, 500 Hz, as an index into the bands.
    constexpr std::size_t kFirstBuiltBand = 2;

    /// @brief Bounds the built-room model's decay against the measurement, as a ratio each way.
    constexpr double kBuiltRatioTolerance = 0.2;

    /// @brief Holds one row of motus_reference.csv, the decay figures the test reads.
    struct MotusRow
    {
        /// @brief Names the measurement, as the CSV spells it: "0001", "0002" or "0003".
        std::string measurement;

        /// @brief Stores the octave band's centre frequency, in hertz.
        int band_hz;

        /// @brief Stores the T20 decay time, in seconds; NaN when the cell is empty.
        double t20_s;

        /// @brief Stores the T30 decay time, in seconds; NaN when the cell is empty.
        double t30_s;
    };

    /// @brief Splits one CSV line on commas into its cells, empty cells included.
    std::vector<std::string> split_cells(const std::string& line)
    {
        std::vector<std::string> cells;
        std::stringstream stream(line);
        std::string cell;
        while (std::getline(stream, cell, ','))
        {
            cells.push_back(cell);
        }
        return cells;
    }

    /// @brief Converts a CSV cell to a number, or to NaN when the cell is empty.
    double cell_to_seconds(const std::string& cell)
    {
        return cell.empty() ? std::numeric_limits<double>::quiet_NaN() : std::stod(cell);
    }

    /// @brief Reads motus_reference.csv, skipping its header line, into one MotusRow per line.
    std::vector<MotusRow> load_motus_reference()
    {
        std::ifstream file(std::string(SUSHIDSP_FIXTURE_DIR) + "/motus_reference.csv");
        std::vector<MotusRow> rows;
        std::string line;
        std::getline(file, line);
        while (std::getline(file, line))
        {
            const std::vector<std::string> cells = split_cells(line);
            if (cells.size() < 5u)
            {
                continue;
            }
            rows.push_back({ cells[0], std::stoi(cells[2]), cell_to_seconds(cells[3]),
                             cell_to_seconds(cells[4]) });
        }
        return rows;
    }

    /// @brief Returns the measured decay of one band: the T30, or the T20 when the T30 is empty.
    double measured_decay_seconds(const std::vector<MotusRow>& rows, const std::string& measurement,
                                  double band_hz)
    {
        for (const MotusRow& row : rows)
        {
            if (row.measurement == measurement && static_cast<double>(row.band_hz) == band_hz)
            {
                return std::isfinite(row.t30_s) ? row.t30_s : row.t20_s;
            }
        }
        return std::numeric_limits<double>::quiet_NaN();
    }

    /// @brief Builds the empty Motus room: 4.9 x 4.4 x 2.9 m of unpainted concrete with the two
    /// windows on +y and the door on -x, poses at the dataset's receiver and source.
    RoomScene motus_empty_scene()
    {
        RoomScene scene;
        scene.dimensions = { 4.9, 4.4, 2.9 };
        scene.surfaces.fill(MaterialId::ConcreteUnpainted);
        scene.scattering.fill(SushiDSP::room::Scattering::Flat);
        scene.patches[0] = { Surface::PlusY, MaterialId::GlassWindow, 5.53 };
        scene.patches[1] = { Surface::MinusX, MaterialId::WoodenDoor, 2.27 };
        scene.patch_count = 2;
        scene.furnishing_count = 0;
        scene.source_position = { 3.462, 2.075, 1.448 };
        scene.listener_position = { 1.825, 2.075, 1.448 };
        scene.microphone_position = { 1.825, 2.075, 1.448 };
        return scene;
    }

    /// @brief Builds the Motus room of measurement 0002: the empty room with 14.0 m2 of thin
    /// carpet on the floor.
    RoomScene motus_carpet_scene()
    {
        RoomScene scene = motus_empty_scene();
        scene.patches[scene.patch_count] = { Surface::Floor, MaterialId::ThinCarpetOnConcrete, 14.0 };
        ++scene.patch_count;
        return scene;
    }

    /// @brief Prints one line of the comparison table: measurement, band, measured, model, ratio.
    void print_table_line(const std::string& measurement, double band_hz, double measured,
                          double model)
    {
        std::cout << std::fixed << std::setprecision(3) << "motus " << measurement << "  "
                  << std::setw(5) << static_cast<int>(band_hz) << " Hz  measured T30 "
                  << std::setw(6) << measured << " s  model ";
        if (std::isfinite(model))
        {
            std::cout << std::setw(6) << model << " s  ratio " << std::setw(5) << model / measured
                      << "\n";
        }
        else
        {
            std::cout << "   n/a (not modelled)\n";
        }
    }
}

TEST(Unit_Dsp, MotusReferenceParsesToEighteenPositiveRows)
{
    const std::vector<MotusRow> rows = load_motus_reference();
    ASSERT_EQ(rows.size(), kExpectedRowCount);
    for (const MotusRow& row : rows)
    {
        const double used = std::isfinite(row.t30_s) ? row.t30_s : row.t20_s;
        EXPECT_TRUE(std::isfinite(used)) << row.measurement << " " << row.band_hz;
        EXPECT_GT(used, 0.0) << row.measurement << " " << row.band_hz;
    }
}

TEST(Unit_Dsp, MotusEmptyRoomDecayIsNotShorterThanMeasuredAndAtMostTwiceAsLong)
{
    const std::vector<MotusRow> rows = load_motus_reference();
    ASSERT_EQ(rows.size(), kExpectedRowCount);
    const RoomScene empty = motus_empty_scene();
    std::string reason;
    ASSERT_TRUE(empty.is_valid(reason)) << reason;
    const RoomScene carpet = motus_carpet_scene();
    ASSERT_TRUE(carpet.is_valid(reason)) << reason;

    const std::array<double, kBandCount> empty_model = EyringDecay::rt60_seconds(empty);
    const std::array<double, kBandCount> carpet_model = EyringDecay::rt60_seconds(carpet);
    for (std::size_t band = 0; band < kBandCount; ++band)
    {
        const double hz = kBandCentresHz[band];
        print_table_line("0001", hz, measured_decay_seconds(rows, "0001", hz), empty_model[band]);
        print_table_line("0002", hz, measured_decay_seconds(rows, "0002", hz), carpet_model[band]);
        print_table_line("0003", hz, measured_decay_seconds(rows, "0003", hz),
                         std::numeric_limits<double>::quiet_NaN());
    }

    for (std::size_t band = kFirstMidBand; band <= kLastMidBand; ++band)
    {
        const double measured = measured_decay_seconds(rows, "0001", kBandCentresHz[band]);
        ASSERT_TRUE(std::isfinite(measured));
        ASSERT_GT(measured, 0.0);
        ASSERT_TRUE(std::isfinite(empty_model[band]));
        // The module's documented state: a first hand estimate put the empty room 40 to 70 percent
        // too long in the mid bands, so two times is a ceiling on that error, not a target.
        const double ratio = empty_model[band] / measured;
        EXPECT_GE(ratio, 1.0) << kBandCentresHz[band] << " Hz";
        EXPECT_LE(ratio, 2.0) << kBandCentresHz[band] << " Hz";
    }
}

TEST(Unit_Dsp, MotusCarpetShortensTheModelAndTheMeasurementAtOneToFourKilohertz)
{
    const std::vector<MotusRow> rows = load_motus_reference();
    ASSERT_EQ(rows.size(), kExpectedRowCount);
    const std::array<double, kBandCount> empty_model = EyringDecay::rt60_seconds(motus_empty_scene());
    const std::array<double, kBandCount> carpet_model = EyringDecay::rt60_seconds(motus_carpet_scene());

    for (std::size_t band = kFirstCarpetBand; band <= kLastCarpetBand; ++band)
    {
        const double hz = kBandCentresHz[band];
        const double measured_empty = measured_decay_seconds(rows, "0001", hz);
        const double measured_carpet = measured_decay_seconds(rows, "0002", hz);
        ASSERT_TRUE(std::isfinite(measured_empty) && measured_empty > 0.0);
        ASSERT_TRUE(std::isfinite(measured_carpet) && measured_carpet > 0.0);
        ASSERT_TRUE(std::isfinite(empty_model[band]) && empty_model[band] > 0.0);
        ASSERT_TRUE(std::isfinite(carpet_model[band]) && carpet_model[band] > 0.0);

        const double model_reduction = 1.0 - carpet_model[band] / empty_model[band];
        const double measured_reduction = 1.0 - measured_carpet / measured_empty;
        std::cout << std::fixed << std::setprecision(1) << "motus carpet " << static_cast<int>(hz)
                  << " Hz  model reduction " << 100.0 * model_reduction
                  << " %  measured reduction " << 100.0 * measured_reduction << " %\n";

        EXPECT_LT(carpet_model[band], empty_model[band]) << hz << " Hz";
        EXPECT_LT(measured_carpet, measured_empty) << hz << " Hz";
    }
}

TEST(Unit_Dsp, MotusEmptyRoomWithTheBuiltRoomLossDecaysWithinAFifthOfMeasured)
{
    const std::vector<MotusRow> rows = load_motus_reference();
    ASSERT_EQ(rows.size(), kExpectedRowCount);
    RoomScene scene = motus_empty_scene();
    scene.built_room_loss = true;
    std::string reason;
    ASSERT_TRUE(scene.is_valid(reason)) << reason;

    const std::array<double, kBandCount> model = EyringDecay::rt60_seconds(scene);
    for (std::size_t band = 0; band < kBandCount; ++band)
    {
        const double hz = kBandCentresHz[band];
        print_table_line("0001 built", hz, measured_decay_seconds(rows, "0001", hz), model[band]);
    }
    for (std::size_t band = kFirstBuiltBand; band <= kLastMidBand; ++band)
    {
        const double measured = measured_decay_seconds(rows, "0001", kBandCentresHz[band]);
        ASSERT_TRUE(std::isfinite(measured) && measured > 0.0);
        const double ratio = model[band] / measured;
        EXPECT_GE(ratio, 1.0 - kBuiltRatioTolerance) << kBandCentresHz[band] << " Hz";
        EXPECT_LE(ratio, 1.0 + kBuiltRatioTolerance) << kBandCentresHz[band] << " Hz";
    }
}
