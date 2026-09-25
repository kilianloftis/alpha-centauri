#pragma once

#include "game/faction/base/BaseTypes.h"
#include "game/map/ElevationRulesConfig.h"
#include "game/map/ImprovementConfigParser.h"

#include <random>
#include <string>
#include <vector>

namespace ac
{

class IUnitOrderWorld;
class ImprovementRegistry;
class Tile;
class WorldMap;
class TileEffectsContext;
class Unit;
class GameState;

// Improvements this order removes when it completes. Place names the improvement the order
// adds (its own config, or placesImprovementId). Other results introduce no feature, so the
// list is empty.
std::vector<std::string> ImprovementsDestroyedByTerraform(const Tile& rTile,
                                                         const ImprovementConfig_t& rOrder,
                                                         const ImprovementRegistry& rImprovements);

// Energy spent when starting Raise/Lower Land. Uses reference_level_meters + distance to the
// nearest owned base (Chebyshev). Config energy_cost is unused for these results.
int QuoteRaiseLowerEnergyCost(const Tile& rTile, FactionId_t factionId, const WorldMap& rWorldMap,
                              const ElevationRulesConfig_t& rRules);

// True if the unit may start this Former project on its current tile (flag, tech, domain,
// mutation preconditions, energy). A Place order is not refused because of a feature it will
// clear when it finishes. Does not spend or mutate.
bool CanStartTerraform(const Unit& rUnit, const ImprovementConfig_t& rConfig,
                       const GameState& rGameState, const ElevationRulesConfig_t& rRules);

// Energy that would be charged if the order starts now (raise/lower quote, else config).
int TerraformEnergyCost(const Unit& rUnit, const ImprovementConfig_t& rConfig,
                        const GameState& rGameState, const ElevationRulesConfig_t& rRules);

// Apply a completed terraform: place improvement or mutate tile. Place removes features that
// cannot share the tile with the improvement it adds, then adds it. Returns false when the
// tile's surface will not take the result.
// Raise and lower roll one level from rRules and relax adjacent slopes. pWorld is forwarded
// to ApplyElevationDelta so a surface flip reconciles occupancy there.
bool ApplyTerraformResult(Tile& rTile, const ImprovementConfig_t& rConfig,
                          TileEffectsContext& rTileEffects, WorldMap& rWorldMap,
                          const Unit& rFormer, std::mt19937& rRng,
                          const ElevationRulesConfig_t& rRules,
                          IUnitOrderWorld* pWorld = nullptr);

} // namespace ac
