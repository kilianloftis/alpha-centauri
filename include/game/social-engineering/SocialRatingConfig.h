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

// One rating axis: levels map to effects. Totals past the table clamp to its extremes (SMAC);
// unlisted levels inside it produce nothing.
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
