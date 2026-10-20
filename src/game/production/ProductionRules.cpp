#include "game/production/ProductionRules.h"

#include "game/CurrentGameState.h"
#include "game/DifficultyConfig.h"
#include "game/GameDataContext.h"
#include "game/GameSettings.h"
#include "game/GameState.h"
#include "game/base/BaseManager.h"
#include "game/buildings/BuildingConfig.h"
#include "game/buildings/BuildingManager.h"
#include "game/economy/ResourceManager.h"
#include "game/effects/ActiveEffect.h"
#include "game/effects/EffectEnums.h"
#include "game/effects/TriggeredEffectRules.h"
#include "game/faction/Faction.h"
#include "game/map/MapUtils.h"
#include "game/map/TerritoryMap.h"
#include "game/map/Tile.h"
#include "game/map/WorldMap.h"
#include "game/population/PopulationManager.h"
#include "game/production/ProductionConfigParser.h"
#include "game/production/ProductionManager.h"
#include "game/production/ScrapConfig.h"
#include "game/production/ScrapRefundCalculator.h"
#include "game/production/StockpileConfig.h"
#include "game/production/StockpileRegistry.h"
#include "game/research/ResearchManager.h"
#include "game/units/IDesign.h"
#include "game/units/Military.h"
#include "game/units/UnitDesign.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <memory>
#include <stdexcept>
#include <string>

