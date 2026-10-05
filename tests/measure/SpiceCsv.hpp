/****************************************************************/
/* SpiceCsv.hpp                                                 */
/* SushiDSP - https://github.com/SushiSystems/SushiDSP          */
/* Copyright (c) 2026 Sushi Systems                             */
/* Licensed under PolyForm Noncommercial 1.0.0. See LICENSE.    */
/* Commercial use requires a licence from Sushi Systems.        */
/****************************************************************/

#pragma once

#include <cstddef>
#include <fstream>
#include <string>
#include <vector>

namespace SushiDSP
{
namespace measure
{

/**
 * @brief One LTspice AC sweep exported to CSV by `tools/spice/export.py`.
 *
 * Rows come from `<block>_ac.csv`, whose columns are
 * `frequency_hz,setting,magnitude_db,phase_degrees`. All four vectors are parallel and
 * therefore always the same length; `setting` identifies which `.step` run a row belongs
 * to (0, 1, 2 for the three stepped control positions).
 */
struct AcSweep
{
    std::vector<double> frequency_hz;   ///< Sweep frequency of each row, in hertz.
    std::vector<double> magnitude_db;   ///< Response magnitude of each row, in decibels.
    std::vector<double> phase_degrees;  ///< Response phase of each row, in degrees.
    std::vector<int> setting;           ///< Stepped-control index of each row (0, 1 or 2).
};

/**
 * @brief One LTspice transient run exported to CSV by `tools/spice/export.py`.
 *
 * Rows come from `<block>_tran.csv`, whose columns are
 * `time_s,setting,input_v,output_v`, resampled onto a uniform grid. `sample_rate` is
 * recovered from the first two time stamps, so the grid must be uniform for it to mean
 * anything; the exporter guarantees that.
 */
struct Transient
{
    double sample_rate = 0.0;      ///< Grid rate in hertz, or 0 when fewer than two rows.
    std::vector<int> setting;      ///< Stepped-control index of each row (0, 1 or 2).
    std::vector<float> input;      ///< Stimulus voltage of each row.
    std::vector<float> output;     ///< Probed `out` node voltage of each row.
};

namespace detail
{

/**
 * @brief Splits one CSV line into its comma-separated fields.
 *
 * Trailing carriage returns (files authored on Windows and read in text mode elsewhere)
 * are stripped so `std::stod` never sees them.
 *
 * @param line Raw line, without its terminating newline.
 * @return The fields in order; a line with no comma yields a single field.
 */
inline std::vector<std::string> split_csv_line(const std::string& line)
{
    std::vector<std::string> fields;
    std::string field;
    for (const char c : line)
    {
        if (c == ',')
        {
            fields.push_back(field);
            field.clear();
        }
        else if (c != '\r')
        {
            field.push_back(c);
        }
    }
    fields.push_back(field);
    return fields;
}

}  // namespace detail

/**
 * @brief Loads an AC sweep CSV produced by `tools/spice/export.py`.
 *
 * The single header line is skipped, as are blank lines and any line with fewer than four
 * fields. A missing or unreadable file is not an error: it yields an empty sweep, which
 * lets a test that needs the corpus skip cleanly instead of aborting.
 *
 * @param path Path to a `<block>_ac.csv` file.
 * @return The parsed sweep, empty when the file is absent or contains no data rows.
 */
inline AcSweep load_ac_csv(const std::string& path)
{
    AcSweep sweep;

    std::ifstream file(path);
    if (!file.is_open())
    {
        return sweep;
    }

    std::string line;
    bool header_skipped = false;
    while (std::getline(file, line))
    {
        if (!header_skipped)
        {
            header_skipped = true;
            continue;
        }

        const std::vector<std::string> fields = detail::split_csv_line(line);
        if (fields.size() < 4u || fields[0].empty())
        {
            continue;
        }

        sweep.frequency_hz.push_back(std::stod(fields[0]));
        sweep.setting.push_back(static_cast<int>(std::stod(fields[1])));
        sweep.magnitude_db.push_back(std::stod(fields[2]));
        sweep.phase_degrees.push_back(std::stod(fields[3]));
    }

    return sweep;
}

/**
 * @brief Loads a transient CSV produced by `tools/spice/export.py`.
 *
 * Behaves like @ref load_ac_csv for headers, blank lines and missing files. The sample
 * rate is derived as `1 / (t[1] - t[0])` and left at 0 when fewer than two rows are
 * present or the first two time stamps coincide.
 *
 * @param path Path to a `<block>_tran.csv` file.
 * @return The parsed transient, empty with a 0 sample rate when the file is absent.
 */
inline Transient load_transient_csv(const std::string& path)
{
    Transient transient;

    std::ifstream file(path);
    if (!file.is_open())
    {
        return transient;
    }

    std::vector<double> time_s;
    std::string line;
    bool header_skipped = false;
    while (std::getline(file, line))
    {
        if (!header_skipped)
        {
            header_skipped = true;
            continue;
        }

        const std::vector<std::string> fields = detail::split_csv_line(line);
        if (fields.size() < 4u || fields[0].empty())
        {
            continue;
        }

        time_s.push_back(std::stod(fields[0]));
        transient.setting.push_back(static_cast<int>(std::stod(fields[1])));
        transient.input.push_back(static_cast<float>(std::stod(fields[2])));
        transient.output.push_back(static_cast<float>(std::stod(fields[3])));
    }

    if (time_s.size() >= 2u)
    {
        const double step = time_s[1] - time_s[0];
        if (step > 0.0)
        {
            transient.sample_rate = 1.0 / step;
        }
    }

    return transient;
}

}  // namespace measure
}  // namespace SushiDSP
