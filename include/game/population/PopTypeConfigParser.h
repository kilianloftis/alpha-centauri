#pragma once

#include "game/effects/EffectConfig.h"
#include <string>
#include <vector>

namespace ac
{

// A type's place in the promotion graph relative to the default type, derived at load so it
// cannot contradict the graph.
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
    // Drone pressure one body absorbs (Drone 1, SuperDrone 2); a scalar, since composition reads it
    // before any pop exists.
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
};

} // namespace ac
