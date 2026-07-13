#pragma once

#include "game/effects/BonusEffect.h"
#include "lib/Rational.h"
#include <nlohmann/json.hpp>
#include <optional>
#include <string>
#include <vector>

namespace ac
{

class Tile;

// A single tile feature definition: a terrain classification (Flat/Rolling/Rocky,
// Arid/Moist/Wet), a natural feature (River, Fungus), or an improvement (Farm, Mine, Bunker,
// Base, and what were formerly "bonus"/"landmark" specials - all just improvements now).
// Terrain and improvements share this type and resolve effects/exclusivity identically; they
// differ only in how they live on a Tile: terrain is intrinsic (enums/bools, listed by
// Tile::GetTerrainFeatureIds() and looked up by id), while improvements are held directly as
// ImprovementConfig_t pointers in Tile::GetImprovements().
struct ImprovementConfig_t
{
    std::string id;
    std::string name;
    std::string description;           // optional flavour text (used for tile bonuses)
    int mineralCost = 0;               // 0 for terrain/natural features (not buildable)
    std::string requiredTech;          // empty if not tech-gated
    std::vector<std::string> excludes; // feature ids that can't coexist with this one on a tile
    // Aura reach is per-effect (EffectConfig_t::radius); MaxEffectReach is derived from those.
    // Resolvers honour radius via TileEffectsContext::CollectAreaEffects.
    // When true, this improvement's effects only apply for the faction that owns the
    // host tile's territory (see TerritoryMap). Sensor is the canonical case — ownership is
    // a property of the improvement, not of individual effects.
    bool ownedByTerritory = false;
    int frequency = 0;                 // world-gen spawn weight; 0 = not randomly placed
    std::string spritePath;            // optional sprite override (used for tile bonuses)
    // Move cost in move-points (converted to fragments via k_moveFragmentsPerPoint).
    // Defaults to 1 when omitted in JSON. On a tile, the highest moveCost wins unless any
    // moveCostOverride is present, in which case the lowest override wins (roads / mag tubes).
    Rational_t moveCost{1, 1};
    std::optional<Rational_t> moveCostOverride;
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
    ImprovementConfig_t ParseImprovementConfig(const nlohmann::json& improvementJson);
};

} // namespace ac
