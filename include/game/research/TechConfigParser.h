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
    // Continuous bonuses while this tech is discovered (e.g. FacilityEnergyUpkeep,
    // commerce_rating +1 for economic techs). ThisTech-scoped TechCost modifiers apply when
    // this tech is the research target, not after discovery.
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
