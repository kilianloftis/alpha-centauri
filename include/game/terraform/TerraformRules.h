#pragma once

#include "game/base/BaseTypes.h"
#include "game/map/ImprovementConfigParser.h"
#include "game/map/TerrainConfig.h"
#include "game/map/Tile.h"

#include <optional>
#include <span>
#include <string>

namespace ac
{

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
    // Surface this project may run on. From the improvement's domain, or an operation's
    // domain; empty means either surface.
    std::optional<ImprovementDomain_t> domain;
    const ImprovementConfig_t* pPlaces = nullptr;
    // Into registry-owned storage; valid while the registries that produced it are alive.
    std::span<const TriggeredEffectConfig_t> onCompleteEffects;
};

// Resolves a project id against buildable improvements first, then terrain operations.
// LoadGameData rejects an id that is both, so the order is not a tie-break.
std::optional<TerraformProject_t> FindTerraformProject(const std::string& rId,
                                                      const ImprovementRegistry& rImprovements,
                                                      const TerrainOperationRegistry& rOperations);

bool CanStartTerraform(const Unit& rUnit, const TerraformProject_t& rProject);

// Whether rUnit may start projectId (a buildable improvement or a terrain operation) on its tile
// now: FindTerraformProject, then CanStartTerraform.
bool CanStartTerraformProject(const Unit& rUnit, const std::string& projectId);

int TerraformEnergyCost(const Unit& rUnit, const TerraformProject_t& rProject);

// The faction a Former's raise or lower land aggrieves by drying land, from the climate rerun's
// changes in scan order. A tile that got drier records the faction holding the nearest base of
// any owner, until a player-controlled one is recorded. The scan stops after the first change
// within terraform_rainfall_stop_distance of one of rTerraformer's bases. nullopt unless the record
// is not player-controlled, rTerraformer is, and the two hold neither Pact nor Vendetta.
std::optional<FactionId_t> RainfallGrievant(const Faction& rTerraformer,
                                            std::span<const MoistureChange_t> changes);

} // namespace ac
