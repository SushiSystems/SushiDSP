/**************************************************************************/
/* DkCircuit.hpp                                                          */
/**************************************************************************/
/*                         This file is part of:                          */
/*                                SushiDSP                                */
/*                https://github.com/SushiSystems/SushiDSP                */
/*                        https://sushisystems.io                         */
/**************************************************************************/
/* Copyright (c) 2026-present Mustafa Garip & Sushi Systems               */
/*                                                                        */
/* Licensed under the Apache License, Version 2.0 (the "License");        */
/* you may not use this file except in compliance with the License.       */
/* You may obtain a copy of the License at                                */
/*                                                                        */
/*     http://www.apache.org/licenses/LICENSE-2.0                         */
/*                                                                        */
/* Unless required by applicable law or agreed to in writing, software    */
/* distributed under the License is distributed on an "AS IS" BASIS,      */
/* WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or        */
/* implied. See the License for the specific language governing           */
/* permissions and limitations under the License.                         */
/**************************************************************************/

#pragma once

#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>

#include "SushiDSP/math/dk/DampedNewtonSolver.hpp"
#include "SushiDSP/math/dk/DenseLu.hpp"
#include "SushiDSP/math/dk/DkNetlist.hpp"

namespace SushiDSP::dk
{
    /// @brief Floor applied to each potentiometer section's resistance.
    ///
    /// A knob at an end stop would otherwise leave a section at exactly zero ohms, shorting two
    /// nodes and making the augmented nodal matrix singular. One ohm is one to three orders of
    /// magnitude below any real pot's wiper contact resistance, so the floor is inaudible while
    /// it keeps the derivation well posed.
    inline constexpr double kMinimumPotOhms = 1.0;

    /// @brief One discretised nonlinear circuit block, solved by damped Newton per sample.
    ///
    /// Implements the nodal DK method (Holters and Zoelzer) with the trapezoidal rule, as
    /// transcribed in the D6b plan. prepare() runs the derivation once; after that each sample
    /// costs one Newton solve on the N-dimensional nonlinear port plus the five small matrix
    /// products of the state-space update. Potentiometers are re-derived rather than carried as
    /// Newton branches (D6b plan, ruling 3), which is what makes a knob cost nothing per sample.
    ///
    /// The caller supplies the glue between the branch vector and its device models as a
    /// NonlinearSystem exposing
    /// `void currents(const std::array<double, N>& v, std::array<double, N>& i) const` and
    /// `void jacobian(const std::array<double, N>& v, std::array<std::array<double, N>, N>& j) const`.
    /// The Jacobian buffer is zeroed before every call, so a glue struct for independent devices
    /// simply leaves the off-diagonal blocks alone. Nothing here includes a circuit model:
    /// DkCircuit stays in SushiDSP/math with no dependency on SushiDSP/circuits.
    ///
    /// process_sample(), update_matrices_if_dirty() and everything they reach are
    /// allocation-free, lock-free, syscall-free and throw-free. Every state is clamped each
    /// sample and a non-finite sample restores the previous state, so the block cannot emit a
    /// non-finite output regardless of what is fed to it (D6b plan, ruling 10).
    /// @tparam S Reactive element (state) count.
    /// @tparam N Nonlinear branch count; may be zero, in which case no solver runs.
    /// @tparam I Input voltage source count.
    /// @tparam O Output probe count.
    template <std::size_t S, std::size_t N, std::size_t I, std::size_t O>
    class DkCircuit
    {
    public:
        /// @brief Dimension of the augmented nodal system at the maximum supported node count.
        ///
        /// One row and one column per node, per input source and per nullor. At the worst shape
        /// that is 16 + 2 + 4 = 22, so the system and its inverse are under 8 KB together.
        static constexpr std::size_t kMaximumSystemOrder = kMaximumNodes + I + kMaximumNullors;

