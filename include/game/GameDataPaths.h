#pragma once

#include <string>

namespace ac
{

// Default filesystem locations for every config LoadGameData reads. Tests can override
// individual fields to point at fixtures / temp files.
struct GameDataPaths
{
    std::string popTypes = "config/pop_types.json";
    std::string buildings = "config/buildings";
    std::string improvements = "config/improvements.json";
    std::string unitComponents = "config/unit_components";
    std::string unitSlots = "config/unit_slot_config.json";
    std::string techs = "config/techs.json";
    std::string socialPolicies = "config/social_policies.json";
    std::string socialRatings = "config/social_rating_effects.json";
    std::string factions = "config/factions";
    std::string popComposition = "config/pop_composition.lua";
    std::string popGrowth = "config/pop_growth.json";
    std::string techCost = "config/tech_cost.lua";
    std::string worldGenPresets = "config/worldGen/presets.json";
    std::string worldGenDecoration = "config/worldGen/decoration.json";
    std::string worldGenLandmarks = "config/worldGen/landmarks.json";
    std::string tileYieldRules = "config/tile_yield_rules.json";
    std::string moraleLevels = "config/morale_levels.json";
    std::string probeActions = "config/probe_actions.json";
};

} // namespace ac
