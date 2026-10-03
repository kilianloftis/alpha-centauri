#include "game/faction/FirstContactResolver.h"

#include "game/Faction.h"
#include "game/faction/DiplomacyLedger.h"
#include "game/faction/DiplomaticPermissionRules.h"
#include "game/faction/FactionVisibleMap.h"
#include "game/faction/UnitManager.h"
#include "game/faction/UnitVisibility.h"
#include "game/faction/base/BaseManager.h"
#include "game/units/Unit.h"

namespace ac
{

namespace
{

void MeetIfNeeded_(DiplomacyLedger& rLedger, const Faction& rA, const Faction& rB)
{
    const FactionId_t a = rA.GetFactionId();
    const FactionId_t b = rB.GetFactionId();
    if (a == b || !HasDiplomacy(rA) || !HasDiplomacy(rB) || rLedger.AreKnown(a, b))
    {
        return;
    }
    rLedger.SetKnown(a, b);
}

bool ObserverSeesForeignBase_(const Faction& rObserver, const Faction& rOther)
{
    const FactionVisibleMap& rVisible = rObserver.GetVisibleMap();
    for (const BaseManager& rBase : rOther.Bases())
    {
        if (rVisible.IsVisible(rBase.GetTile()))
        {
            return true;
        }
    }
    return false;
}

// Fog of war only: Conceal hides the unit sprite/combat target, but a foreign unit on a
// currently visible tile still establishes diplomatic Known. A covert unit does not, since the
// observer cannot tell whose it is.
bool ObserverSeesForeignUnit_(const Faction& rObserver, const Faction& rOther)
{
    const FactionVisibleMap& rVisible = rObserver.GetVisibleMap();
    for (const Unit& rUnit : rOther.GetUnitManager().Units())
    {
        if (rVisible.IsVisible(rUnit.GetTile())
            && IsOwnerKnownTo(rObserver.GetFactionId(), rUnit))
        {
            return true;
        }
    }
    return false;
}

} // namespace

FirstContactResolver::FirstContactResolver(DiplomacyLedger& rLedger,
                                           std::vector<std::unique_ptr<Faction>>& rFactions)
    : m_rLedger(rLedger)
    , m_rFactions(rFactions)
{
}

void FirstContactResolver::ConsiderObserver(Faction& rObserver)
{
    const FactionId_t observerId = rObserver.GetFactionId();

    for (const auto& pOther : m_rFactions)
    {
        if (!pOther || pOther->GetFactionId() == observerId)
        {
            continue;
        }
        if (m_rLedger.AreKnown(observerId, pOther->GetFactionId()))
        {
            continue;
        }
        if (ObserverSeesForeignUnit_(rObserver, *pOther)
            || ObserverSeesForeignBase_(rObserver, *pOther))
        {
            MeetIfNeeded_(m_rLedger, rObserver, *pOther);
        }
    }
}

void FirstContactResolver::ConsiderUnit(const Unit& rSubject)
{
    const FactionId_t subjectFactionId = rSubject.GetFaction().GetFactionId();
    const auto& rTile = rSubject.GetTile();

    for (const auto& pObserver : m_rFactions)
    {
        if (!pObserver || pObserver->GetFactionId() == subjectFactionId)
        {
            continue;
        }
        if (m_rLedger.AreKnown(pObserver->GetFactionId(), subjectFactionId)
            || !IsOwnerKnownTo(pObserver->GetFactionId(), rSubject))
        {
            continue;
        }
        if (pObserver->GetVisibleMap().IsVisible(rTile))
        {
            MeetIfNeeded_(m_rLedger, *pObserver, rSubject.GetFaction());
        }
    }
}

} // namespace ac