        /// @brief Validates a netlist, discretises it at the run rate and derives the matrices.
        ///
        /// The run rate is the oversampled rate the block will actually be stepped at, not the
        /// host rate. All potentiometers start at position 0.5. Also clears state and statistics.
        /// @param netlist The circuit description; copied, so the builder may go out of scope.
        /// @param sample_rate Run rate in hertz, strictly positive.
        /// @return False when the netlist is malformed or the nodal system is singular; the
        /// object is then not usable.
        bool prepare(const DkNetlist<S, N, I, O>& netlist, double sample_rate)
        {
            if (!(sample_rate > 0.0))
            {
                return false;
            }
            if (netlist.node_count == 0 || netlist.node_count > kMaximumNodes)
            {
                return false;
            }
            if (netlist.resistor_count_ > kMaximumResistors || netlist.reactive_count_ != S
                || netlist.source_count_ != I || netlist.nonlinear_count_ != N
                || netlist.output_count_ != O
                || netlist.potentiometer_count_ > kMaximumPotentiometers
                || netlist.nullor_count_ > kMaximumNullors)
            {
                return false;
            }

            for (std::size_t index = 0; index < netlist.resistor_count_; ++index)
            {
                if (!in_range(netlist.resistors_[index], netlist.node_count)
                    || !(netlist.resistors_[index].value > 0.0))
                {
                    return false;
                }
            }
            for (std::size_t index = 0; index < S; ++index)
            {
                if (!in_range(netlist.reactive_[index], netlist.node_count)
                    || !(netlist.reactive_[index].value > 0.0))
                {
                    return false;
                }
            }
            for (std::size_t index = 0; index < I; ++index)
            {
                if (!in_range(netlist.sources_[index], netlist.node_count))
                {
                    return false;
                }
            }
            for (std::size_t index = 0; index < N; ++index)
            {
                if (!in_range(netlist.nonlinear_[index], netlist.node_count))
                {
                    return false;
                }
            }
            for (std::size_t index = 0; index < O; ++index)
            {
                if (!in_range(netlist.outputs_[index], netlist.node_count))
                {
                    return false;
                }
            }
            for (std::size_t index = 0; index < netlist.nullor_count_; ++index)
            {
                if (!in_range(netlist.nullors_[index].nullator, netlist.node_count)
                    || !in_range(netlist.nullors_[index].norator, netlist.node_count))
                {
                    return false;
                }
            }
            for (std::size_t index = 0; index < netlist.potentiometer_count_; ++index)
            {
                const PotentiometerEntry& entry = netlist.potentiometers_[index];
                if (entry.first_section_index >= netlist.resistor_count_
                    || entry.second_section_index >= netlist.resistor_count_
                    || !(entry.total_ohms > 0.0))
                {
                    return false;
                }
            }

            netlist_ = netlist;
            node_count_ = netlist.node_count;
            sample_rate_ = sample_rate;

            for (std::size_t index = 0; index < S; ++index)
            {
                // Gx = 2C/T for a capacitor and T/(2L) for an inductor, with T = 1/sample_rate
                // (D6b plan, Reactive companion).
                gx_[index] = netlist_.reactive_sign_[index] > 0.0
                                 ? 2.0 * netlist_.reactive_[index].value * sample_rate
                                 : 1.0 / (2.0 * netlist_.reactive_[index].value * sample_rate);
            }

            for (std::size_t index = 0; index < kMaximumPotentiometers; ++index)
            {
                pot_position_[index] = 0.5;
            }
            matrices_dirty_ = true;

            const bool derived = derive();
            reset();
            return derived;
        }

