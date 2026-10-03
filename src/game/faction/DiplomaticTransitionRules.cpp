#include "game/faction/DiplomaticTransitionRules.h"

#include "game/Faction.h"
#include "game/faction/DiplomacyLedger.h"
#include "game/faction/UnitVisibility.h"
#include "game/units/Unit.h"

namespace ac
{

HostileAct_t HostileActAgainst(const Unit& rActor, const Unit& rTarget)
{
    const FactionId_t aggressor = rActor.GetFaction().GetFactionId();
    const FactionId_t victim = rTarget.GetFaction().GetFactionId();
    return {.aggressor = aggressor,
            .victim = victim,
            .bAttributed = IsOwnerKnownTo(victim, rActor) && IsOwnerKnownTo(aggressor, rTarget)};
}

HostileAct_t HostileActAgainst(const Unit& rActor, FactionId_t victim)
{
    return {.aggressor = rActor.GetFaction().GetFactionId(),
            .victim = victim,
            .bAttributed = IsOwnerKnownTo(victim, rActor)};
}

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

} // namespace ac
