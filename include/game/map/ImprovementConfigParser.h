#pragma once

#include "game/effects/EffectConfig.h"
#include "game/effects/TriggeredEffect.h"
#include <nlohmann/json.hpp>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace ac
{

class Tile;

// Which tile surface a placed improvement may occupy. Omitted means either surface.
// Land and Sea match the enumerator names aside from case; parse with magic_enum.
enum class ImprovementDomain_t
{
    Land,
    Sea,
};

// Where an occupant comes from. Improvements are placed by a former or a founded base and
// live in Tile::GetImprovements(); terrain occupants come from world generation or a terrain
// operation and live in the tile's terrain list. Stamped by the parser from the file the
// entry was read out of, so one id can never be both.
enum class OccupantPlacement_t
{
    Terrain,
    Improvement,
};

// A tile occupant that grants effects: an improvement (Farm, Mine, Base) or a terrain
// feature (Rocky, Fungus, a landmark). Both live in ImprovementRegistry, told apart by
// `placement`, so effect collection and coexistence walk one pointer type and one lookup.
// The construction half of an improvement: present only when a former can build it.
struct FormerProject_t
{
    int turnsRequired = 0;
    int energyCost = 0;
    std::string requiredTech;
};

struct ImprovementConfig_t
{
    std::string id;
    std::string name;
    std::string description;
    OccupantPlacement_t placement = OccupantPlacement_t::Improvement;
    // What a former needs to build this improvement. Absent means none can: Base is placed by
    // founding, and terrain occupants come from world gen or a project's effects. A terrain
    // entry never has one, which is what makes "terrain with a build cost" unrepresentable
    // rather than three fields every terrain consumer has to know to ignore.
    std::optional<FormerProject_t> project;
    // Optional classification labels (e.g. "landmark", "landform"). Stored for later use and
    // available as "@tag" references in excludes / suppress_yield_sources (expanded at parse).
    std::vector<std::string> tags;
    // Occupancy after the feature is on the tile. A surface flip removes an improvement whose
    // domain is not the new surface. Empty means the improvement survives either surface.
    std::optional<ImprovementDomain_t> domain;
    std::vector<std::string> excludes; // feature ids that can't coexist with this one on a tile
    // Aura reach is per-effect (EffectConfig_t::radius); MaxEffectReach is derived from those.
    // Resolvers honour radius via TileEffectsContext::CollectAreaEffects.
    // When true, this improvement's effects only apply for the faction that owns the
    // host tile's territory (see TerritoryMap). Sensor is the canonical case — ownership is
    // a property of the improvement, not of individual effects.
    bool ownedByTerritory = false;
    int frequency = 0;                 // world-gen spawn weight; 0 = not randomly placed
    // Optional world-map sprites. Empty → TileRenderer paints a procedural fallback
    // (landform rings/centers today; tile bonuses simply omit the overlay). When more than
    // one path is listed, TileRenderer picks one deterministically from the tile coords.
    std::vector<std::string> spritePaths;
    // Feature/improvement ids whose yield StatModifiers are dropped while this improvement
    // is present (Forest suppresses landform; Borehole suppresses most terraform).
    std::vector<std::string> suppressYieldSources;
    // When true, downhill river flow marks this tile then stops (ThermalBorehole).
    bool terminatesRiver = false;
    // Optional move cost in fragments (JSON still uses move-points; conversion happens at
    // parse). On a tile, the highest moveCostFragments among features that define one is used.
    // A move_cost MaxClamp on a feature's effects, or on the entering unit, then ceilings
    // that price. If no feature defines a cost, defaultMoveCost applies.
    std::optional<int> moveCostFragments;
    std::vector<EffectConfig_t> effects;
    // One-shot effects for a unit deliberately visiting this improvement (Investigate). Fired
    // by ApplyVisitEffects after arrival when the player chooses Investigate (AI auto-fires).
    std::vector<TriggeredEffectConfig_t> onVisitEffects;
    // Fog sight range granted by this improvement's ThisTile Vision StatModifiers, resolved at
    // parse. Derived purely from `effects`, and read on every visibility rebuild — resolving it
    // per tile per rebuild allocated a vector and ran the stat resolver for static config data.
    // Effect radius is a separate axis (auras); this is sight only.
    int visionRadius = 0;
};

// True when a former can build this occupant.
bool IsBuildable(const ImprovementConfig_t& rConfig);

// Expand @tag references in excludes and suppress_yield_sources. A tag no entry declares
// throws. Self-references are skipped. Call on the full occupant list, so an improvement
// may name a tag that only terrain entries carry.
void ExpandFeatureTagReferences(std::vector<ImprovementConfig_t>& rConfigs);

// One occupant object. A terrain entry rejects construction fields; those belong on a
// terrain operation.
ImprovementConfig_t ParseImprovementBody(const nlohmann::json& rImprovementJson,
                                         OccupantPlacement_t placement);

// Parse improvements and terrain occupants into one list, tags expanded across both.
std::vector<ImprovementConfig_t> LoadTileOccupants(const std::string& rImprovementsPath,
                                                   const std::string& rTerrainPath);

// The same, for a caller that already parsed the terrain half (see ParseTerrainFile) and
// should not open that file a second time.
std::vector<ImprovementConfig_t> LoadTileOccupants(
    const std::string& rImprovementsPath, std::vector<ImprovementConfig_t> terrainFeatures);

// improvements.json with its @tags left unexpanded, for a caller that will expand them over
// a wider list. LoadTileOccupants does that across improvements and terrain together, so an
// improvement may name a tag only terrain entries carry.
std::vector<ImprovementConfig_t> ParseImprovementsUnexpanded(const std::string& rConfigPath);

class ImprovementConfigParser
{
public:
    // Registry<> adapter: this file alone, tags expanded against this file alone.
    std::vector<ImprovementConfig_t> ParseConfig(const std::string& configPath);
};

} // namespace ac
