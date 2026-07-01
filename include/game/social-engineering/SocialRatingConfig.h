#pragma once

#include "lib/effects/BonusEffect.h"
#include "lib/effects/EffectEnums.h"
#include <map>
#include <stdexcept>
#include <string>
#include <vector>

namespace ac
{

// Configuration for one social engineering rating axis (e.g. "economy", "morale").
// Maps integer rating levels (positive and negative) to lists of gameplay effects.
// Levels not listed in the config produce no effects.
struct SocialRatingConfig
{
    std::string id;          // matches SocialRatingId string ("economy", "morale", …)
    SocialRatingId rating;
    std::map<int, std::vector<EffectConfig_t>> levelEffects;
};

inline std::string SocialRatingIdToString(SocialRatingId rating)
{
    switch (rating)
    {
        case SocialRatingId::Economy:    return "economy";
        case SocialRatingId::Efficiency: return "efficiency";
        case SocialRatingId::Support:    return "support";
        case SocialRatingId::Police:     return "police";
        case SocialRatingId::Morale:     return "morale";
        case SocialRatingId::Growth:     return "growth";
        case SocialRatingId::Planet:     return "planet";
        case SocialRatingId::Research:   return "research";
        case SocialRatingId::Industry:   return "industry";
        case SocialRatingId::Probe:      return "probe";
    }
    throw std::runtime_error("Unknown SocialRatingId");
}

} // namespace ac
