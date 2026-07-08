#pragma once

#include "lib/effects/BonusEffect.h"
#include <string>
#include <unordered_map>
#include <vector>

namespace ac
{

struct FactionIdentityConfig
{
    std::string name;
    std::string descriptiveName;
    std::string noun;
    std::string adjective;
};

struct LeaderConfig
{
    std::string name;
    std::string title;
};

struct AITendenciesConfig
{
    float wealth = 0.5f;
    float power = 0.5f;
    float growth = 0.5f;
    float tech = 0.5f;
};

struct FactionFlavorConfig
{
    std::vector<std::string> baseNames;
    std::unordered_map<std::string, std::vector<std::string>> phrases;
};

struct FactionConfig_t
{
    std::string id;
    FactionIdentityConfig identity;
    LeaderConfig leader;
    AITendenciesConfig ai;
    FactionFlavorConfig flavor;
    std::vector<EffectConfig_t> effects;
};

} // namespace ac
