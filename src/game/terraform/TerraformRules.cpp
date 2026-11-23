#include "game/terraform/TerraformRules.h"

#include "game/CurrentGameState.h"
#include "game/research/TechRequirement.h"
#include "game/faction/Faction.h"
#include "game/GameDataContext.h"
#include "game/GameState.h"
#include "game/diplomacy/DiplomacyConfig.h"
#include "game/diplomacy/DiplomacyLedger.h"
#include "game/diplomacy/DiplomaticStatus.h"
#include "game/effects/ActiveEffect.h"
#include "game/effects/CoexistenceResolve.h"
#include "game/effects/EffectEnums.h"
#include "game/effects/TileEffectsContext.h"
#include "game/economy/EconomyManager.h"
#include "game/research/ResearchManager.h"
#include "game/base/BaseManager.h"
#include "game/base/BaseTypes.h"
#include "game/map/ImprovementIds.h"
#include "game/map/ImprovementRegistry.h"
#include "game/map/MapUtils.h"
#include "game/map/OccupantCoexistence.h"
#include "game/map/TerraformProjectRegistry.h"
#include "game/map/Tile.h"
#include "game/map/WorldMap.h"
#include "game/population/PopulationManager.h"
#include "game/terraform/ElevationRules.h"
#include "game/terraform/FormerEnergyCostCalculator.h"
#include "game/effects/TriggeredEffectDispatch.h"
#include "game/units/Unit.h"
#include "game/units/UnitDomain.h"

#include <algorithm>
#include <limits>
#include <optional>
#include <ranges>
#include <span>
#include <variant>
#include <vector>

