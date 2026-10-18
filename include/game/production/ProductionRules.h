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

// A priced scrap together with the sink that will receive it. Quoting builds this, and
// crediting consumes it, so the number shown to the player is the number granted: a
// base-destined refund with no base to land in is zeroed here rather than at payout time,
// where the quote could no longer see it.
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
// surchargePercent is production.json's prototype mineral penalty (0 when not a prototype).
// TODO: the surcharge multiplies CostMultiplier rather than adding to it, and the result
// rounds half away from zero while the retool penalty next door documents floor rounding "so
// the remainder favours the player". Both are guesses; the real stacking and rounding rules
// are not recorded anywhere.
int ComputeProductionCost(int baseCost, const BaseEffects_t& rBaseEffects, int surchargePercent);

// The minerals a retool forfeits. Returning to the item queued when the turn handed over is
// free, and so is a retool before anything has been banked.
int RetoolForfeit(int mineralStockpile, bool bHasTurnOriginal, bool bReturningToTurnOriginal,
                  const ProductionConfig_t& rConfig, const BaseEffects_t& rBaseEffects);

// The leftover minerals a completion carries to the next item.
int OverflowCarried(int leftover, const ProductionConfig_t& rConfig);

// 0 for a stockpile; a prototype pays production.json's surcharge.
int ItemMineralCost(const BaseManager& rBase, const IConstructable& rItem,
                    const ProductionConfig_t& rConfig);

// A unit design is a prototype while any of its components has not been fielded by the faction.
// A native design never is.
bool IsPrototype(const Faction& rFaction, const IDesign& rDesign);

// A base with production disabled (a riot) banks nothing, completes nothing and cannot be hurried.
bool CanBankProduction(const BaseManager& rBase);
bool CanCompleteProduction(const BaseManager& rBase);
bool CanHurry(const BaseManager& rBase);

bool CanScrapBuilding(const BuildingConfig_t& rBuilding);

std::vector<const IConstructable*> ConstructableItems(const BaseManager& rBase);

// The smallest size a completed item's pop cost may leave: 1 at a difficulty that preserves a
// size-1 base (Citizen), otherwise 0.
int CompletionPopulationFloor();

// The item's pop cost would empty the base at its current size. Growth due this turn does not
// count, as SMAC's prompt does not.
bool CompletionAbandonsBase(const BaseManager& rBase);

// The closest base by TabletopDiagonalDistance, then the lower BaseId. Empty outside the
// faction's territory. TODO: SMAC's nearest-base metric for unit disband is not recorded.
std::optional<BaseId_t> ScrapRefundBase(const Faction& rFaction, const Tile& rTile);

} // namespace ac