        /// @brief Advances the circuit by one sample.
        ///
        /// Allocation-free, lock-free, syscall-free and throw-free, provided the supplied models
        /// are. Warm-starts the Newton iteration from the previous sample's port voltages, which
        /// is what keeps it at one to three iterations on an oversampled path.
        /// @tparam NonlinearSystem The caller's model glue; see the class documentation.
        /// @param nonlinear The model glue instance; referenced, never stored.
        /// @param inputs Source voltages u[n], in the order the sources were added.
        /// @param outputs Probe voltages y[n], overwritten; always finite.
        template <typename NonlinearSystem>
        void process_sample(NonlinearSystem&                nonlinear,
                            const std::array<double, I>&    inputs,
                            std::array<double, O>&          outputs)
        {
            static_cast<void>(nonlinear);
            state_backup_ = state_;

            if constexpr (N > 0)
            {
                // p[n] = D x[n-1] + E u[n]  (D6b plan, Per sample).
                for (std::size_t row = 0; row < N; ++row)
                {
                    double accumulator = 0.0;
                    for (std::size_t column = 0; column < S; ++column)
                    {
                        accumulator += matrix_d_[row][column] * state_[column];
                    }
                    for (std::size_t column = 0; column < I; ++column)
                    {
                        accumulator += matrix_e_[row][column] * inputs[column];
                    }
                    open_circuit_voltage_[row] = accumulator;
                }

                // The adapter holds five references and nothing else: it is a handful of
                // pointers on the stack, not an allocation.
                struct NewtonAdapter
                {
                    NonlinearSystem&                             system;
                    const std::array<double, N>&                 open_circuit;
                    const std::array<std::array<double, N>, N>&  f_matrix;
                    std::array<double, N>&                       current_scratch;
                    std::array<std::array<double, N>, N>&        model_jacobian;

                    void residual_and_jacobian(const std::array<double, N>&          v,
                                               std::array<double, N>&                residual,
                                               std::array<std::array<double, N>, N>& jacobian)
                    {
                        // r(v) = v - p - F f(v)  (D6b plan, Newton on the nonlinear port).
                        system.currents(v, current_scratch);
                        for (std::size_t row = 0; row < N; ++row)
                        {
                            double accumulator = v[row] - open_circuit[row];
                            for (std::size_t column = 0; column < N; ++column)
                            {
                                accumulator -= f_matrix[row][column] * current_scratch[column];
                            }
                            residual[row] = accumulator;
                        }

                        for (std::size_t row = 0; row < N; ++row)
                        {
                            for (std::size_t column = 0; column < N; ++column)
                            {
                                model_jacobian[row][column] = 0.0;
                            }
                        }
                        system.jacobian(v, model_jacobian);

                        // J(v) = Identity - F Jf(v).
                        for (std::size_t row = 0; row < N; ++row)
                        {
                            for (std::size_t column = 0; column < N; ++column)
                            {
                                double accumulator = row == column ? 1.0 : 0.0;
                                for (std::size_t inner = 0; inner < N; ++inner)
                                {
                                    accumulator -= f_matrix[row][inner] * model_jacobian[inner][column];
                                }
                                jacobian[row][column] = accumulator;
                            }
                        }
                    }
                };

                NewtonAdapter adapter{ nonlinear, open_circuit_voltage_, matrix_f_,
                                       model_current_, model_jacobian_ };
                if (!solver_.solve(adapter, nonlinear_voltage_))
                {
                    ++non_converged_samples_;
                }
                nonlinear.currents(nonlinear_voltage_, nonlinear_current_);
            }

            // y[n] = G x[n-1] + H u[n] + K i[n]  (D6b plan, Per sample).
            for (std::size_t row = 0; row < O; ++row)
            {
                outputs[row] = output_from_state(row, state_, inputs, true);
            }

            // x[n] = A x[n-1] + B u[n] + C i[n]  (D6b plan, Per sample).
            for (std::size_t row = 0; row < S; ++row)
            {
                double accumulator = 0.0;
                for (std::size_t column = 0; column < S; ++column)
                {
                    accumulator += matrix_a_[row][column] * state_[column];
                }
                for (std::size_t column = 0; column < I; ++column)
                {
                    accumulator += matrix_b_[row][column] * inputs[column];
                }
                for (std::size_t column = 0; column < N; ++column)
                {
                    accumulator += matrix_c_[row][column] * nonlinear_current_[column];
                }
                next_state_[row] = accumulator;
            }

            bool finite = true;
            for (std::size_t row = 0; row < S; ++row)
            {
                double value = next_state_[row];
                if (value > state_limit_)
                {
                    value = state_limit_;
                }
                else if (value < -state_limit_)
                {
                    value = -state_limit_;
                }
                state_[row] = value;
                finite = finite && std::isfinite(value);
            }
            for (std::size_t row = 0; row < O; ++row)
            {
                finite = finite && std::isfinite(outputs[row]);
            }
            for (std::size_t row = 0; row < N; ++row)
            {
                finite = finite && std::isfinite(nonlinear_current_[row])
                         && std::isfinite(nonlinear_voltage_[row]);
            }

            if (!finite)
            {
                state_ = state_backup_;
                for (std::size_t row = 0; row < N; ++row)
                {
                    nonlinear_current_[row] = 0.0;
                    nonlinear_voltage_[row] = 0.0;
                }
                for (std::size_t row = 0; row < O; ++row)
                {
                    outputs[row] = output_from_state(row, state_, inputs, false);
                }
                ++nan_fallback_samples_;
            }

            ++total_samples_;
        }

