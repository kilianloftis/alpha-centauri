#pragma once

#include "game/faction/base/BaseTypes.h"
#include "game/map/ImprovementConfigParser.h"
#include <string>

namespace ac
{

class Tile;
class WorldMap;
class TileEffectsContext;
class Unit;
class GameState;

// Energy spent when starting Raise/Lower Land. Uses elevation band + distance to the
// nearest owned base (Chebyshev). Config energy_cost is unused for these results.
int QuoteRaiseLowerEnergyCost(const Tile& rTile, FactionId_t factionId, const WorldMap& rWorldMap);

// True if the unit may start this Former project on its current tile (flag, tech, domain,
// CanBuildImprovement / mutation preconditions, energy). Does not spend or mutate.
bool CanStartTerraform(const Unit& rUnit, const ImprovementConfig_t& rConfig,
                       const GameState& rGameState);

// Energy that would be charged if the order starts now (raise/lower quote, else config).
int TerraformEnergyCost(const Unit& rUnit, const ImprovementConfig_t& rConfig,
                        const GameState& rGameState);

// Apply a completed terraform: place improvement or mutate tile. Returns false on failure.
bool ApplyTerraformResult(Tile& rTile, const ImprovementConfig_t& rConfig,
                          TileEffectsContext& rTileEffects, WorldMap& rWorldMap,
                          const Unit& rFormer);

} // namespace ac
