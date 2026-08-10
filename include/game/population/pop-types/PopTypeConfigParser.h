#pragma once

#include "game/effects/EffectConfig.h"
#include <nlohmann/json.hpp>
#include <string>
#include <vector>

namespace ac
{

// What a pop *is*, as declared by config. A closed partition: every pop has exactly one role,
// so IsPlainWorker / IsDrone / IsTalent / IsSpecialist cannot overlap.
enum class PopRole_t
{
    Worker,     // works a tile, no composition role
    Drone,      // works a tile, counts toward riots
    Talent,     // works a tile, counts toward golden ages
    Specialist  // does not work a tile
};

struct PopTypeConfig_t
{
    std::string id;
    std::string name;
    // Single character drawn in the base population strip. Required, because deriving it from
    // the id collides (Drone/Doctor, Talent/Technician) and the collision is invisible.
    char displayGlyph = '?';
    PopRole_t role = PopRole_t::Worker;
    bool bIsDefault = false;
    bool bCanWorkTile = false;
    bool bPlayerAssignable = false;
    // How many drones this pop counts as for riot. Defaults to 1 for role=drone, else 0.
    // Super Drone overrides to 2.
    int riotContribution = 0;
    std::vector<std::string> obsoletes;
    std::string requiredTech;
    std::string fallbackPopTypeId;
    std::vector<EffectConfig_t> effects;
};

class PopTypeConfigParser
{
public:
    PopTypeConfigParser() = default;
    ~PopTypeConfigParser() = default;

    std::vector<PopTypeConfig_t> ParseConfig(const std::string& configPath);

private:
    PopTypeConfig_t ParsePopTypeConfig(const nlohmann::json& popJson);
};

} // namespace ac
