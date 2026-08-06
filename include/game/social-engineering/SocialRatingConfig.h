#pragma once

#include "game/effects/EffectConfig.h"
#include "game/effects/EffectEnums.h"
#include <magic_enum.hpp>
#include <algorithm>
#include <cctype>
#include <map>
#include <stdexcept>
#include <string>
#include <vector>

namespace ac
{

// Configuration for one social engineering rating axis (e.g. "economy", "morale").
// Maps integer rating levels (positive and negative) to lists of gameplay effects.
// Totals outside [min, max] of this table clamp to the extreme (SMAC rule); levels
// inside the range but not listed (including typical absent 0) produce no effects.
struct SocialRatingConfig_t
{
    std::string id;          // matches SocialRatingId_t string ("economy", "morale", …)
    SocialRatingId_t rating;
    std::map<int, std::vector<EffectConfig_t>> levelEffects;
};

// Config/wire form is lowercase enumerator name (economy, morale, …).
inline std::string SocialRatingIdToString(SocialRatingId_t rating)
{
    const auto name = magic_enum::enum_name(rating);
    if (name.empty())
    {
        throw std::runtime_error("Unknown SocialRatingId_t");
    }
    std::string result(name);
    std::transform(result.begin(), result.end(), result.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return result;
}

} // namespace ac
