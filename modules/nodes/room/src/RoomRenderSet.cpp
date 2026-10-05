/****************************************************************/
/* RoomRenderSet.cpp                                            */
/* SushiDSP - https://github.com/SushiSystems/SushiDSP          */
/* Copyright (c) 2026 Sushi Systems                             */
/* Licensed under PolyForm Noncommercial 1.0.0. See LICENSE.    */
/* Commercial use requires a licence from Sushi Systems.        */
/****************************************************************/

/**
 * @file RoomRenderSet.cpp
 * @brief The control-thread build that joins the four solvers and the HRTF into one render set.
 * @author Mustafa Garip & Sushi Systems
 */

#include "SushiDSP/room/RoomRenderSet.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <stdexcept>
#include <string>

#include "SushiDSP/room/BandResponse.hpp"
#include "SushiDSP/room/EyringDecay.hpp"
#include "SushiDSP/room/LateReverbDesign.hpp"
#include "SushiDSP/room/ModalSolver.hpp"

namespace SushiDSP::room
{
    namespace
    {
        /// @brief Holds the reflection order every set is solved to. Spec §4.1.
        constexpr std::uint32_t kImageOrder = 4;

        /// @brief Bounds the sample rate from below, in hertz, the image solver's own rule.
        constexpr double kMinSampleRate = 8000.0;

        /// @brief Holds a receiver's pose: where the direct path ends and which way it faces.
        struct Receiver
        {
            /// @brief Stores the receiver's position.
            Vec3 position;

            /// @brief Stores the receiver's yaw, in radians, zero facing +x.
            double yaw_rad;
        };

        /// @brief Returns the receiver the listener kind names, the one the image solver ends
        /// its paths at.
        Receiver receiver_for(const RoomScene& scene, ListenerKind listener) noexcept
        {
            if (listener == ListenerKind::Microphone)
            {
                return { scene.microphone_position, scene.microphone_yaw_rad };
            }
            return { scene.listener_position, scene.listener_yaw_rad };
        }

        /// @brief Throws std::runtime_error naming the first rule the inputs break.
        void validate(const RoomScene& scene, const HrtfSet& hrtf, double sample_rate)
        {
            std::string reason;
            if (!scene.is_valid(reason))
            {
                throw std::runtime_error("RoomRenderSet: " + reason);
            }
            if (!(sample_rate > kMinSampleRate))
            {
                throw std::runtime_error("RoomRenderSet: sample rate " + std::to_string(sample_rate)
                                         + " Hz is not above " + std::to_string(kMinSampleRate)
                                         + " Hz");
            }
            if (hrtf.empty())
            {
                throw std::runtime_error("RoomRenderSet: the HRTF set holds no direction");
            }
            if (hrtf.sample_rate() != sample_rate)
            {
                throw std::runtime_error("RoomRenderSet: the HRTF set was measured at "
                                         + std::to_string(hrtf.sample_rate())
                                         + " Hz and the node runs at "
                                         + std::to_string(sample_rate) + " Hz");
            }
        }

        /// @brief Fills the direct path: its delay and 1 / r from the source to the receiver, and
        /// the HRIR pair at the arrival direction in the receiver's frame.
        void fill_direct_path(RoomRenderSet& set, const RoomScene& scene, const HrtfSet& hrtf,
                              double sample_rate, const Receiver& receiver)
        {
            const double dx = scene.source_position.x - receiver.position.x;
            const double dy = scene.source_position.y - receiver.position.y;
            const double dz = scene.source_position.z - receiver.position.z;
            const double distance = std::sqrt(dx * dx + dy * dy + dz * dz);
            const double azimuth = std::atan2(dy, dx) - receiver.yaw_rad;
            const double elevation =
                distance > 0.0 ? std::asin(std::clamp(dz / distance, -1.0, 1.0)) : 0.0;

            const HrtfSet::Pair pair = hrtf.interpolate(azimuth, elevation);
            set.direct_left = pair.left;
            set.direct_right = pair.right;
            set.direct_itd_frames = pair.itd_frames;
            set.direct_delay_frames =
                static_cast<float>(std::round(distance / kSpeedOfSound * sample_rate));
            set.direct_gain = distance > 0.0 ? static_cast<float>(1.0 / distance) : 1.0f;
        }
    }

    std::unique_ptr<RoomRenderSet> build_render_set(const RoomScene& scene, const HrtfSet& hrtf,
                                                    double sample_rate, ListenerKind listener)
    {
        validate(scene, hrtf, sample_rate);

        auto set = std::make_unique<RoomRenderSet>();
        ImageSourceSolver::Options options;
        options.max_order = kImageOrder;
        options.listener = listener;
        options.sample_rate = sample_rate;
        set->early = ImageSourceSolver::solve(scene, options);
        set->late = LateReverbDesign::design(scene, set->early, sample_rate, listener);

        const double schroeder_hz = EyringDecay::schroeder_frequency_hz(scene);
        set->modal = ModalSolver::design(scene, sample_rate, schroeder_hz, listener);
        set->crossover_hz = set->modal.crossover_hz;

        fill_direct_path(*set, scene, hrtf, sample_rate, receiver_for(scene, listener));
        set->diffuse_left = hrtf.diffuse_field(0);
        set->diffuse_right = hrtf.diffuse_field(1);
        set->listener = listener;
        return set;
    }
}
