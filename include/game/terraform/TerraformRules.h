#pragma once

#include "game/base/BaseTypes.h"
#include "game/map/ImprovementConfigParser.h"
#include "game/map/ElevationRulesConfig.h"
#include "game/map/TerrainConfig.h"
#include "game/units/UnitDomain.h"

#include <optional>
#include <span>
#include <string>
#include <vector>

namespace ac
{

class ImprovementRegistry;
class TerrainOperationRegistry;
class Tile;
class WorldMap;
class Unit;
struct CoexistenceOverrideEffect_t;
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

// These read the elevation rules of the world the tile or unit stands in.
int QuoteRaiseLowerEnergyCost(const Tile& rTile, FactionId_t factionId, const WorldMap& rWorldMap);

bool CanStartTerraform(const Unit& rUnit, const TerraformProject_t& rProject);

int TerraformEnergyCost(const Unit& rUnit, const TerraformProject_t& rProject);

// The improvement can be placed once the improvements it displaces are gone. Terrain is
// never displaced, so anything left after that displacement is a hard refusal.
bool ImprovementApplies(const Tile& rTile, const ImprovementConfig_t& rPlaced,
                        std::span<const CoexistenceOverrideEffect_t> overrides);

// Whether a Former of this domain can still move rTile in this direction. Raising stops at
// Planet's ceiling, and a sea Former may only raise ground still at least a reference level
// deep; lowering stops a level short of Planet's floor, and a land Former stops at the
// reference level so it cannot dig its own tile out from under itself.
bool FormerElevationChangeAllowed(const Tile& rTile, UnitDomain_t formerDomain, bool bRaise,
                                  const ElevationRulesConfig_t& rRules);

// The floor a Former of this domain lowers toward: ocean level on land (the tile stops at the
// waterline), Planet's floor at sea.
int FormerLowerFloorMeters(UnitDomain_t formerDomain, const ElevationRulesConfig_t& rRules);

// SMAC alien_fauna forest/kelp growth: MapAreaTiles / (turn/4 + 32) random tile
// samples per turn. turnIndex is years since GameState::k_FirstPlayableMissionYear
// (0 on the first playable year).
int TerraformSpreadGrowthAttempts(int mapTileCount, int turnIndex);

} // namespace ac