        /// @brief Sets a potentiometer's wiper position.
        ///
        /// Cheap and idempotent: an unchanged position marks nothing dirty, which is what makes
        /// calling this from a SmoothedParameter every block free while the knob is still.
        /// Cadence contract: the owning node calls update_matrices_if_dirty() once per block,
        /// after its smoothers advance and before the sample loop; never from inside the loop.
        /// @param index The index returned by DkNetlist::potentiometer; out-of-range is ignored.
        /// @param position Wiper position, clamped to [0, 1].
        void set_potentiometer(std::size_t index, double position)
        {
            if (index >= kMaximumPotentiometers)
            {
                return;
            }
            const double clamped = position < 0.0 ? 0.0 : (position > 1.0 ? 1.0 : position);
            if (clamped == pot_position_[index])
            {
                return;
            }
            pot_position_[index] = clamped;
            matrices_dirty_ = true;
        }

        /// @brief Re-derives the DK matrices when a potentiometer has moved since the last call.
        ///
        /// RT-safe. Cadence contract: call once per block, after the smoothers advance and
        /// before the sample loop; never from inside the sample loop.
        /// @return True when the matrices were re-derived; false when nothing was dirty, or when
        /// the new nodal system was singular and the previous matrices were kept.
        bool update_matrices_if_dirty()
        {
            if (!matrices_dirty_)
            {
                return false;
            }
            return derive();
        }

        /// @brief Clears state, port voltages, port currents and the statistics counters.
        ///
        /// Does not re-derive: a reset is a state clear, not a topology change.
        void reset()
        {
            for (std::size_t index = 0; index < S; ++index)
            {
                state_[index] = 0.0;
                state_backup_[index] = 0.0;
                next_state_[index] = 0.0;
            }
            for (std::size_t index = 0; index < N; ++index)
            {
                nonlinear_voltage_[index] = 0.0;
                nonlinear_current_[index] = 0.0;
                open_circuit_voltage_[index] = 0.0;
            }
            reset_statistics();
        }

        /// @brief The nonlinear branch currents from the most recent sample.
        ///
        /// D6d reads this to feed a power-supply-sag block one sample late (D6b plan, ruling 7).
        /// @return i[n], in the order the nonlinear branches were added.
        const std::array<double, N>& nonlinear_currents() const { return nonlinear_current_; }

        /// @brief The nonlinear branch voltages from the most recent sample.
        /// @return v[n], in the order the nonlinear branches were added.
        const std::array<double, N>& nonlinear_voltages() const { return nonlinear_voltage_; }

        /// @brief The reactive element states after the most recent sample.
        /// @return x[n], in the order the reactive elements were added.
        const std::array<double, S>& states() const { return state_; }

        /// @brief Samples whose Newton iteration did not converge since the last reset.
        /// @return The non-convergence count.
        std::uint64_t non_converged_samples() const { return non_converged_samples_; }

        /// @brief Samples that produced a non-finite value and fell back to the previous state.
        /// @return The fallback count.
        std::uint64_t nan_fallback_samples() const { return nan_fallback_samples_; }

        /// @brief Samples processed since the last reset.
        /// @return The sample count.
        std::uint64_t total_samples() const { return total_samples_; }

        /// @brief Worst-case Newton iteration count over any single sample since the last reset.
        /// @return The worst-case iteration count.
        std::uint32_t maximum_iterations_seen() const { return solver_.maximum_iterations_seen(); }

        /// @brief Clears the convergence, fallback and sample counters.
        void reset_statistics()
        {
            non_converged_samples_ = 0;
            nan_fallback_samples_ = 0;
            total_samples_ = 0;
            solver_.reset_statistics();
        }

        /// @brief Sets the magnitude each state is clamped to every sample.
        ///
        /// The clamp is what stops a transient from reaching the range where a device model's
        /// exponential overflows; the default of 1e4 is far outside any physical rail.
        /// @param limit Clamp magnitude; non-positive values are ignored.
        void set_state_limit(double limit)
        {
            if (limit > 0.0)
            {
                state_limit_ = limit;
            }
        }

    private:
        static bool in_range(const Branch& branch, std::size_t node_count)
        {
            return branch.positive_node <= node_count && branch.negative_node <= node_count;
        }

        /// Reads entry @p node of a node-indexed row, with ground reading as zero.
        static double node_value(const std::array<double, kMaximumNodes>& row, std::size_t node)
        {
            return node == 0 ? 0.0 : row[node - 1];
        }