namespace ac
{

namespace
{

bool FormerMatchesTile_(const Unit& rUnit, const Tile& rTile)
{
    const bool bSea = rTile.IsWater();
    const UnitDomain_t domain = rUnit.GetDomain();
    if (bSea)
    {
        return domain == UnitDomain_t::Sea;
    }
    return domain == UnitDomain_t::Land;
}

bool ElevationEffectAllowed_(const Unit& rUnit, const TriggeredEffectConfig_t& rEffect,
                             const MapRulesConfig_t& rRules)
{
    const auto* pChange = std::get_if<ElevationChangeEffect_t>(&rEffect.effect);
    if (!pChange)
    {
        return true;
    }
    return pChange->bRaise ? FormerRaiseAllowed(rUnit, rRules)
                           : FormerLowerAllowed(rUnit, rRules);
}

// Refused before payment when nothing would happen. Effect conditions answer for terrain
// writes; an elevation change's bound depends on the Former's domain.
bool AnyEffectWouldFire_(const Unit& rUnit, const TerraformProject_t& rProject,
                         const Tile& rTile, const MapRulesConfig_t& rRules)
{
    EffectContext_t context;
    context.targetTile = &rTile;
    context.pUnit = &rUnit;
    context.pFaction = &rUnit.GetFaction();
    for (const TriggeredEffectConfig_t& rEffect : rProject.onCompleteEffects)
    {
        if (rEffect.condition && !ConditionSatisfied(*rEffect.condition, context, nullptr))
        {
            continue;
        }
        if (ElevationEffectAllowed_(rUnit, rEffect, rRules))
        {
            return true;
        }
    }
    return false;
}

bool DomainAllows_(const Unit& rUnit, const TerraformProject_t& rProject, const Tile& rTile)
{
    if (rProject.domain.has_value() && !TileMatchesDomain(rTile, *rProject.domain))
    {
        return false;
    }
    if (rProject.formerDomain == FormerDomainRule_t::Any)
    {
        const UnitDomain_t domain = rUnit.GetDomain();
        return domain == UnitDomain_t::Sea || domain == UnitDomain_t::Land;
    }
    return FormerMatchesTile_(rUnit, rTile);
}

bool BlockedByOccupant_(const TerraformProject_t& rProject, const Tile& rTile)
{
    for (const std::string& rId : rProject.blockedBy)
    {
        if (rTile.HasFeature(rId))
        {
            return true;
        }
    }
    return false;
}

// docs/thinker/terranx-map.md "Starting a project": fungus needs CentEmp for roads /
// MagTube and CentPsi for other place-projects.
bool FungusBlocksPlaceProject_(const Unit& rUnit, const ImprovementConfig_t& rPlaces,
                               const Tile& rTile)
{
    if (!rTile.HasFeature(ImprovementIds::k_Fungus))
    {
        return false;
    }
    // TODO: Try to remove direct reference to road and magtube
    const bool bRoadOrTube =
        rPlaces.id == ImprovementIds::k_Road || rPlaces.id == ImprovementIds::k_MagTube;
    const RuleFlagId_t flag = bRoadOrTube ? RuleFlagId_t::TerraformFungusRoad
                                          : RuleFlagId_t::TerraformFungusImprovement;
    return !ResolveFlag(rUnit, flag);
}

bool CanApplyProject_(const Unit& rUnit, const TerraformProject_t& rProject, const Tile& rTile,
                      std::span<const CoexistenceOverrideEffect_t> overrides,
                      const MapRulesConfig_t& rRules)
{
    if (BlockedByOccupant_(rProject, rTile))
    {
        return false;
    }
    if (rProject.pPlaces)
    {
        if (FungusBlocksPlaceProject_(rUnit, *rProject.pPlaces, rTile))
        {
            return false;
        }
        if (ImprovementApplies(rTile, *rProject.pPlaces, overrides))
        {
            return true;
        }
        if (rProject.onCompleteEffects.empty())
        {
            return false;
        }
    }
    return AnyEffectWouldFire_(rUnit, rProject, rTile, rRules);
}

} // namespace

std::optional<TerraformProject_t> FindTerraformProject(const std::string& rId,
                                                      const ImprovementRegistry& rImprovements,
                                                      const TerraformProjectRegistry& rProjects)
{
    const TerraformProjectConfig_t* pConfig = rProjects.Find(rId);
    if (!pConfig)
    {
        return std::nullopt;
    }
    TerraformProject_t project;
    project.id = pConfig->id;
    project.name = pConfig->name;
    project.project = pConfig->project;
    project.formerDomain = pConfig->formerDomain;
    project.domain = pConfig->domain;
    project.blockedBy = pConfig->blockedBy;
    project.onCompleteEffects = pConfig->onCompleteEffects;
    if (pConfig->places)
    {
        project.pPlaces = &rImprovements.Get(*pConfig->places);
    }
    return project;
}

namespace
{

const BaseManager* NearestEnemyBase_(const Tile& rTile, FactionId_t factionId)
{
    const GameState& rGame = CurrentGameState();
    const int mapWidth = rGame.GetWorldMap().GetWidth();
    const BaseManager* pNearest = nullptr;
    int nearestDist = std::numeric_limits<int>::max();

    for (const Faction& rFaction : rGame.Factions())
    {
        if (rFaction.GetFactionId() == factionId)
        {
            continue;
        }
        for (const BaseManager& rBase : rFaction.Bases())
        {
            const int dist = ChebyshevDistance(rTile, rBase.GetTile(), mapWidth);
            if (dist < nearestDist)
            {
                nearestDist = dist;
                pNearest = &rBase;
            }
        }
    }
    return pNearest;
}

void FillEnemyScaleInputs_(FormerEnergyCostInputs_t& rInputs, const Tile& rTile,
                           FactionId_t factionId)
{
    const Faction& rFaction = CurrentGameState().RequireFaction(factionId);
    const BaseManager* pOwn = rFaction.FindNearestBase(rTile);
    if (!pOwn)
    {
        return;
    }
    const BaseManager* pEnemy = NearestEnemyBase_(rTile, factionId);
    if (!pEnemy)
    {
        return;
    }
    const DiplomaticStatus_t status = CurrentGameState().GetDiplomacyLedger().GetStatus(
        factionId, pEnemy->GetFaction().GetFactionId());
    if (status == DiplomaticStatus_t::Pact)
    {
        return;
    }

    const int mapWidth = CurrentGameState().GetWorldMap().GetWidth();
    rInputs.ownPop = pOwn->GetPopulation().GetSize();
    rInputs.enemyPop = pEnemy->GetPopulation().GetSize();
    rInputs.ownRange = ChebyshevDistance(rTile, pOwn->GetTile(), mapWidth);
    rInputs.enemyRange = ChebyshevDistance(rTile, pEnemy->GetTile(), mapWidth);
}

FormerEnergyCostInputs_t RaiseLowerEnergyInputs_(const Unit& rUnit)
{
    const Tile& rTile = rUnit.GetTile();
    const FactionId_t factionId = rUnit.GetFaction().GetFactionId();
    FormerEnergyCostInputs_t inputs;
    inputs.elevBand = RaiseLowerElevBand(rTile);
    inputs.bFungus = rTile.HasFeature(ImprovementIds::k_Fungus);
    inputs.bAtSea = rTile.IsWater();
    inputs.seaCostScale = 1.0;
    if (rTile.IsWater())
    {
        // SeedFor (1); terraform_cost.lua multiplies by sea_cost_seed. Doc Air MaxClamp 0.5.
        inputs.seaCostScale = ResolveFactionStat(
            rUnit.GetFaction().GetActiveEffects(), StatId_t::SeaRaiseLowerCostScale,
            SeedFor(StatId_t::SeaRaiseLowerCostScale), nullptr);
    }
    inputs.nearestBaseDistance = NearestOwnedBaseDistance(rTile, factionId);
    FillEnemyScaleInputs_(inputs, rTile, factionId);
    return inputs;
}

} // namespace

int TerraformEnergyCost(const Unit& rUnit, const TerraformProject_t& rProject)
{
    const FormerEnergyCost_t& rCost = rProject.project.energyCost;
    if (!rCost.formula.has_value())
    {
        return rCost.flat;
    }
    return CurrentGameState().GetGameData().formerEnergyCostCalculator->Cost(
        rCost, RaiseLowerEnergyInputs_(rUnit));
}

bool CanStartTerraformProject(const Unit& rUnit, const std::string& projectId)
{
    const GameState& rGame = CurrentGameState();
    const std::optional<TerraformProject_t> resolved =
        FindTerraformProject(projectId, rGame.GetTileEffects().GetImprovements(),
                             *rGame.GetGameData().terraformProjectRegistry);
    return resolved && CanStartTerraform(rUnit, *resolved);
}

bool CanStartTerraform(const Unit& rUnit, const TerraformProject_t& rProject)
{
    if (!ResolveFlag(rUnit, RuleFlagId_t::Terraform))
    {
        return false;
    }
    if (rProject.project.turnsRequired <= 0)
    {
        return false;
    }
    if (!MeetsTechRequirement(rProject.project.requiredTechs,
                              rUnit.GetFaction().GetResearch().GetDiscoveredTechs()))
    {
        return false;
    }
    if (!DomainAllows_(rUnit, rProject, rUnit.GetTile()))
    {
        return false;
    }

    const std::vector<CoexistenceOverrideEffect_t> overrides = ActiveCoexistenceOverrides(rUnit);
    if (!CanApplyProject_(rUnit, rProject, rUnit.GetTile(), overrides,
                          rUnit.GetTile().MapRules()))
    {
        return false;
    }

    const int cost = TerraformEnergyCost(rUnit, rProject);
    return rUnit.GetFaction().GetEconomy().CanAfford(cost);
}

std::optional<FactionId_t> RainfallGrievant(const Faction& rTerraformer,
                                            std::span<const MoistureChange_t> changes)
{
    if (!rTerraformer.IsPlayerControlled())
    {
        return std::nullopt;
    }
    const GameState& rGame = CurrentGameState();
    const int stopDistance = rGame.GetGameData().diplomacyConfig->terraformRainfallStopDistance;
    const int mapWidth = rGame.GetWorldMap().GetWidth();
    const auto nearTerraformerBase = [&](const Tile& rTile) {
        return std::ranges::any_of(rTerraformer.Bases(), [&](const BaseManager& rBase) {
            return TabletopDiagonalDistance(rTile, rBase.GetTile(), mapWidth) <= stopDistance;
        });
    };

    std::optional<FactionId_t> recorded;
    for (const MoistureChange_t& rChange : changes)
    {
        const bool bDrier = rChange.pTile->GetBaseMoisture() < rChange.before;
        if (bDrier
            && (!recorded || !rGame.RequireFaction(*recorded).IsPlayerControlled()))
        {
            if (const BaseManager* pNearest = rGame.FindNearestBase(*rChange.pTile))
            {
                recorded = pNearest->GetFactionId();
            }
        }
        if (nearTerraformerBase(*rChange.pTile))
        {
            break;
        }
    }

    // TODO: a grievant that wants revenge is marked to betray the terraformer instead;
    // there is no revenge record yet.
    if (!recorded || rGame.RequireFaction(*recorded).IsPlayerControlled())
    {
        return std::nullopt;
    }
    const DiplomaticStatus_t status =
        rGame.GetDiplomacyLedger().GetStatus(rTerraformer.GetFactionId(), *recorded);
    if (status == DiplomaticStatus_t::Pact || status == DiplomaticStatus_t::Vendetta)
    {
        return std::nullopt;
    }
    return recorded;
}

} // namespace ac
