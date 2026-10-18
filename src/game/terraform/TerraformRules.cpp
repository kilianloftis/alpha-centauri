#include "game/terraform/TerraformRules.h"

#include "game/CurrentGameState.h"
#include "game/faction/Faction.h"
#include "game/GameState.h"
#include "game/effects/CoexistenceResolve.h"
#include "game/effects/EffectEnums.h"
#include "game/economy/EconomyManager.h"
#include "game/research/ResearchManager.h"
#include "game/base/BaseTypes.h"
#include "game/map/ElevationRulesConfig.h"
#include "game/map/ImprovementIds.h"
#include "game/map/ImprovementRegistry.h"
#include "game/effects/TriggeredEffectDispatch.h"
#include "game/map/MapUtils.h"
#include "game/map/TerrainOperationRegistry.h"
#include "game/map/TerritoryMap.h"
#include "game/map/OccupantCoexistence.h"
#include "game/map/Tile.h"
#include "game/map/WorldMap.h"
#include "game/units/Unit.h"
#include "game/units/UnitDomain.h"

#include <algorithm>
#include <cmath>
#include <span>
#include <limits>
#include <stdexcept>

namespace ac
{

bool ImprovementApplies(const Tile& rTile, const ImprovementConfig_t& rPlaced,
                        std::span<const CoexistenceOverrideEffect_t> overrides)
{
    if (rTile.HasImprovement(rPlaced.id))
    {
        return false;
    }
    const std::vector<std::string> displaced =
        ImprovementsDisplacedBy(rTile, rPlaced, overrides);
    return !OccupantsBlockPlacement(rTile, rPlaced, displaced, overrides);
}

namespace
{

bool HasRequiredTech_(const Faction& rFaction, const std::string& rRequiredTech)
{
    if (rRequiredTech.empty())
    {
        return true;
    }
    return rFaction.GetResearch().HasDiscoveredTech(rRequiredTech);
}

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
                             const ElevationRulesConfig_t& rRules)
{
    const auto* pChange = std::get_if<ElevationChangeEffect_t>(&rEffect.effect);
    if (!pChange)
    {
        return true;
    }
    return FormerElevationChangeAllowed(rUnit.GetTile(), rUnit.GetDomain(), pChange->bRaise,
                                        rRules);
}

// A project is refused before it is paid for when nothing it declares would happen. Each
// effect's own `condition` answers that for terrain writes; an elevation change also has a
// bound that depends on the Former's domain, so it is asked directly.
bool AnyEffectWouldFire_(const Unit& rUnit, const TerraformProject_t& rProject,
                         const Tile& rTile, const ElevationRulesConfig_t& rRules)
{
    EffectContext_t context;
    context.targetTile = &rTile;
    context.pUnit = &rUnit;
    context.pFaction = &rUnit.GetFaction();
    for (const TriggeredEffectConfig_t& rEffect : rProject.onCompleteEffects)
    {
        if (rEffect.condition && !ConditionSatisfied(*rEffect.condition, context))
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
    if (rProject.formerDomain == FormerDomainRule_t::Any)
    {
        const UnitDomain_t domain = rUnit.GetDomain();
        return domain == UnitDomain_t::Sea || domain == UnitDomain_t::Land;
    }
    if (rProject.pPlaces)
    {
        if (rProject.pPlaces->domain == ImprovementDomain_t::Sea)
        {
            return rTile.IsWater() && rUnit.GetDomain() == UnitDomain_t::Sea;
        }
        if (rProject.pPlaces->domain == ImprovementDomain_t::Land)
        {
            return rTile.IsLand() && rUnit.GetDomain() == UnitDomain_t::Land;
        }
    }
    return FormerMatchesTile_(rUnit, rTile);
}

bool CanApplyProject_(const Unit& rUnit, const TerraformProject_t& rProject, const Tile& rTile,
                      std::span<const CoexistenceOverrideEffect_t> overrides,
                      const ElevationRulesConfig_t& rRules)
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
        project.onCompleteEffects = pOperation->onCompleteEffects;
        return project;
    }
    return std::nullopt;
}

int QuoteRaiseLowerEnergyCost(const Tile& rTile, FactionId_t factionId, const WorldMap& rWorldMap)
{
    const ElevationRulesConfig_t& rRules = rTile.MapRules();
    if (rRules.referenceLevelMeters <= 0)
    {
        throw std::logic_error("QuoteRaiseLowerEnergyCost: reference_level_meters must be > 0");
    }
    const int elevBand = std::abs(rTile.GetElevation()) / rRules.referenceLevelMeters + 1;
    int nearest = std::numeric_limits<int>::max();
    const int mapWidth = rWorldMap.GetWidth();

    for (const auto& pTile : rWorldMap.GetTiles())
    {
        if (!pTile || !pTile->HasImprovement(ImprovementIds::k_Base))
        {
            continue;
        }
        if (rWorldMap.GetTerritory().GetOwner(*pTile) == factionId)
        {
            nearest = std::min(nearest, ChebyshevDistance(rTile, *pTile, mapWidth));
        }
    }
    if (nearest == std::numeric_limits<int>::max())
    {
        nearest = std::max(rWorldMap.GetWidth(), rWorldMap.GetHeight());
    }
    return elevBand * 8 + nearest * 2;
}

int TerraformEnergyCost(const Unit& rUnit, const TerraformProject_t& rProject)
{
    const GameState& rGameState = CurrentGameState();
    if (rProject.energyCostSource == EnergyCostSource_t::RaiseLowerQuote)
    {
        return QuoteRaiseLowerEnergyCost(rUnit.GetTile(), rUnit.GetFaction().GetFactionId(),
                                         rGameState.GetWorldMap());
    }
    return rProject.project.energyCost;
}

bool CanStartTerraform(const Unit& rUnit, const TerraformProject_t& rProject)
{
    if (!rUnit.GetFlag(RuleFlagId_t::Terraform))
    {
        return false;
    }
    if (rProject.project.turnsRequired <= 0)
    {
        return false;
    }
    if (!HasRequiredTech_(rUnit.GetFaction(), rProject.project.requiredTech))
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

bool FormerElevationChangeAllowed(const Tile& rTile, UnitDomain_t formerDomain, bool bRaise,
                                  const ElevationRulesConfig_t& rRules)
{
    const bool bSeaFormer = formerDomain == UnitDomain_t::Sea;
    const int elevation = rTile.GetElevation();
    if (bRaise)
    {
        return bSeaFormer ? elevation <= -rRules.referenceLevelMeters
                          : elevation < rRules.maxElevationMeters;
    }
    // TODO: SMAC's floor for sea-former lowering is unknown; Planet's own floor is the only
    // limit we can state.
    return bSeaFormer ? elevation - rRules.referenceLevelMeters >= rRules.minElevationMeters
                      : elevation >= rRules.referenceLevelMeters;
}

int FormerLowerFloorMeters(UnitDomain_t formerDomain, const ElevationRulesConfig_t& rRules)
{
    return formerDomain == UnitDomain_t::Land ? rRules.oceanLevelMeters : rRules.minElevationMeters;
}

int TerraformSpreadGrowthAttempts(int mapTileCount, int turnIndex)
{
    if (mapTileCount <= 0)
    {
        return 0;
    }
    const int turn = std::max(0, turnIndex);
    return mapTileCount / (turn / 4 + 32);
}

} // namespace ac