        /// One column of `M Nt`: the branch's positive-node column minus its negative-node one.
        static double column_difference(const std::array<double, kMaximumNodes>& row,
                                        const Branch&                            branch)
        {
            return node_value(row, branch.positive_node) - node_value(row, branch.negative_node);
        }

        void stamp_conductance(const Branch& branch, double conductance)
        {
            const std::size_t a = branch.positive_node;
            const std::size_t b = branch.negative_node;
            if (a != 0)
            {
                system_[a - 1][a - 1] += conductance;
            }
            if (b != 0)
            {
                system_[b - 1][b - 1] += conductance;
            }
            if (a != 0 && b != 0)
            {
                system_[a - 1][b - 1] -= conductance;
                system_[b - 1][a - 1] -= conductance;
            }
        }

        double output_from_state(std::size_t                  row,
                                 const std::array<double, S>& state,
                                 const std::array<double, I>& inputs,
                                 bool                         include_current) const
        {
            double accumulator = 0.0;
            for (std::size_t column = 0; column < S; ++column)
            {
                accumulator += matrix_g_[row][column] * state[column];
            }
            for (std::size_t column = 0; column < I; ++column)
            {
                accumulator += matrix_h_[row][column] * inputs[column];
            }
            if (include_current)
            {
                for (std::size_t column = 0; column < N; ++column)
                {
                    accumulator += matrix_k_[row][column] * nonlinear_current_[column];
                }
            }
            return accumulator;
        }

