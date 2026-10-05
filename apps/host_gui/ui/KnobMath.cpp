/****************************************************************/
/* KnobMath.cpp                                                 */
/* SushiDSP - https://github.com/SushiSystems/SushiDSP          */
/* Copyright (c) 2026 Sushi Systems                             */
/* Licensed under PolyForm Noncommercial 1.0.0. See LICENSE.    */
/* Commercial use requires a licence from Sushi Systems.        */
/****************************************************************/

#include "ui/KnobMath.hpp"

#include <algorithm>
#include <cmath>

namespace ui
{
    namespace
    {
        /// @brief Reports whether a parameter's range admits a logarithmic taper.
        bool uses_log_taper(const SushiDSP::ParameterDescriptor& param)
        {
            return param.kind == SushiDSP::ParamKind::Logarithmic
                && param.min > 0.0f && param.max > param.min;
        }
    }

    float value_to_normalized(const SushiDSP::ParameterDescriptor& param, float value)
    {
        const float clamped = std::clamp(value, param.min, param.max);
        if (uses_log_taper(param))
        {
            return std::log(clamped / param.min) / std::log(param.max / param.min);
        }
        if (param.max <= param.min)
        {
            return 0.0f;
        }
        return (clamped - param.min) / (param.max - param.min);
    }

    float normalized_to_value(const SushiDSP::ParameterDescriptor& param, float normalized)
    {
        const float position = std::clamp(normalized, 0.0f, 1.0f);
        if (uses_log_taper(param))
        {
            return param.min * std::pow(param.max / param.min, position);
        }
        if (param.max <= param.min)
        {
            return param.min;
        }
        return param.min + position * (param.max - param.min);
    }

    float normalized_to_angle(float normalized)
    {
        return kKnobStartRadians + std::clamp(normalized, 0.0f, 1.0f) * kKnobSweepRadians;
    }

    float apply_drag(float normalized, float pixels_dy, bool fine)
    {
        const float travel = fine ? kKnobFineDragPixels : kKnobDragPixels;
        return std::clamp(normalized - pixels_dy / travel, 0.0f, 1.0f);
    }
}
