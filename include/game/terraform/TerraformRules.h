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
class TerraformProjectRegistry;
class Tile;
class Unit;
struct CoexistenceOverrideEffect_t;
struct TriggeredEffectConfig_t;

// A resolved Former project from the terraform project registry.
struct TerraformProject_t
{
    std::string id;
    std::string name;
    FormerProject_t project;
    FormerDomainRule_t formerDomain = FormerDomainRule_t::MatchesTile;
    // Surface this project may run on. Empty means either surface.
    std::optional<ImprovementDomain_t> domain;
    // Occupant placed on complete, when the project declares `places`.
    const ImprovementConfig_t* pPlaces = nullptr;
    std::span<const std::string> blockedBy;
    // Into registry-owned storage; valid while the registries that produced it are alive.
    std::span<const TriggeredEffectConfig_t> onCompleteEffects;
};

// Resolves a project id against the former-project registry; fills pPlaces from occupants.
std::optional<TerraformProject_t> FindTerraformProject(const std::string& rId,
                                                      const ImprovementRegistry& rImprovements,
                                                      const TerraformProjectRegistry& rProjects);

bool CanStartTerraform(const Unit& rUnit, const TerraformProject_t& rProject);

// Whether rUnit may start projectId now: FindTerraformProject, then CanStartTerraform.
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
