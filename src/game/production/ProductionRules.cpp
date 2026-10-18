#include "game/production/ProductionRules.h"

#include "game/production/ScrapConfig.h"
#include "game/production/ScrapRefundCalculator.h"

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

} // namespace ac
