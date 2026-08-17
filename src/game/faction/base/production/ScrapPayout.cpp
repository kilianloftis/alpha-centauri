#include "game/faction/base/production/ScrapPayout.h"

#include "game/Faction.h"
#include "game/faction/EconomyManager.h"
#include "game/faction/base/production/ProductionManager.h"
#include "game/faction/base/production/ScrapConfig.h"
#include "game/faction/base/production/ScrapRefundCalculator.h"
#include "game/faction/base/resources/ResourceManager.h"

#include <stdexcept>
#include <string>

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

int CreditScrapRefund(const ScrapPayout_t& rPayout, Faction& rFaction)
{
    if (rPayout.amount <= 0)
    {
        return 0;
    }
    if (!ScrapRefundNeedsBase(rPayout.refundType))
    {
        rFaction.GetEconomy().AddEnergy(rPayout.amount);
        return rPayout.amount;
    }

    BaseManager* pDest = rPayout.destBaseId ? rFaction.FindBase(*rPayout.destBaseId) : nullptr;
    if (!pDest)
    {
        throw std::runtime_error("CreditScrapRefund: destination base "
                                 + std::to_string(rPayout.destBaseId.value_or(-1))
                                 + " is not held by this faction");
    }

    switch (rPayout.refundType)
    {
        case StatId_t::Minerals:
        {
            ProductionManager& rProduction = pDest->GetProduction();
            rProduction.SetMineralStockpile(rProduction.GetMineralStockpile() + rPayout.amount);
            break;
        }
        case StatId_t::Energy:
            pDest->GetResources().AddAllocatedEnergy(rPayout.amount);
            break;
        default:
            pDest->GetResources().AddResource(rPayout.refundType, rPayout.amount);
            break;
    }
    return rPayout.amount;
}

} // namespace ac
