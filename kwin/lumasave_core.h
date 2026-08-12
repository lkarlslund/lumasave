// SPDX-License-Identifier: MIT
#pragma once
#include <cstddef>
#include <cstdint>

extern "C" float lumasave_decide_backlight_scale(
    const std::uint64_t *bins,
    std::size_t binsLength,
    float maxReduction,
    float blackThreshold,
    float maxRmsError,
    float maxP99Error);

extern "C" float lumasave_full_scale_backlight_saving(
    float userBrightness,
    float relativeReduction);
