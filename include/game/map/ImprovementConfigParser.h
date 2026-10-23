#pragma once

#include "game/effects/EffectConfig.h"
#include "game/effects/TriggeredEffect.h"
#include "game/display/OccupantArt.h"
#include <nlohmann/json.hpp>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
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

// Where an occupant comes from: a former or a founded base (improvements), or world gen and
// terrain operations (terrain). Set from the file it was read from.
enum class OccupantPlacement_t
{
    Terrain,
    Improvement,
};

// A tile occupant that grants effects: an improvement or a terrain feature, told apart by
// `placement`. The construction half of an improvement a former can build.
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
    // What a former needs to build it; absent when none can (Base, every terrain entry).
    std::optional<FormerProject_t> project;
    // Optional classification labels (e.g. "landmark", "landform"). Stored for later use and
    // available as "@tag" references in excludes / suppress_yield_sources (expanded at parse).
    std::vector<std::string> tags;
    // Occupancy after the feature is on the tile. A surface flip removes an improvement whose
    // domain is not the new surface. Empty means the improvement survives either surface.
    std::optional<ImprovementDomain_t> domain;
    std::vector<std::string> excludes; // feature ids that can't coexist with this one on a tile
    // Its effects apply only for the faction owning the host tile's territory (Sensor).
    bool ownedByTerritory = false;
    int frequency = 0;                 // world-gen spawn weight; 0 = not randomly placed
    // How this occupant draws on the world map; absent draws nothing.
    std::optional<OccupantArt_t> art;
    // Feature/improvement ids whose yield StatModifiers are dropped while this improvement
    // is present (Forest suppresses landform; Borehole suppresses most terraform).
    std::vector<std::string> suppressYieldSources;
    // Terrain kept on the tile but dormant while this terrain is present: Ocean keeps Fungus
    // dormant until the sea floor rises to the shelf.
    std::vector<std::string> suppressTerrain;
    // When true, downhill river flow marks this tile then stops (ThermalBorehole).
    bool terminatesRiver = false;
    // Move cost in fragments (move points in JSON). A tile uses the highest among its features,
    // then any move_cost MaxClamp; with none, defaultMoveCost.
    std::optional<int> moveCostFragments;
    std::vector<EffectConfig_t> effects;
    // One-shot effects for a unit deliberately visiting this improvement (Investigate). Fired
    // by ApplyVisitEffects after arrival when the player chooses Investigate (AI auto-fires).
    std::vector<TriggeredEffectConfig_t> onVisitEffects;
    // Sight range from its ThisTile Vision modifiers, resolved once at parse.
    int visionRadius = 0;
};

// True when a former can build this occupant.
bool IsBuildable(const ImprovementConfig_t& rConfig);

// Expands @tags in excludes, suppress lists and art hides; an unknown tag or id throws. Run
// over every occupant, since an improvement may name a terrain-only tag.
void ExpandFeatureTagReferences(std::vector<ImprovementConfig_t>& rConfigs);

// One occupant object. A terrain entry rejects construction fields; those belong on a
// terrain operation.
ImprovementConfig_t ParseImprovementBody(const nlohmann::json& rImprovementJson,
                                         OccupantPlacement_t placement);

// Parse improvements and the already-parsed terrain occupants (see ParseTerrainFile) into one
// list, tags expanded across both.
std::vector<ImprovementConfig_t> LoadTileOccupants(
    const std::string& rImprovementsPath, std::vector<ImprovementConfig_t> terrainFeatures);

// improvements.json with @tags unexpanded, for LoadTileOccupants to expand with terrain.
std::vector<ImprovementConfig_t> ParseImprovementsUnexpanded(const std::string& rConfigPath);

class ImprovementConfigParser
{
public:
    // Registry<> adapter: this file alone, tags expanded against this file alone.
    std::vector<ImprovementConfig_t> ParseConfig(const std::string& configPath);
};

} // namespace ac
