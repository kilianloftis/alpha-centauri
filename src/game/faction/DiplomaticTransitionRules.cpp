#include "game/faction/DiplomaticTransitionRules.h"

#include "game/faction/DiplomacyLedger.h"

namespace ac
{

bool CanProposeStepUp(const DiplomacyLedger& rLedger, FactionId_t a, FactionId_t b)
{
    return rLedger.AreKnown(a, b) && StepUp(rLedger.GetStatus(a, b)).has_value();
}

bool CanDeclareVendetta(const DiplomacyLedger& rLedger, FactionId_t a, FactionId_t b)
{
    if (!rLedger.AreKnown(a, b))
    {
        return false;
    }
    return rLedger.GetStatus(a, b) != DiplomaticStatus_t::Vendetta;
}

bool CanCancelTreaty(const DiplomacyLedger& rLedger, FactionId_t a, FactionId_t b)
{
    if (!rLedger.AreKnown(a, b))
    {
        return false;
    }
    return StepDown(rLedger.GetStatus(a, b)).has_value();
}

bool CanRequestStatus(const DiplomacyLedger& rLedger, FactionId_t a, FactionId_t b,
                      DiplomaticStatus_t requested)
{
    if (requested == DiplomaticStatus_t::Vendetta)
    {
        return CanDeclareVendetta(rLedger, a, b);
    }
    const DiplomaticStatus_t current = rLedger.GetStatus(a, b);
    if (StepUp(current) == requested)
    {
        return CanProposeStepUp(rLedger, a, b);
    }
    if (StepDown(current) == requested)
    {
        return CanCancelTreaty(rLedger, a, b);
    }
    return false;
}

} // namespace ac
