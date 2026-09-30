#include "game/units/TerraformRules.h"

#include "game/Faction.h"
#include "game/GameState.h"
#include "game/effects/CoexistenceResolve.h"
#include "game/effects/EffectEnums.h"
#include "game/effects/TileEffectsContext.h"
#include "game/faction/EconomyManager.h"
#include "game/faction/ResearchManager.h"
#include "game/faction/base/BaseTypes.h"
#include "game/map/ElevationChange.h"
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

// The improvement can be placed once the improvements it displaces are gone. Terrain is
// never displaced, so anything left after that displacement is a hard refusal.
bool ImprovementApplies_(const Tile& rTile, const ImprovementConfig_t& rPlaced,
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

bool CanApplyProject_(const Unit& rUnit, const TerraformProject_t& rProject, const Tile& rTile,
                      std::span<const CoexistenceOverrideEffect_t> overrides,
                      const ElevationRulesConfig_t& rRules)
{
    if (rProject.pPlaces && ImprovementApplies_(rTile, *rProject.pPlaces, overrides))
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

int TerraformEnergyCost(const Unit& rUnit, const TerraformProject_t& rProject,
                        const GameState& rGameState)
{
    if (rProject.energyCostSource == EnergyCostSource_t::RaiseLowerQuote)
    {
        return QuoteRaiseLowerEnergyCost(rUnit.GetTile(), rUnit.GetFaction().GetFactionId(),
                                         rGameState.GetWorldMap());
    }
    return rProject.project.energyCost;
}

bool CanStartTerraform(const Unit& rUnit, const TerraformProject_t& rProject,
                       const GameState& rGameState)
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

    const int cost = TerraformEnergyCost(rUnit, rProject, rGameState);
    return rUnit.GetFaction().GetEconomy().CanAfford(cost);
}

namespace
{

// Nothing is removed until the placement is known to succeed: a project that has already
// been paid for must not leave the tile stripped and empty when terrain shifted mid-order.
bool PlaceImprovement_(Tile& rTile, const ImprovementConfig_t& rPlaced,
                       TileEffectsContext& rTileEffects,
                       std::span<const CoexistenceOverrideEffect_t> overrides)
{
    if (rTile.HasImprovement(rPlaced.id))
    {
        return true;
    }
    if (!ImprovementApplies_(rTile, rPlaced, overrides))
    {
        return false;
    }

    for (const auto& [rImprovementId, rFeatureId] : WaivedPairsFor(rTile, rPlaced, overrides))
    {
        rTile.AddCoexistenceWaiver(rImprovementId, rFeatureId);
    }
    for (const std::string& rId : ImprovementsDisplacedBy(rTile, rPlaced, overrides))
    {
        rTileEffects.RemoveOccupantWithEffects(rTile, rId);
    }
    rTileEffects.AddOccupantWithEffects(rTile, rPlaced.id);
    return rTile.HasImprovement(rPlaced.id);
}

// The project's own effects, fired against the Former's tile. A project that declares any
// needs a session: the effects mutate world state through GameState, which a movement-only
// harness does not have.
bool RunCompleteEffects_(Tile& rTile, const TerraformProject_t& rProject, Unit& rFormer,
                         std::mt19937& rRng)
{
    GameState* pGameState = rFormer.GetFaction().GetGameState();
    if (!pGameState)
    {
        throw std::runtime_error("Terraform project '" + rProject.id
                                 + "' declares on_complete_effects but the former's faction is "
                                   "not attached to a session");
    }
    TriggeredEffectContext_t context(*pGameState, rFormer.GetFaction());
    context.pUnit = &rFormer;
    context.pTile = &rTile;
    context.pRng = &rRng;
    return !ApplyTriggeredEffects(rProject.onCompleteEffects, context).empty();
}

} // namespace

bool ApplyTerraformResult(Tile& rTile, const TerraformProject_t& rProject,
                          TileEffectsContext& rTileEffects, Unit& rFormer, std::mt19937& rRng)
{
    bool bDidAnything = false;
    if (rProject.pPlaces)
    {
        const std::vector<CoexistenceOverrideEffect_t> overrides =
            ActiveCoexistenceOverrides(rFormer);
        bDidAnything = PlaceImprovement_(rTile, *rProject.pPlaces, rTileEffects, overrides);
    }
    if (!rProject.onCompleteEffects.empty())
    {
        bDidAnything = RunCompleteEffects_(rTile, rProject, rFormer, rRng) || bDidAnything;
    }
    return bDidAnything;
}

} // namespace ac
