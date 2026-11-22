#include "game/terraform/TerraformRules.h"

#include "game/CurrentGameState.h"
#include "game/research/TechRequirement.h"
#include "game/faction/Faction.h"
#include "game/GameDataContext.h"
#include "game/GameState.h"
#include "game/diplomacy/DiplomacyConfig.h"
#include "game/diplomacy/DiplomacyLedger.h"
#include "game/effects/CoexistenceResolve.h"
#include "game/effects/EffectEnums.h"
#include "game/effects/TileEffectsContext.h"
#include "game/economy/EconomyManager.h"
#include "game/research/ResearchManager.h"
#include "game/base/BaseManager.h"
#include "game/base/BaseTypes.h"
#include "game/map/ImprovementRegistry.h"
#include "game/map/MapUtils.h"
#include "game/map/OccupantCoexistence.h"
#include "game/map/TerrainOperationRegistry.h"
#include "game/map/Tile.h"
#include "game/map/WorldMap.h"
#include "game/terraform/ElevationRules.h"
#include "game/effects/TriggeredEffectDispatch.h"
#include "game/units/Unit.h"
#include "game/units/UnitDomain.h"

#include <algorithm>
#include <optional>
#include <ranges>
#include <span>
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
    return FormerElevationChangeAllowed(rUnit, pChange->bRaise, rRules);
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

bool CanApplyProject_(const Unit& rUnit, const TerraformProject_t& rProject, const Tile& rTile,
                      std::span<const CoexistenceOverrideEffect_t> overrides,
                      const MapRulesConfig_t& rRules)
{
    if (rProject.pPlaces && ImprovementApplies(rTile, *rProject.pPlaces, overrides))
    {
        return true;
    }
    return AnyEffectWouldFire_(rUnit, rProject, rTile, rRules);
}

} // namespace

std::optional<TerraformProject_t> FindTerraformProject(const std::string& rId,
                                                      const ImprovementRegistry& rImprovements,
                                                      const TerrainOperationRegistry& rOperations)
{
    const ImprovementConfig_t* pImprovement = rImprovements.Find(rId);
    if (pImprovement && IsBuildable(*pImprovement))
    {
        TerraformProject_t project;
        project.id = pImprovement->id;
        project.name = pImprovement->name;
        project.project = *pImprovement->project;
        project.domain = pImprovement->domain;
        project.pPlaces = pImprovement;
        return project;
    }
    if (const TerrainOperationConfig_t* pOperation = rOperations.Find(rId))
    {
        TerraformProject_t project;
        project.id = pOperation->id;
        project.name = pOperation->name;
        project.project = pOperation->project;
        project.energyCostSource = pOperation->energyCostSource;
        project.formerDomain = pOperation->formerDomain;
        project.domain = pOperation->domain;
        project.onCompleteEffects = pOperation->onCompleteEffects;
        return project;
    }
    return std::nullopt;
}

int TerraformEnergyCost(const Unit& rUnit, const TerraformProject_t& rProject)
{
    if (rProject.energyCostSource == EnergyCostSource_t::RaiseLowerQuote)
    {
        return QuoteRaiseLowerEnergyCost(rUnit.GetTile(), rUnit.GetFaction().GetFactionId());
    }
    return rProject.project.energyCost;
}

bool CanStartTerraformProject(const Unit& rUnit, const std::string& projectId)
{
    const GameState& rGame = CurrentGameState();
    const std::optional<TerraformProject_t> resolved =
        FindTerraformProject(projectId, rGame.GetTileEffects().GetImprovements(),
                             *rGame.GetGameData().terrainOperationRegistry);
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
