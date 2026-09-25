#pragma once

#include "game/faction/base/BaseTypes.h"
#include "game/map/ElevationRulesConfig.h"
#include "game/map/ImprovementConfigParser.h"
#include "game/map/TerrainConfig.h"

#include <optional>
#include <random>
#include <span>
#include <string>
#include <vector>

namespace ac
{

class IUnitOrderWorld;
class ImprovementRegistry;
class TerrainOperationRegistry;
class Tile;
class WorldMap;
class TileEffectsContext;
class Unit;
class GameState;
struct TriggeredEffectConfig_t;

// A Former project resolved from either a buildable improvement or a terrain operation.
// Exactly one of the two halves is filled: pPlaces is the improvement a buildable improvement
// adds, and onCompleteEffects are the effects an operation runs against the Former's tile.
struct TerraformProject_t
{
    std::string id;
    std::string name;
    FormerProject_t project;
    EnergyCostSource_t energyCostSource = EnergyCostSource_t::Flat;
    FormerDomainRule_t formerDomain = FormerDomainRule_t::MatchesTile;
    const ImprovementConfig_t* pPlaces = nullptr;
    // Into registry-owned storage; valid while the registries that produced it are alive.
    std::span<const TriggeredEffectConfig_t> onCompleteEffects;
};

// Resolves a project id against buildable improvements first, then terrain operations.
// LoadGameData rejects an id that is both, so the order is not a tie-break.
std::optional<TerraformProject_t> FindTerraformProject(const std::string& rId,
                                                      const ImprovementRegistry& rImprovements,
                                                      const TerrainOperationRegistry& rOperations);

int QuoteRaiseLowerEnergyCost(const Tile& rTile, FactionId_t factionId, const WorldMap& rWorldMap,
                              const ElevationRulesConfig_t& rRules);

bool CanStartTerraform(const Unit& rUnit, const TerraformProject_t& rProject,
                       const GameState& rGameState, const ElevationRulesConfig_t& rRules);

int TerraformEnergyCost(const Unit& rUnit, const TerraformProject_t& rProject,
                        const GameState& rGameState, const ElevationRulesConfig_t& rRules);

// Place the project's improvement and fire its on_complete_effects against the Former's tile.
// An improvement refuses before it removes anything when terrain would still conflict, and
// records on the tile any pair the Former's coexistence override waived. Returns whether
// anything actually changed.
bool ApplyTerraformResult(Tile& rTile, const TerraformProject_t& rProject,
                          TileEffectsContext& rTileEffects, Unit& rFormer, std::mt19937& rRng);

} // namespace ac
