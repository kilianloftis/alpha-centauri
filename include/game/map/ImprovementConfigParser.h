#pragma once

#include "game/effects/EffectConfig.h"
#include "game/effects/TriggeredEffect.h"
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

// World-map art per tile surface: JSON "sprite_paths": {"land": [...], "sea": [...]}. A surface
// left out has no art there.
struct OccupantSpritePaths_t
{
    std::vector<std::string> land;
    std::vector<std::string> sea;
};

// How a tile set picks its sprite from the tile's neighbors (see ui/TileAutotile.h). Edges and
// Blob match the JSON names aside from case; parse with magic_enum.
enum class SpriteTileLayout_t
{
    Edges,
    Blob,
    // SMAC's road network: {mask} 0 is the hub, 1–8 one link each toward the NW edge, N corner,
    // NE edge, E corner, SE edge, S corner, SW edge and W corner.
    Links,
};

// World-map art drawn one sprite per neighbor mask: JSON "sprite_tiles": {"layout": "edges" |
// "blob" | "links", "land": "...{mask}...", "sea": "..."}. An empty pattern means no art on that
// surface. A links set also names "link_occupants" (a tile carrying any of them joins the
// network; Base carries every network) and may name "replaces_links_of": the network whose
// link it draws over where both tiles carry this one (a mag tube over a road).
struct OccupantSpriteTiles_t
{
    SpriteTileLayout_t layout = SpriteTileLayout_t::Edges;
    std::string land;
    std::string sea;
    std::vector<std::string> linkOccupants;
    std::string replacesLinksOf;
};

// The yield an object sprite row follows.
enum class YieldStat_t
{
    Nutrients,
    Minerals,
    Energy,
};

// Object sprites picked by the tile's yield: JSON "sprite_yield_rows": {"stat": "nutrients",
// "land": [...], "sea": [...]}; row = clamp(yield - 1, 0, rows - 1), as SMAC picks its farm
// structures.
struct OccupantYieldRowSprites_t
{
    YieldStat_t stat = YieldStat_t::Nutrients;
    OccupantSpritePaths_t paths;
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
    OccupantSpritePaths_t spritePaths;
    // Neighbor-driven tile set instead of variants; never set together with spritePaths.
    std::optional<OccupantSpriteTiles_t> spriteTiles;
    // Object sprites by yield, drawn instead of spritePaths.
    std::optional<OccupantYieldRowSprites_t> spriteYieldRows;
    // Occupants whose object sprites are not drawn while this one is present (a soil enricher
    // replaces the farm structures).
    std::vector<std::string> hidesSpritesOf;
    // Art drawn in place of the tile's moisture base, keyed by moisture name (Arid, Moist,
    // Wet), one variant per tile (SMAC's farm ground).
    std::unordered_map<std::string, std::vector<std::string>> groundSprites;
    // How far the sprite reaches above the tile, as a fraction of the tile's height. Object
    // sprites (TER1.PCX: 100×62 over a 100×50 footprint) use 0.24; tile textures use 0.
    float spriteOverhangRatio = 0.0f;
    // Feature/improvement ids whose yield StatModifiers are dropped while this improvement
    // is present (Forest suppresses landform; Borehole suppresses most terraform).
    std::vector<std::string> suppressYieldSources;
    // Optional terrain ids that lie dormant on a tile carrying this terrain: kept on the tile
    // but absent to effects, mechanics and rendering until the terrain changes (Ocean keeps
    // Fungus dormant, so raising the sea floor to the shelf wakes it).
    std::vector<std::string> suppressTerrain;
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

// Expand @tag references in excludes, suppress_yield_sources, suppress_terrain and
// hides_sprites_of. A tag no entry declares throws, as does an id no occupant has in
// suppress_terrain, hides_sprites_of or a links tile set. Self-references are skipped. Call on
// the full occupant list, so an improvement may name a tag that only terrain entries carry.
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
