#pragma once

#include "game/Faction.h"
#include "game/faction/base/BaseManager.h"
#include "game/faction/base/production/ScrapConfig.h"
#include "game/faction/base/production/ScrapPayout.h"

#include <stdexcept>
#include <string>

namespace ac
{

inline const char* ScrapRefundLabel(StatId_t refundType)
{
    switch (refundType)
    {
        case StatId_t::EnergyCredits:
            return "energy credits";
        case StatId_t::Minerals:
            return "minerals";
        case StatId_t::Nutrients:
            return "nutrients";
        case StatId_t::Energy:
            return "energy";
        case StatId_t::Econ:
            return "econ";
        case StatId_t::Labs:
            return "labs";
        case StatId_t::Psych:
            return "psych";
        default:
            throw std::logic_error("ScrapRefundLabel: refund type is not a scrap payout");
    }
}

inline std::string FormatScrapConfirm(const ScrapPayout_t& rPayout, const Faction& rFaction)
{
    std::string message = "Refund " + std::to_string(rPayout.amount) + " "
                          + ScrapRefundLabel(rPayout.refundType);
    if (rPayout.destBaseId)
    {
        const BaseManager* pDest = rFaction.FindBase(*rPayout.destBaseId);
        message += " to ";
        message += pDest ? pDest->GetName() : ("base " + std::to_string(*rPayout.destBaseId));
    }
    else if (ScrapRefundNeedsBase(rPayout.refundType))
    {
        message += " to each holding base";
    }
    message += "?";
    return message;
}

} // namespace ac
