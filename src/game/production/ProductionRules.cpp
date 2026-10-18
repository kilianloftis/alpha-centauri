#include "game/production/ProductionRules.h"

#include "game/effects/ActiveEffect.h"
#include "game/effects/EffectEnums.h"
#include "game/production/ScrapConfig.h"
#include "game/production/ScrapRefundCalculator.h"

#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace ac
{

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

    return std::max(1, static_cast<int>(std::lround(baseCost * multiplier)));
}

} // namespace ac
