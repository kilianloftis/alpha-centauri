#include "game/economy/EconomyRules.h"

#include "game/GameState.h"
#include "game/base/BaseManager.h"
#include "game/base/HomeBaseIndex.h"
#include "game/economy/EconomyManager.h"
#include "game/economy/Inefficiency.h"
#include "game/economy/ResourceManager.h"
#include "game/economy/WorkerAssignmentManager.h"
#include "game/effects/ActiveEffect.h"
#include "game/effects/EffectEnums.h"
#include "game/effects/TileEffectsContext.h"
#include "game/faction/Faction.h"
#include "game/map/ImprovementIds.h"
#include "game/map/MapUtils.h"
#include "game/map/Tile.h"
#include "game/map/WorldMap.h"
#include "game/units/Unit.h"
#include "game/units/UnitOrder.h"

#include <algorithm>
#include <numeric>
#include <cstddef>
#include <variant>
#include <vector>

namespace ac
{

namespace
{

int ResolveFreeUnitSupport_(const BaseManager& rBase)
{
    const int raw = FinalizeResolvedStat(ResolveBaseStat(
        rBase.GetBaseEffects(), StatId_t::FreeUnitSupport, SeedFor(StatId_t::FreeUnitSupport)));
    return std::max(0, raw);
}

TileResources_t CollectSupplyCrawlYield_(const HomeBaseIndex& rHomeUnits,
                                         const TileEffectsContext& rTileEffects,
                                         const BaseEffects_t& rBaseEffects)
{
    TileResources_t total{0, 0, 0};
    for (const Unit* pUnit : rHomeUnits.GetUnits())
    {
        if (!pUnit || !pUnit->IsSupplyCrawling())
        {
            continue;
        }

        const Tile* pTile = pUnit->GetWorkedTile();
        if (!pTile)
        {
            continue;
        }

        const auto& rOrder = std::get<SupplyCrawlOrder_t>(*pUnit->GetOrder());
        const TileResources_t yield =
            rTileEffects.ResolveTileYield(*pTile, /*bIsBaseTile*/false, rBaseEffects).effective;
        switch (rOrder.resource)
        {
            case StatId_t::Nutrients:
                total.nutrients += yield.nutrients;
                break;
            case StatId_t::Minerals:
                total.minerals += yield.minerals;
                break;
            case StatId_t::Energy:
                total.energy += yield.energy;
                break;
            default:
                break;
        }
    }
    return total;
}

int ModifiedShare_(const BaseEffects_t& rEffects, StatId_t stat, int share)
{
    // ResolveBaseStat (FilterBaseLevelByStatId): base-level resolution must never pick up
    // selector-carrying (per-tile) modifiers.
    const double resolved = ResolveBaseStat(rEffects, stat, static_cast<double>(share));
    return FinalizeResolvedStat(resolved);
}

} // namespace

int EnergyAfterInefficiency(const BaseManager& rBase, int energy)
{
    const BaseManager* pHq = rBase.GetFaction().GetHeadquarters();
    if (pHq == &rBase)
    {
        return energy;
    }

    const int mapWidth = rBase.GetTileEffects().GetWorldMap().GetWidth();
    const int distance = pHq ? TabletopDiagonalDistance(rBase.GetTile(), pHq->GetTile(), mapWidth)
                             : k_DefaultInefficiencyHqDistance;
    const SocialRatingRegistry& rRatings = rBase.GetSocialRatings();
    const int efficiency = rBase.GetEffectiveSocialRating(SocialRatingId_t::Efficiency);
    const int denominator = InefficiencyDenominatorForRating(rRatings, efficiency);
    const int loss = CalculateInefficiencyLoss(energy, distance, denominator);
    return energy - loss;
}

EnergySplit_t SplitEnergy(const EnergyAllocation_t& rAllocation, int energy)
{
    EnergySplit_t split;
    split.labs = energy * rAllocation.labsPercent / 100;
    split.psych = energy * rAllocation.psychPercent / 100;
    split.econ = energy - split.labs - split.psych;
    return split;
}

EnergySplit_t AllocateBaseEnergy(const BaseManager& rBase, int energy)
{
    const int allocatable = EnergyAfterInefficiency(rBase, energy);
    const EnergyAllocation_t allocation = rBase.GetFaction().GetEconomy().GetEnergyAllocation();
    const EnergySplit_t split = SplitEnergy(allocation, allocatable);
    const BaseEffects_t& rEffects = rBase.GetBaseEffects();

    EnergySplit_t allocated;
    allocated.econ = ModifiedShare_(rEffects, StatId_t::Econ, split.econ);
    allocated.labs = ModifiedShare_(rEffects, StatId_t::Labs, split.labs);
    allocated.psych = ModifiedShare_(rEffects, StatId_t::Psych, split.psych);
    return allocated;
}

TileResources_t WorkedYield(const BaseManager& rBase)
{
    const BaseEffects_t& rEffects = rBase.GetBaseEffects();
    const TileEffectsContext& rTileEffects = rBase.GetTileEffects();
    TileResources_t total = rBase.GetWorkerAssignments().ComputeWorkedResources(rEffects);

    const TileResources_t crawl =
        CollectSupplyCrawlYield_(rBase.GetHomeUnits(), rTileEffects, rEffects);
    total.nutrients += crawl.nutrients;
    total.energy    += crawl.energy;
    total.minerals  += crawl.minerals;

    const TileResources_t baseTile =
        rTileEffects.ResolveTileYield(rBase.GetTile(), /*bIsBaseTile*/true, rEffects).effective;
    total.nutrients += baseTile.nutrients;
    total.energy    += baseTile.energy;
    total.minerals  += baseTile.minerals;

    return total;
}

int MineralsForProduction(const BaseManager& rBase)
{
    const int support = MineralSupportCost(rBase);
    return std::max(0, rBase.GetMineralProduction() - support);
}

int UnitSupportCost(const Unit& rUnit)
{
    const bool bNativeOnFungus = rUnit.GetDesign().IsNativeLife()
                                 && rUnit.GetTile().HasFeature(ImprovementIds::k_Fungus);
    if (bNativeOnFungus)
    {
        return 0;
    }
    return rUnit.GetMineralUpkeep();
}

std::vector<int> MineralSupportCharges(const BaseManager& rBase)
{
    const std::vector<Unit*>& rUnits = rBase.GetHomeUnits().GetUnits();
    std::vector<int> charges;
    charges.reserve(rUnits.size());

    int freeLeft = ResolveFreeUnitSupport_(rBase);
    for (const Unit* pUnit : rUnits)
    {
        if (!pUnit)
        {
            charges.push_back(0);
            continue;
        }
        const int cost = UnitSupportCost(*pUnit);
        if (cost <= 0)
        {
            charges.push_back(0);
        }
        else if (freeLeft > 0)
        {
            charges.push_back(0);
            --freeLeft;
        }
        else
        {
            charges.push_back(cost);
        }
    }
    return charges;
}

int MineralSupportCost(const BaseManager& rBase)
{
    const std::vector<int> charges = MineralSupportCharges(rBase);
    return std::accumulate(charges.begin(), charges.end(), 0);
}

Unit* UnitToDisbandForSupport(const BaseManager& rBase)
{
    const std::vector<int> charges = MineralSupportCharges(rBase);
    const int total = std::accumulate(charges.begin(), charges.end(), 0);
    if (total <= rBase.GetResources().GetMineralBank())
    {
        return nullptr;
    }
    const std::vector<Unit*>& rHomeUnits = rBase.GetHomeUnits().GetUnits();
    for (int i = static_cast<int>(rHomeUnits.size()) - 1; i >= 0; --i)
    {
        const std::size_t index = static_cast<std::size_t>(i);
        if (charges[index] > 0)
        {
            return rHomeUnits[index];
        }
    }
    return nullptr;
}

} // namespace ac
