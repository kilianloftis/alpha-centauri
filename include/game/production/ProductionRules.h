#pragma once

#include "game/effects/EffectEnums.h"
#include "game/base/BaseTypes.h"

#include <optional>
#include <vector>

namespace ac
{

class BaseManager;
class Faction;
class IConstructable;
class IDesign;
class Tile;
struct BaseEffects_t;
struct BuildingConfig_t;
struct ProductionConfig_t;
struct ScrapQuote_t;

// A priced scrap with its sink, so the number quoted is the number paid. A refund with no base
// to land in is zeroed here.
struct ScrapPayout_t
{
    int amount = 0;
    StatId_t refundType = StatId_t::EnergyCredits;
    // Base credited for a base-destined refund. Empty for energy credits (the faction
    // treasury takes those) and when no base can receive one — amount is then 0.
    std::optional<BaseId_t> destBaseId;
};

// Pair rQuote with destBaseId, zeroing the amount when the refund needs a base and none was
// supplied. Throws if rQuote is unavailable or its refundType is not a creditable payout.
ScrapPayout_t PlanScrapPayout(const ScrapQuote_t& rQuote, std::optional<BaseId_t> destBaseId);

// baseCost * CostMultiplier * (1 + surchargePercent/100 * PrototypeSurchargeScale), at least 1.
// TODO: the stacking and rounding are guesses; the real rules are unrecorded.
int ComputeProductionCost(int baseCost, const BaseEffects_t& rBaseEffects, int surchargePercent);

// The minerals a retool forfeits. Returning to the item queued when the turn handed over is
// free, and so is a retool before anything has been banked.
int RetoolForfeit(int mineralStockpile, bool bHasTurnOriginal, bool bReturningToTurnOriginal,
                  const ProductionConfig_t& rConfig, const BaseEffects_t& rBaseEffects);

// The leftover minerals a completion carries to the next item.
int OverflowCarried(int leftover, const ProductionConfig_t& rConfig);

// 0 for a stockpile; a prototype pays production.json's surcharge.
int ItemMineralCost(const BaseManager& rBase, const IConstructable& rItem);

// A unit design is a prototype while any of its components has not been fielded by the faction.
// A native design never is.
bool IsPrototype(const Faction& rFaction, const IDesign& rDesign);

// A base with production disabled (a riot) banks nothing, completes nothing and cannot be hurried.
bool CanBankProduction(const BaseManager& rBase);
bool CanCompleteProduction(const BaseManager& rBase);
bool CanHurry(const BaseManager& rBase);

// Secret projects cannot be scrapped. Pressure Dome cannot be scrapped on water.
bool CanScrapBuilding(const BaseManager& rBase, const BuildingConfig_t& rBuilding);

std::vector<const IConstructable*> ConstructableItems(const BaseManager& rBase);

// True when rItem is in ConstructableItems: the player could still queue it at this base.
bool CanProduceItem(const BaseManager& rBase, const IConstructable& rItem);

// The smallest size a completed item's pop cost may leave: 1 at a difficulty that preserves a
// size-1 base (Citizen), otherwise 0.
int CompletionPopulationFloor();

// The item's pop cost would empty the base at its current size. Growth due this turn does not
// count, as SMAC's prompt does not.
bool CompletionAbandonsBase(const BaseManager& rBase);

// Closest own-faction base by TabletopDiagonalDistance, then the lower BaseId. Empty outside
// the faction's territory.
std::optional<BaseId_t> ScrapRefundBase(const Faction& rFaction, const Tile& rTile);

} // namespace ac
