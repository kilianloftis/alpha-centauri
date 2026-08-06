#pragma once

#include "game/effects/EffectConfig.h"
#include <nlohmann/json.hpp>
#include <optional>
#include <string>
#include <vector>

namespace ac
{

class Tile;

// What happens when a Former finishes a terraform order for this config.
// place (default): AddImprovementWithEffects. Other values mutate the tile and never
// add this config id as a tile feature.
enum class TerraformResult_t
{
    Place,
    LevelTerrain,
    RaiseLand,
    LowerLand,
    PlantFungus,
    RemoveFungus,
    Aquifer,
};

// A single tile feature definition: a terrain classification (Flat/Rolling/Rocky,
// Arid/Moist/Wet), a natural feature (River, Fungus), or an improvement (Farm, Mine, Bunker,
// Base, and what were formerly "bonus"/"landmark" specials - all just improvements now).
// Terrain and improvements share this type and resolve effects/exclusivity identically; they
// differ only in how they live on a Tile: terrain enums/bools are mirrored as config pointers
// via Tile::GetTerrainFeatures(), while improvements are held directly in
// Tile::GetImprovements().
struct ImprovementConfig_t
{
    std::string id;
    std::string name;
    std::string description;           // optional flavour text (used for tile bonuses)
    int turnsRequired = 0;             // 0 = not a Former project
    int energyCost = 0;                // flat energy at order start; raise/lower may override
    std::string requiredTech;          // empty if not tech-gated
    // Optional classification labels (e.g. "landmark", "landform"). Stored for later use and
    // available as "@tag" references in excludes / suppress_yield_sources (expanded at parse).
    std::vector<std::string> tags;
    std::vector<std::string> excludes; // feature ids that can't coexist with this one on a tile
    // Aura reach is per-effect (EffectConfig_t::radius); MaxEffectReach is derived from those.
    // Resolvers honour radius via TileEffectsContext::CollectAreaEffects.
    // When true, this improvement's effects only apply for the faction that owns the
    // host tile's territory (see TerritoryMap). Sensor is the canonical case — ownership is
    // a property of the improvement, not of individual effects.
    bool ownedByTerritory = false;
    int frequency = 0;                 // world-gen spawn weight; 0 = not randomly placed
    std::string spritePath;            // optional sprite override (used for tile bonuses)
    TerraformResult_t terraformResult = TerraformResult_t::Place;
    // Feature/improvement ids whose yield StatModifiers are dropped while this improvement
    // is present (Forest suppresses landform; Borehole suppresses most terraform).
    std::vector<std::string> suppressYieldSources;
    // When true, downhill river flow marks this tile then stops (ThermalBorehole).
    bool terminatesRiver = false;
    // Optional move cost in fragments (JSON still uses move-points; conversion happens at
    // parse). On a tile, the highest moveCostFragments among features that define one is used,
    // unless any feature defines moveCostOverrideFragments — then the lowest override replaces
    // the cost entirely (even if higher than the max moveCostFragments). If neither is present,
    // defaultMoveCost applies.
    std::optional<int> moveCostFragments;
    std::optional<int> moveCostOverrideFragments;
    std::vector<EffectConfig_t> effects;
};

// Returns true if none of rCandidate's excludes are present among rTile's current feature ids.
// Does not check requiredTech/turnsRequired/energyCost - those are construction-flow concerns.
bool CanBuildImprovement(const Tile& rTile, const ImprovementConfig_t& rCandidate);

class ImprovementConfigParser
{
public:
    ImprovementConfigParser() = default;
    ~ImprovementConfigParser() = default;

    std::vector<ImprovementConfig_t> ParseConfig(const std::string& configPath);

private:
    ImprovementConfig_t ParseImprovementConfig(const nlohmann::json& improvementJson);
    void ExpandTagReferences(std::vector<ImprovementConfig_t>& rConfigs) const;
};

} // namespace ac
