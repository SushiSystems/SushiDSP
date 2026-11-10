/**************************************************************************/
/* in_memory_room.hpp                                                     */
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
 * @file in_memory_room.hpp
 * @brief Declares the in-memory adapter of the RoomHost port: a room node that holds its arrangement and renders silence.
 * @author Mustafa Garip
 */

#pragma once

#include <algorithm>
#include <cstdint>
#include <memory>
#include <string>

#include <SushiDSP/foundation/core/audio_node.hpp>
#include <SushiDSP/foundation/core/node_descriptor.hpp>
#include <SushiDSP/nodes/room/eyring_decay.hpp>
#include <SushiDSP/nodes/room/room_host.hpp>
#include <SushiDSP/nodes/room/room_node.hpp>
#include <SushiDSP/nodes/room/starter_scene.hpp>

namespace TestSupport
{
    /// @brief A room node of the real room's shape that keeps what a host installs and builds no render set.
    ///
    /// It keeps every promise of RoomHost that a host can read back: an arrangement or a scene that is not
    /// valid is refused and leaves the one in place, an accepted one is returned by arrangement() and
    /// scene(), and an HRTF set names its source. It declares the real room's ports and writes silence on
    /// all of them. It solves no pair and builds no filter, so a test of a host's wiring does not pay for
    /// the acoustics.
    class InMemoryRoom final : public SushiDSP::AudioNode, public SushiDSP::room::RoomHost
    {
        public:
            /// @brief Makes a room holding the starter arrangement, as the real room does.
            InMemoryRoom()
                : arrangement_(SushiDSP::Room::Arrangement::from_scene(SushiDSP::room::starter_scene()))
            {
            }

            /// @brief Does nothing; the node holds no state that depends on the rate.
            void prepare(const SushiDSP::PrepareInfo&) override
            {
            }

            /// @brief Writes silence on the listener port and on every microphone port.
            void process(SushiDSP::ProcessContext& context) override
            {
                const std::uint32_t ports = output_port_count();
                for (std::uint32_t port = 0; port < ports; ++port)
                {
                    float* const* outputs =
                        context.port_outputs != nullptr ? context.port_outputs[port] : context.outputs;
                    const std::uint32_t channels = output_port_channels(port);
                    for (std::uint32_t channel = 0; channel < channels; ++channel)
                    {
                        std::fill_n(outputs[channel], context.frames, 0.0f);
                    }
                }
            }

            /// @brief Returns the real room's layout.
            SushiDSP::PortLayout ports() const override
            {
                return {SushiDSP::room::RoomNode::K_MAX_SOURCES * SushiDSP::room::RoomNode::K_SLOT_CHANNELS, 2};
            }

            /// @brief Returns the real room's port count.
            std::uint32_t output_port_count() const override
            {
                return SushiDSP::room::RoomNode::K_OUTPUT_PORTS;
            }

            /// @brief Returns two channels on the listener port and one on each microphone port.
            std::uint32_t output_port_channels(std::uint32_t port) const override
            {
                if (port == SushiDSP::room::RoomNode::K_LISTENER_PORT)
                {
                    return 2u;
                }
                return port < SushiDSP::room::RoomNode::K_OUTPUT_PORTS ? 1u : 0u;
            }

            /// @brief Returns the direct delay the host wrote, and zero for any other id.
            float get_parameter(const char* id) const override
            {
                return std::string(id) == "direct_delay" ? direct_delay_ : 0.0f;
            }

            /// @brief Keeps the direct delay; any other id changes nothing.
            void set_parameter_for_control_thread(const char* id, float value) override
            {
                if (std::string(id) == "direct_delay")
                {
                    direct_delay_ = value;
                }
            }

            /// @brief Does nothing; the node holds no rendering state.
            void reset() override
            {
            }

            /// @brief Returns the real room's type id.
            const char* type_id() const override
            {
                return "sushidsp.room";
            }

