#pragma once

#include "game/base/BaseTypes.h"
#include "game/map/ImprovementConfigParser.h"
#include "game/map/ElevationRulesConfig.h"
#include "game/map/TerrainConfig.h"
#include "game/map/Tile.h"
#include "game/units/UnitDomain.h"

#include <optional>
#include <span>
#include <string>
#include <vector>

namespace ac
{

class BaseManager;
class Faction;
class ImprovementRegistry;
class TerrainOperationRegistry;
class Tile;
class Unit;
struct CoexistenceOverrideEffect_t;
struct TriggeredEffectConfig_t;

// A Former project from an improvement (pPlaces) or a terrain operation (onCompleteEffects);
// exactly one is filled.
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
int QuoteRaiseLowerEnergyCost(const Tile& rTile, FactionId_t factionId);

bool CanStartTerraform(const Unit& rUnit, const TerraformProject_t& rProject);

// Whether rUnit may start projectId (a buildable improvement or a terrain operation) on its tile
// now: FindTerraformProject, then CanStartTerraform.
bool CanStartTerraformProject(const Unit& rUnit, const std::string& projectId);

int TerraformEnergyCost(const Unit& rUnit, const TerraformProject_t& rProject);

// The improvement can be placed once the improvements it displaces are gone. Terrain is
// never displaced, so anything left after that displacement is a hard refusal.
bool ImprovementApplies(const Tile& rTile, const ImprovementConfig_t& rPlaced,
                        std::span<const CoexistenceOverrideEffect_t> overrides);

// Whether a Former of this domain can still move rTile this way. A sea Former raises only deep
// ground; a land Former never lowers below the reference level.
bool FormerElevationChangeAllowed(const Tile& rTile, UnitDomain_t formerDomain, bool bRaise,
                                  const ElevationRulesConfig_t& rRules);

// The floor a Former of this domain lowers toward: ocean level on land (the tile stops at the
// waterline), Planet's floor at sea.
int FormerLowerFloorMeters(UnitDomain_t formerDomain, const ElevationRulesConfig_t& rRules);

// SMAC alien_fauna growth: MapAreaTiles / (turn/4 + 32) samples per turn, turnIndex counted
// from the first playable year.
int TerraformSpreadGrowthAttempts(int mapTileCount, int turnIndex);

// A base without a Pressure Dome on a tile that became water floods (terranx alt_set, see
// docs/thinker/terranx-map.md "Sea bases"). The roll is drawn in [rollMin, rollMax].
struct SubmergedBase_t
{
    int size = 0;
    int rollMin = 0;
    int rollMax = 0;
    int lossCap = 0;
    // The faction's last base is kept at one pop rather than destroyed.
    bool bLastBase = false;
};

struct SurfaceFlipConsequences_t
{
    std::vector<std::string> removedImprovements;
    std::optional<SubmergedBase_t> submerged;
    // Units whose domain no longer matches the surface. They die unless a base on the tile is
    // still standing after the flip.
    std::vector<Unit*> strandedUnits;
};

// A flip removes every improvement whose domain is not the new surface (an omitted domain stays),
// except the Base marker of a base on the tile. A base that may occupy water, or whose tile became
// land, is untouched and shelters the units on it; any other base on new water floods.
SurfaceFlipConsequences_t SurfaceFlipConsequences(const Tile& rTile, bool bNowWater,
                                                  const BaseManager* pBase);

struct SubmergenceOutcome_t
{
    int popLoss = 0;
    bool bDestroyed = false;
};

// The base loses clamp(size / 2, roll, cap) pop. Left with none, it is destroyed, unless it is
// the faction's last base, which keeps one.
// TODO: SMAC also keeps an objective base at one pop (is_objective); there are no scenarios yet.
SubmergenceOutcome_t ResolveSubmergence(const SubmergedBase_t& rSubmerged, int roll);

struct TerraformSpread_t
{
    const Tile* pTarget = nullptr;
    const ImprovementConfig_t* pImprovement = nullptr;
};

// Forest spreads from land, KelpFarm from sea, onto the best eligible neighbour (arid and flat
// first). Empty when rOrigin holds neither or no neighbour qualifies.
std::optional<TerraformSpread_t> PickSpreadTarget(const Tile& rOrigin);

struct ElevationSeed_t
{
    const Tile* pTile = nullptr;
    int deltaMeters = 0;
};

struct ElevationWrite_t
{
    const Tile* pTile = nullptr;
    int elevation = 0;
};

// Every write a change makes, in order: seeds move within the band and Planet's range, then each
// moved tile pulls its neighbours within the slope limit. Throws on a band outside Planet's range.
std::vector<ElevationWrite_t> PlanElevationChange(std::span<const ElevationSeed_t> seeds,
                                                  int floorMeters, int ceilingMeters,
                                                  const ElevationRulesConfig_t& rRules);

// The sum of the level rolls, capped at Planet's full elevation span.
int EarthquakeDeltaMeters(std::span<const int> levelRollsMeters,
                          const ElevationRulesConfig_t& rRules);

// The faction a Former's raise or lower land aggrieves by drying land, from the climate rerun's
// changes in scan order. A tile that got drier records the faction holding the nearest base of
// any owner, until a player-controlled one is recorded. The scan stops after the first change
// within terraform_rainfall_stop_distance of one of rTerraformer's bases. nullopt unless the record
// is not player-controlled, rTerraformer is, and the two hold neither Pact nor Vendetta.
std::optional<FactionId_t> RainfallGrievant(const Faction& rTerraformer,
                                            std::span<const MoistureChange_t> changes);

} // namespace ac
