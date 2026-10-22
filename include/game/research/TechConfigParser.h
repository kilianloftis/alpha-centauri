#pragma once

#include "game/GameCategory.h"
#include "game/effects/EffectConfig.h"
#include "game/effects/TriggeredEffect.h"

#include <nlohmann/json.hpp>
#include <string>
#include <vector>

namespace ac
{

using TechId = std::string;

struct TechConfig_t
{
    std::string id;
    std::string name;
    GameCategory_t category;
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
    TechConfigParser() = default;
    ~TechConfigParser() = default;

    std::vector<TechConfig_t> ParseConfig(const std::string& configPath);

private:
    TechConfig_t ParseTechConfig_(const nlohmann::json& techJson);
};

} // namespace ac