            /// @brief Keeps the arrangement when it is valid.
            /// @return False and the reason when it is not; the one in place stays.
            bool set_arrangement(const SushiDSP::Room::Arrangement& arrangement, std::string& reason_out) override
            {
                if (!arrangement.is_valid(reason_out))
                {
                    return false;
                }
                arrangement_ = arrangement;
                return true;
            }

            /// @brief Returns the arrangement in place.
            SushiDSP::Room::Arrangement arrangement() const override
            {
                return arrangement_;
            }

            /// @brief Replaces the shell, the listener and the first cabinet from one scene, as the real room does.
            bool set_scene(const SushiDSP::room::RoomScene& scene, std::string& reason_out) override
            {
                if (!scene.is_valid(reason_out))
                {
                    return false;
                }
                SushiDSP::Room::Arrangement next = arrangement_;
                next.set_shell(scene);
                next.set_listener(SushiDSP::Room::Pose{scene.listener_position, scene.listener_yaw_rad});
                const SushiDSP::Room::Pose cabinet{scene.source_position, scene.source_yaw_rad};
                if (next.sources().empty())
                {
                    next.add_source(SushiDSP::Room::SourcePlacement{0u, cabinet, scene.cabinet}, reason_out);
                }
                else
                {
                    next.move_source(next.sources().front().id, cabinet);
                    next.set_source_cabinet(next.sources().front().id, scene.cabinet);
                }
                return set_arrangement(next, reason_out);
            }

            /// @brief Returns the shell with the listener's pose and the first cabinet's.
            SushiDSP::room::RoomScene scene() const override
            {
                SushiDSP::room::RoomScene scene = arrangement_.shell();
                scene.listener_position = arrangement_.listener().position;
                scene.listener_yaw_rad = arrangement_.listener().yaw_rad;
                if (!arrangement_.sources().empty())
                {
                    scene.source_position = arrangement_.sources().front().pose.position;
                    scene.source_yaw_rad = arrangement_.sources().front().pose.yaw_rad;
                    scene.cabinet = arrangement_.sources().front().cabinet;
                }
                return scene;
            }

            /// @brief Names the source of the set; a measured set is installed whatever its rate.
            [[nodiscard]] SushiDSP::room::HrtfStatus set_hrtf(SushiDSP::room::HrtfSet set) override
            {
                source_ = set.empty() ? SushiDSP::room::HrtfSource::SphericalHead
                                      : SushiDSP::room::HrtfSource::Measured;
                return SushiDSP::room::HrtfStatus::Installed;
            }

            /// @brief Returns where the set in place came from.
            SushiDSP::room::HrtfSource hrtf_source() const override
            {
                return source_;
            }

            /// @brief Returns Eyring's figure for the arrangement in place.
            double schroeder_frequency_hz() const override
            {
                return SushiDSP::room::EyringDecay::schroeder_frequency_hz(arrangement_.pair_scene(0, 0));
            }

            /// @brief Does nothing; the node holds no render set.
            void reclaim_rooms() override
            {
            }

        private:
            SushiDSP::Room::Arrangement arrangement_;
            SushiDSP::room::HrtfSource source_ = SushiDSP::room::HrtfSource::SphericalHead;
            float direct_delay_ = 0.0f;
    };

    /// @brief Returns the descriptor of the in-memory room, under the real room's id, ports and parameters.
    inline const SushiDSP::NodeDescriptor& in_memory_room_descriptor()
    {
        static const SushiDSP::NodeDescriptor descriptor{
            "sushidsp.room",
            "Room",
            SushiDSP::room::RoomNode::parameters().descriptors(),
            SushiDSP::room::RoomNode::parameters().count(),
            SushiDSP::PortLayout{SushiDSP::room::RoomNode::K_MAX_SOURCES * SushiDSP::room::RoomNode::K_SLOT_CHANNELS,
                                 2},
            [](const SushiDSP::PrepareInfo&) -> std::unique_ptr<SushiDSP::AudioNode>
            {
                return std::make_unique<InMemoryRoom>();
            },
        };
        return descriptor;
    }
} // namespace TestSupport
