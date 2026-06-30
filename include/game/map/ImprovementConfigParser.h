#pragma once

#include "lib/effects/BonusEffect.h"
#include <nlohmann/json.hpp>
#include <string>
#include <vector>

namespace ac
{

class Tile;

// A single tile feature definition: a terrain classification (Flat/Rolling/Rocky,
// Arid/Moist/Wet), a natural feature (River, Fungus), a landmark, or a player-built
// improvement (Farm, Mine, Bunker, ...). All of these are looked up the same way via
// Tile::GetFeatureIds() - terrain and improvements differ in how they're set on a Tile,
// not in how their effects/exclusivity are resolved.
struct ImprovementConfig_t
{
    std::string id;
    std::string name;
    int mineralCost = 0;               // 0 for terrain/natural features (not buildable)
    std::string requiredTech;          // empty if not tech-gated
    std::vector<std::string> excludes; // feature ids that can't coexist with this one on a tile
    std::vector<EffectConfig_t> effects;
};

// Returns true if none of rCandidate's excludes are present among rTile's current feature ids.
// Does not check requiredTech/mineralCost - those are construction-flow concerns, not modeled yet.
bool CanBuildImprovement(const Tile& rTile, const ImprovementConfig_t& rCandidate);

class ImprovementConfigParser
{
public:
    ImprovementConfigParser() = default;
    ~ImprovementConfigParser() = default;

    std::vector<ImprovementConfig_t> ParseConfig(const std::string& configPath);

private:
    std::vector<ImprovementConfig_t> ParseFile_(const std::string& filePath);
    ImprovementConfig_t ParseImprovementConfig(const nlohmann::json& improvementJson);
};

} // namespace ac
