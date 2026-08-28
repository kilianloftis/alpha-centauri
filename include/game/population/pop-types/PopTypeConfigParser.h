#pragma once

#include "game/effects/EffectConfig.h"
#include <nlohmann/json.hpp>
#include <string>
#include <vector>

namespace ac
{

// Where a pop type sits in the promotion graph, relative to the is_default type. Derived by
// PopTypeRegistry at load — never authored in config. It replaces the old `role` key, which
// could contradict every field it was meant to summarise (role:drone with riot weight 0, a
// specialist that worked tiles). A derived class cannot drift from the graph it is read off.
enum class PopClass_t
{
    Drone,        // promotes toward the default: SuperDrone, Drone
    PlainWorker,  // is_default itself
    Talent,       // the default promotes toward it
    Outside       // not in the promotion graph at all: Doctor, Technician, …
};

struct PopTypeConfig_t
{
    std::string id;
    std::string name;
    // Single character drawn in the base population strip. Required, because deriving it from
    // the id collides (Drone/Doctor, Talent/Technician) and the collision is invisible.
    char displayGlyph = '?';
    bool bIsDefault = false;
    bool bCanWorkTile = false;
    bool bPlayerAssignable = false;
    // Promotion graph edge: this type promotes to promotesToId for psychToPromote psych.
    // Both or neither — PopTypeRegistry rejects one without the other.
    int psychToPromote = 0;
    std::string promotesToId;
    // Drone pressure one body of this type absorbs (Drone 1, SuperDrone 2). 0 for every type
    // that is not drone-class. Read by composition on counts, before any pop exists, which is
    // why it is a plain scalar rather than a ThisPop effect like the mood weights.
    int droneWeight = 0;
    std::vector<std::string> obsoletes;
    std::string requiredTech;
    std::string fallbackPopTypeId;
    std::vector<EffectConfig_t> effects;
    // Filled by PopTypeRegistry::Validate_, not by the parser.
    PopClass_t derivedClass = PopClass_t::Outside;
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
