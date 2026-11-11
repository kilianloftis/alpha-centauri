#pragma once

#include "game/GameCategory.h"
#include "game/effects/EffectConfig.h"
#include "game/effects/TriggeredEffect.h"

#include <string>
#include <vector>

namespace ac
{

using TechId = std::string;

// alphax.txt #TECHNOLOGY columns 3-6: how much an AI with each priority wants the tech.
struct TechAiWeights_t
{
    int power = 0;
    int tech = 0;
    int wealth = 0;
    int growth = 0;
};

struct TechConfig_t
{
    std::string id;
    std::string name;
    GameCategory_t category;
    TechAiWeights_t aiWeights;
    std::vector<std::string> prerequisites;
    // Optional UI icon path (research panels). Empty = text-only.
    std::string icon;
    // Continuous while the tech is discovered. ThisTech TechCost modifiers apply while it is the
    // research target.
    std::vector<EffectConfig_t> effects;
    // One-shot effects when this tech joins a faction's discovered set (research, probe,
    // diplomatic grant, nested GrantTech). Fired via ApplyTechDiscoverEffects.
    std::vector<TriggeredEffectConfig_t> onDiscoverEffects;
};

class TechConfigParser
{
public:
    static std::vector<TechConfig_t> ParseConfig(const std::string& configPath);
};

} // namespace ac
