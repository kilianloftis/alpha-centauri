#pragma once

#include <string>
#include <unordered_map>

namespace ac
{

struct StatBlock_t
{
    float base = 0.0f;
    float additiveMult = 0.0f;  // contributes to (1 + sum of all additive mults)
    float geometricMult = 1.0f; // contributes to product of all geometric mults
};

struct UnitComponentConfig_t
{
    std::string id;
    std::string name;
    std::string type; // e.g. "chassis", "weapon" — matches component_type in unit_slot_config.json
    std::string requiredTech;
    int mineralCost;
    std::unordered_map<std::string, StatBlock_t> stats;
    std::unordered_map<std::string, bool> flags;
    std::unordered_map<std::string, std::unordered_map<std::string, float>> bonusTables;
};

} // namespace ac
