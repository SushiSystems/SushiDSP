/**************************************************************************/
/* Preset.cpp                                                             */
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

#include "Preset.hpp"

#include <fstream>
#include <map>
#include <stdexcept>
#include <vector>

#include <nlohmann/json.hpp>

#include "SushiDSP/node_registry/NodeRegistry.hpp"

void save_preset(RigController& controller, const std::string& path)
{
    nlohmann::json root;
    root["nodes"] = nlohmann::json::array();

    const auto ids = controller.chain_ids();
    for (std::size_t i = 0; i < ids.size(); ++i)
    {
        nlohmann::json node_json;
        node_json["type"] = ids[i];
        node_json["params"] = nlohmann::json::object();

        for (const auto& entry : SushiDSP::node_registry())
        {
            if (ids[i] == entry.descriptor->id)
            {
                for (std::size_t p = 0; p < entry.descriptor->parameter_count; ++p)
                {
                    const char* param_id = entry.descriptor->parameters[p].id;
                    node_json["params"][param_id] = controller.get_parameter(i, param_id);
                }
                break;
            }
        }
        root["nodes"].push_back(node_json);
    }

    std::ofstream out(path);
    if (!out)
    {
        throw std::runtime_error("save_preset: could not open '" + path + "' for writing");
    }
    out << root.dump(2);
}

void load_preset(RigController& controller, const std::string& path)
{
    std::ifstream in(path);
    if (!in)
    {
        throw std::runtime_error("load_preset: could not open '" + path + "' for reading");
    }

    nlohmann::json root;
    in >> root;

    std::vector<std::string> ids;
    std::vector<std::map<std::string, float>> params;

    for (const auto& node_json : root.at("nodes"))
    {
        ids.push_back(node_json.at("type").get<std::string>());

        std::map<std::string, float> node_params;
        for (const auto& [key, value] : node_json.at("params").items())
        {
            node_params[key] = value.get<float>();
        }
        params.push_back(std::move(node_params));
    }

    controller.load_chain(ids, params);
}