namespace ac
{

namespace
{

int SizeAfterCompletion_(const IConstructable& rItem, int size)
{
    if (const auto* pBuilding = dynamic_cast<const BuildingConfig_t*>(&rItem))
    {
        return PredictTriggeredPopulationSize(pBuilding->onCompleteEffects, size);
    }
    if (const auto* pDesign = dynamic_cast<const UnitDesign*>(&rItem))
    {
        return PredictUnitProductionPopulationSize(*pDesign, size);
    }
    return size;
}

} // namespace

ScrapPayout_t PlanScrapPayout(const ScrapQuote_t& rQuote, std::optional<BaseId_t> destBaseId)
{
    if (!rQuote.bAvailable)
    {
        throw std::invalid_argument("PlanScrapPayout: quote is unavailable");
    }
    if (!IsScrapRefundStat(rQuote.refundType))
    {
        throw std::invalid_argument("PlanScrapPayout: refund type is not a scrap payout");
    }

    ScrapPayout_t payout;
    payout.refundType = rQuote.refundType;
    if (!ScrapRefundNeedsBase(rQuote.refundType))
    {
        payout.amount = rQuote.amount;
        return payout;
    }

    payout.destBaseId = destBaseId;
    payout.amount = destBaseId ? rQuote.amount : 0;
    return payout;
}

int ComputeProductionCost(int baseCost, const BaseEffects_t& rBaseEffects, int surchargePercent)
{
    // CostMultiplier is PureMultiplier (seed 1.0). Industry rating levels emit AddPercent
    // contributions here via ResolveSocialRatingLevelEffects — same seam as GrowthRate.
    double multiplier = ResolveBaseStat(rBaseEffects, StatId_t::CostMultiplier,
                                        SeedFor(StatId_t::CostMultiplier));

    if (surchargePercent > 0)
    {
        // Scale only the extra surcharge term so MultiplyGeometric 0 (Skunkworks) returns
        // ordinary cost rather than zeroing the whole production bill.
        const double scale = ResolveBaseStat(rBaseEffects, StatId_t::PrototypeSurchargeScale,
                                             SeedFor(StatId_t::PrototypeSurchargeScale));
        multiplier *= (1.0 + static_cast<double>(surchargePercent) / 100.0 * scale);
    }

    const int roundedCost = static_cast<int>(std::lround(baseCost * multiplier));
    return std::max(1, roundedCost);
}

int RetoolForfeit(int mineralStockpile, bool bHasTurnOriginal, bool bReturningToTurnOriginal,
                  const ProductionConfig_t& rConfig, const BaseEffects_t& rBaseEffects)
{
    if (!bHasTurnOriginal || bReturningToTurnOriginal)
    {
        return 0;
    }
    if (mineralStockpile <= rConfig.retoolPenaltyThreshold)
    {
        return 0;
    }

    // Integer division rounds the loss down, so the remainder favours the player. Scale
    // afterward so RetoolPenaltyScale 0 (Skunkworks) cancels the forfeit.
    const int baseForfeit = mineralStockpile * rConfig.retoolPenaltyPercent / 100;
    const double scale = ResolveBaseStat(rBaseEffects, StatId_t::RetoolPenaltyScale,
                                         SeedFor(StatId_t::RetoolPenaltyScale));
    const long forfeited = std::lround(baseForfeit * scale);
    return std::max(0, static_cast<int>(forfeited));
}

int OverflowCarried(int leftover, const ProductionConfig_t& rConfig)
{
    return std::min(leftover, rConfig.retoolPenaltyThreshold);
}

int ItemMineralCost(const BaseManager& rBase, const IConstructable& rItem)
{
    if (rItem.IsStockpile())
    {
        return 0;
    }
    const GameDataContext& rData = CurrentGameState().GetGameData();
    const ProductionConfig_t& rConfig = *rData.productionConfig;
    const auto* pDesign = dynamic_cast<const IDesign*>(&rItem);
    const bool bPrototype = pDesign && IsPrototype(rBase.GetFaction(), *pDesign);
    const int surcharge = bPrototype ? rConfig.prototypeSurchargePercent : 0;
    return ComputeProductionCost(rItem.GetBaseCost(), rBase.GetBaseEffects(), surcharge);
}

bool IsPrototype(const Faction& rFaction, const IDesign& rDesign)
{
    const auto* pUnitDesign = dynamic_cast<const UnitDesign*>(&rDesign);
    if (!pUnitDesign)
    {
        return false;
    }
    const Military& rMilitary = rFaction.GetMilitary();
    for (const UnitComponentConfig_t* pComp : pUnitDesign->GetComponents())
    {
        if (pComp && !rMilitary.HasBuiltComponent(pComp->id))
        {
            return true;
        }
    }
    return false;
}

bool CanBankProduction(const BaseManager& rBase)
{
    return !rBase.IsProductionDisabled();
}

bool CanCompleteProduction(const BaseManager& rBase)
{
    return !rBase.IsProductionDisabled();
}

bool CanHurry(const BaseManager& rBase)
{
    return !rBase.IsProductionDisabled();
}

bool CanScrapBuilding(const BuildingConfig_t& rBuilding)
{
    return !rBuilding.bIsSecretProject;
}

std::vector<const IConstructable*> ConstructableItems(const BaseManager& rBase)
{
    std::vector<const IConstructable*> available;
    const BuildingManager& rBuildings = rBase.GetBuildingManager();
    for (const BuildingConfig_t* pBuilding : rBuildings.GetBuildingsAvailableForConstruction())
    {
        available.push_back(pBuilding);
    }
    const Faction& rFaction = rBase.GetFaction();
    const std::vector<std::string>& rDiscoveredTechs = rFaction.GetResearch().GetDiscoveredTechs();
    for (const StockpileConfig_t& rStockpile : rBase.GetStockpileRegistry().GetAll())
    {
        if (rStockpile.IsAvailable(rDiscoveredTechs))
        {
            available.push_back(&rStockpile);
        }
    }
    for (const std::unique_ptr<IDesign>& pDesign : rFaction.GetMilitary().GetDesigns())
    {
        if (const auto* pItem = dynamic_cast<const IConstructable*>(pDesign.get()))
        {
            available.push_back(pItem);
        }
    }
    return available;
}

int CompletionPopulationFloor()
{
    const GameState& rGameState = CurrentGameState();
    const GameDataContext& rData = rGameState.GetGameData();
    if (!rData.difficultyConfig)
    {
        throw std::runtime_error("CompletionPopulationFloor requires a difficultyConfig");
    }
    const std::string& rDifficultyId = rGameState.GetSettings().GetGameRules().difficultyId;
    const DifficultyRules_t& rRules =
        rData.difficultyConfig->RequireForSession(rDifficultyId).rules;
    return rRules.colonyPodPreservesSize1Base ? 1 : 0;
}

bool CompletionAbandonsBase(const BaseManager& rBase)
{
    const IConstructable* pItem = rBase.GetProduction().GetCurrentProduction();
    if (!pItem)
    {
        return false;
    }
    const int sizeAfter = SizeAfterCompletion_(*pItem, rBase.GetPopulation().GetSize());
    // Only an item that would empty the base depends on the difficulty.
    return sizeAfter <= 0 && CompletionPopulationFloor() == 0;
}

std::optional<BaseId_t> ScrapRefundBase(const Faction& rFaction, const Tile& rTile)
{
    const WorldMap& rWorldMap = rFaction.GetWorldMap();
    if (rWorldMap.GetTerritory().GetOwner(rTile) != rFaction.GetFactionId())
    {
        return std::nullopt;
    }

    std::optional<BaseId_t> bestId;
    int bestDist = std::numeric_limits<int>::max();
    const int mapWidth = rWorldMap.GetWidth();
    for (const BaseManager& rBase : rFaction.Bases())
    {
        const int dist = TabletopDiagonalDistance(rTile, rBase.GetTile(), mapWidth);
        const BaseId_t id = rBase.GetBaseId();
        if (!bestId || dist < bestDist || (dist == bestDist && id < *bestId))
        {
            bestDist = dist;
            bestId = id;
        }
    }
    return bestId;
}

} // namespace ac
