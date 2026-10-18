#include "game/production/ProductionMutations.h"

#include "game/base/BaseManager.h"
#include "game/economy/EconomyManager.h"
#include "game/economy/ResourceManager.h"
#include "game/faction/Faction.h"
#include "game/production/ProductionManager.h"
#include "game/production/ScrapConfig.h"

#include <stdexcept>
#include <string>

namespace ac
{

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
