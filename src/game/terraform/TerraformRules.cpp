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
#include <deque>
#include <limits>
#include <optional>
#include <span>
#include <stdexcept>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

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

namespace
{

int ClampElevation_(int elevation, int floorMeters, int ceilingMeters)
{
    return std::max(floorMeters, std::min(elevation, ceilingMeters));
}

// The caller's band intersected with Planet's own. Seeds honor both; pulled neighbors only
// Planet's.
struct ElevationBand_t
{
    int floor = 0;
    int ceiling = 0;
};

ElevationBand_t RequireLegalEdit_(int floorMeters, int ceilingMeters,
                                  const ElevationRulesConfig_t& rRules)
{
    if (rRules.maxAdjacentDifferenceMeters <= 0)
    {
        throw std::logic_error("PlanElevationChange: max adjacent difference must be > 0");
    }
    ElevationBand_t band;
    band.floor = std::max(floorMeters, rRules.minElevationMeters);
    band.ceiling = std::min(ceilingMeters, rRules.maxElevationMeters);
    if (band.floor > band.ceiling)
    {
        throw std::logic_error("PlanElevationChange: the requested band excludes Planet's range");
    }
    return band;
}

// Elevation a neighbor must take to sit within maxDiff of tileElev, or nullopt when it
// already does (including after Planet's own clamp).
std::optional<int> PulledElevation_(int tileElev, int neighborElev, int maxDiff, int floorMeters,
                                    int ceilingMeters)
{
    int desired = neighborElev;
    if (neighborElev > tileElev + maxDiff)
    {
        desired = tileElev + maxDiff;
    }
    else if (neighborElev < tileElev - maxDiff)
    {
        desired = tileElev - maxDiff;
    }
    else
    {
        return std::nullopt;
    }

    desired = ClampElevation_(desired, floorMeters, ceilingMeters);
    if (desired == neighborElev)
    {
        return std::nullopt;
    }
    return desired;
}

// Elevations as the planned writes leave them, with the writes kept in order.
class WorkingElevations_
{
public:
    int Get(const Tile& rTile) const
    {
        const auto it = m_elevations.find(&rTile);
        return it == m_elevations.end() ? rTile.GetElevation() : it->second;
    }

    void Set(const Tile& rTile, int elevation)
    {
        m_elevations[&rTile] = elevation;
        m_writes.push_back(ElevationWrite_t{&rTile, elevation});
    }

    std::vector<ElevationWrite_t> TakeWrites() { return std::move(m_writes); }

private:
    std::unordered_map<const Tile*, int> m_elevations;
    std::vector<ElevationWrite_t> m_writes;
};

void RelaxAdjacentSlopes_(const std::vector<const Tile*>& rSeeds, const WorldMap& rWorldMap,
                          const ElevationRulesConfig_t& rRules, WorkingElevations_& rWorking)
{
    std::deque<const Tile*> pending;
    std::unordered_set<const Tile*> queued;
    for (const Tile* pSeed : rSeeds)
    {
        if (queued.insert(pSeed).second)
        {
            pending.push_back(pSeed);
        }
    }

    while (!pending.empty())
    {
        const Tile* pTile = pending.front();
        pending.pop_front();
        queued.erase(pTile);

        ForEachTileInChebyshevRadius(*pTile, rWorldMap, 1, false,
            [&](const Tile* pNeighbor, int /*distance*/)
            {
                const int tileElev = rWorking.Get(*pTile);
                const int neighborElev = rWorking.Get(*pNeighbor);
                const std::optional<int> desired = PulledElevation_(
                    tileElev, neighborElev, rRules.maxAdjacentDifferenceMeters,
                    rRules.minElevationMeters, rRules.maxElevationMeters);
                if (!desired)
                {
                    return;
                }
                rWorking.Set(*pNeighbor, *desired);
                if (queued.insert(pNeighbor).second)
                {
                    pending.push_back(pNeighbor);
                }
            });
    }
}

} // namespace

std::vector<ElevationWrite_t> PlanElevationChange(std::span<const ElevationSeed_t> seeds,
                                                  int floorMeters, int ceilingMeters,
                                                  const WorldMap& rWorldMap,
                                                  const ElevationRulesConfig_t& rRules)
{
    const ElevationBand_t band = RequireLegalEdit_(floorMeters, ceilingMeters, rRules);
    WorkingElevations_ working;
    std::vector<const Tile*> moved;
    for (const ElevationSeed_t& rSeed : seeds)
    {
        const int current = working.Get(*rSeed.pTile);
        const int next =
            ClampElevation_(current + rSeed.deltaMeters, band.floor, band.ceiling);
        if (next == current)
        {
            continue;
        }
        working.Set(*rSeed.pTile, next);
        moved.push_back(rSeed.pTile);
    }
    if (!moved.empty())
    {
        RelaxAdjacentSlopes_(moved, rWorldMap, rRules, working);
    }
    return working.TakeWrites();
}

int EarthquakeDeltaMeters(std::span<const int> levelRollsMeters,
                          const ElevationRulesConfig_t& rRules)
{
    const int span = rRules.maxElevationMeters - rRules.minElevationMeters;
    int delta = 0;
    for (const int roll : levelRollsMeters)
    {
        delta = roll >= span - delta ? span : delta + roll;
    }
    return delta;
}

} // namespace ac