        /// Rebuilds all nine DK matrices from the netlist and the current pot positions.
        ///
        /// Cost at the worst D6d shape (node_count_ + I = 13, S = 4, N = 10, O = 1): the LU
        /// factorisation is about 13^3/3 = 730 multiply-adds, the thirteen unit-vector solves
        /// about 13 * 13^2 = 2200, and the nine matrix products are row and column differences
        /// rather than dense multiplies, so together they are under a thousand more - roughly
        /// 4 kflop, on the order of three microseconds, once per block and only when a knob
        /// actually moved. That is why ruling 3 re-derives instead of maintaining a Woodbury
        /// update: against a 1.5 ms block budget the saving would be noise, and the rank-two
        /// update is far harder to keep correct.
        bool derive()
        {
            const std::size_t nodes = node_count_;
            const std::size_t order = nodes + I + netlist_.nullor_count_;

            for (std::size_t row = 0; row < order; ++row)
            {
                for (std::size_t column = 0; column < order; ++column)
                {
                    system_[row][column] = 0.0;
                }
            }

            for (std::size_t index = 0; index < netlist_.resistor_count_; ++index)
            {
                resistor_ohms_[index] = netlist_.resistors_[index].value;
            }
            for (std::size_t index = 0; index < netlist_.potentiometer_count_; ++index)
            {
                const PotentiometerEntry& entry = netlist_.potentiometers_[index];
                const double              position = pot_position_[index];
                const double              lower = entry.total_ohms * position;
                const double              upper = entry.total_ohms * (1.0 - position);
                resistor_ohms_[entry.first_section_index]
                    = lower < kMinimumPotOhms ? kMinimumPotOhms : lower;
                resistor_ohms_[entry.second_section_index]
                    = upper < kMinimumPotOhms ? kMinimumPotOhms : upper;
            }

            // S = Nres' Gres Nres + Nx' Gx Nx  (D6b plan, Nodal system).
            for (std::size_t index = 0; index < netlist_.resistor_count_; ++index)
            {
                stamp_conductance(netlist_.resistors_[index], 1.0 / resistor_ohms_[index]);
            }
            for (std::size_t index = 0; index < S; ++index)
            {
                stamp_conductance(netlist_.reactive_[index], gx_[index]);
            }

            // Asys = [ S Nu' ; Nu 0 ]  (D6b plan, Nodal system).
            for (std::size_t index = 0; index < I; ++index)
            {
                const std::size_t row = nodes + index;
                const std::size_t a = netlist_.sources_[index].positive_node;
                const std::size_t b = netlist_.sources_[index].negative_node;
                if (a != 0)
                {
                    system_[row][a - 1] += 1.0;
                    system_[a - 1][row] += 1.0;
                }
                if (b != 0)
                {
                    system_[row][b - 1] -= 1.0;
                    system_[b - 1][row] -= 1.0;
                }
            }

            // A nullor's row is its nullator (V(p) - V(q) = 0, right-hand side zero) and its
            // column is its norator (an unknown current into the driven pair). Unlike a source,
            // the two are different node pairs, which is exactly why an op-amp cannot be spelled
            // with voltage_source() (D6c plan, ruling 1).
            for (std::size_t index = 0; index < netlist_.nullor_count_; ++index)
            {
                const std::size_t row = nodes + I + index;
                const Branch&     sense = netlist_.nullors_[index].nullator;
                const Branch&     drive = netlist_.nullors_[index].norator;
                if (sense.positive_node != 0)
                {
                    system_[row][sense.positive_node - 1] += 1.0;
                }
                if (sense.negative_node != 0)
                {
                    system_[row][sense.negative_node - 1] -= 1.0;
                }
                if (drive.positive_node != 0)
                {
                    system_[drive.positive_node - 1][row] += 1.0;
                }
                if (drive.negative_node != 0)
                {
                    system_[drive.negative_node - 1][row] -= 1.0;
                }
            }

            // Every nullor puts a zero on the diagonal: its row touches only the sensed nodes
            // and its column only the driven ones. DenseLu's partial pivoting takes the largest
            // magnitude in each column rather than the diagonal entry, so those zeros cost it
            // nothing. It fails only when a whole column below the diagonal is zero, which is
            // the rank deficiency an op-amp outside a feedback loop leaves behind.
            if (!lu_.factorise(system_, order))
            {
                return false;
            }

            for (std::size_t column = 0; column < order; ++column)
            {
                for (std::size_t row = 0; row < order; ++row)
                {
                    unit_right_hand_side_[row] = row == column ? 1.0 : 0.0;
                }
                lu_.solve(unit_right_hand_side_, inverse_column_, order);
                for (std::size_t row = 0; row < order; ++row)
                {
                    inverse_[row][column] = inverse_column_[row];
                }
            }

            // Pleft is still inverse_[0..nodes)[0..nodes) and Pright still
            // inverse_[0..nodes)[nodes..nodes+I): the nullor rows and columns sit after the
            // sources, and their right-hand side is zero, so they shape the inverse without ever
            // being read out of it.
            build_left_and_right(netlist_.reactive_, reactive_left_, reactive_right_, nodes);
            build_left_and_right(netlist_.nonlinear_, nonlinear_left_, nonlinear_right_, nodes);
            build_left_and_right(netlist_.outputs_, output_left_, output_right_, nodes);

            // A = 2 Gx Nx Pleft Nx' Z - Z, B = 2 Gx Nx Pright, C = -2 Gx Nx Pleft Nnl'.
            for (std::size_t row = 0; row < S; ++row)
            {
                const double twice_gx = 2.0 * gx_[row];
                for (std::size_t column = 0; column < S; ++column)
                {
                    matrix_a_[row][column]
                        = twice_gx * column_difference(reactive_left_[row], netlist_.reactive_[column])
                          * netlist_.reactive_sign_[column];
                }
                matrix_a_[row][row] -= netlist_.reactive_sign_[row];
                for (std::size_t column = 0; column < I; ++column)
                {
                    matrix_b_[row][column] = twice_gx * reactive_right_[row][column];
                }
                for (std::size_t column = 0; column < N; ++column)
                {
                    matrix_c_[row][column] = -twice_gx
                        * column_difference(reactive_left_[row], netlist_.nonlinear_[column]);
                }
            }

            // D = Nnl Pleft Nx' Z, E = Nnl Pright, F = -Nnl Pleft Nnl'.
            for (std::size_t row = 0; row < N; ++row)
            {
                for (std::size_t column = 0; column < S; ++column)
                {
                    matrix_d_[row][column]
                        = column_difference(nonlinear_left_[row], netlist_.reactive_[column])
                          * netlist_.reactive_sign_[column];
                }
                for (std::size_t column = 0; column < I; ++column)
                {
                    matrix_e_[row][column] = nonlinear_right_[row][column];
                }
                for (std::size_t column = 0; column < N; ++column)
                {
                    matrix_f_[row][column]
                        = -column_difference(nonlinear_left_[row], netlist_.nonlinear_[column]);
                }
            }

            // G = No Pleft Nx' Z, H = No Pright, K = -No Pleft Nnl'.
            for (std::size_t row = 0; row < O; ++row)
            {
                for (std::size_t column = 0; column < S; ++column)
                {
                    matrix_g_[row][column]
                        = column_difference(output_left_[row], netlist_.reactive_[column])
                          * netlist_.reactive_sign_[column];
                }
                for (std::size_t column = 0; column < I; ++column)
                {
                    matrix_h_[row][column] = output_right_[row][column];
                }
                for (std::size_t column = 0; column < N; ++column)
                {
                    matrix_k_[row][column]
                        = -column_difference(output_left_[row], netlist_.nonlinear_[column]);
                }
            }

            matrices_dirty_ = false;
            return true;
        }

        /// Forms `Nb Pleft` and `Nb Pright` for one branch table: row k of the result is the
        /// positive node's row of P minus the negative node's, with ground rows read as zero.
        template <std::size_t Rows>
        void build_left_and_right(const std::array<Branch, Rows>&                      branches,
                                  std::array<std::array<double, kMaximumNodes>, Rows>& left,
                                  std::array<std::array<double, I>, Rows>&             right,
                                  std::size_t                                          nodes)
        {
            for (std::size_t row = 0; row < Rows; ++row)
            {
                const std::size_t a = branches[row].positive_node;
                const std::size_t b = branches[row].negative_node;
                for (std::size_t column = 0; column < nodes; ++column)
                {
                    const double positive = a == 0 ? 0.0 : inverse_[a - 1][column];
                    const double negative = b == 0 ? 0.0 : inverse_[b - 1][column];
                    left[row][column] = positive - negative;
                }
                for (std::size_t column = 0; column < I; ++column)
                {
                    const double positive = a == 0 ? 0.0 : inverse_[a - 1][nodes + column];
                    const double negative = b == 0 ? 0.0 : inverse_[b - 1][nodes + column];
                    right[row][column] = positive - negative;
                }
            }
        }

        DkNetlist<S, N, I, O> netlist_{};
        std::size_t           node_count_{ 0 };
        double                sample_rate_{ 0.0 };

        std::array<double, S> gx_{};

        std::array<std::array<double, S>, S> matrix_a_{};
        std::array<std::array<double, I>, S> matrix_b_{};
        std::array<std::array<double, N>, S> matrix_c_{};
        std::array<std::array<double, S>, N> matrix_d_{};
        std::array<std::array<double, I>, N> matrix_e_{};
        std::array<std::array<double, N>, N> matrix_f_{};
        std::array<std::array<double, S>, O> matrix_g_{};
        std::array<std::array<double, I>, O> matrix_h_{};
        std::array<std::array<double, N>, O> matrix_k_{};

        std::array<double, S> state_{};
        std::array<double, S> state_backup_{};
        std::array<double, S> next_state_{};

        std::array<double, N> nonlinear_voltage_{};
        std::array<double, N> nonlinear_current_{};
        std::array<double, N> open_circuit_voltage_{};
        std::array<double, N> model_current_{};

        std::array<std::array<double, N>, N> model_jacobian_{};

        DampedNewtonSolver<N> solver_{};

        std::array<std::array<double, kMaximumSystemOrder>, kMaximumSystemOrder> system_{};
        std::array<std::array<double, kMaximumSystemOrder>, kMaximumSystemOrder> inverse_{};
        std::array<double, kMaximumSystemOrder>                                  unit_right_hand_side_{};
        std::array<double, kMaximumSystemOrder>                                  inverse_column_{};
        DenseLu<kMaximumSystemOrder>                                             lu_{};

        std::array<std::array<double, kMaximumNodes>, S> reactive_left_{};
        std::array<std::array<double, I>, S>             reactive_right_{};
        std::array<std::array<double, kMaximumNodes>, N> nonlinear_left_{};
        std::array<std::array<double, I>, N>             nonlinear_right_{};
        std::array<std::array<double, kMaximumNodes>, O> output_left_{};
        std::array<std::array<double, I>, O>             output_right_{};

        std::array<double, kMaximumResistors>          resistor_ohms_{};
        std::array<double, kMaximumPotentiometers>     pot_position_{};

        bool matrices_dirty_{ true };

        std::uint64_t non_converged_samples_{ 0 };
        std::uint64_t nan_fallback_samples_{ 0 };
        std::uint64_t total_samples_{ 0 };

        double state_limit_{ 1e4 };
    };
}
