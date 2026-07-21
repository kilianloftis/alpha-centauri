#include "game/faction/FirstContactResolver.h"

#include "game/Faction.h"
#include "game/faction/DiplomacyLedger.h"
#include "game/faction/FactionVisibleMap.h"
#include "game/faction/UnitManager.h"
#include "game/faction/base/BaseManager.h"
#include "game/units/Unit.h"

namespace ac
{

namespace
{

void MeetIfNeeded_(DiplomacyLedger& rLedger, FactionId_t a, FactionId_t b)
{
    if (a == b || rLedger.AreKnown(a, b))
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
        if (rVisible.IsVisible(rBase.GetX(), rBase.GetY()))
        {
            return true;
        }
    }
    return false;
}

// Fog of war only: Conceal hides the unit sprite/combat target, but a foreign unit on a
// currently visible tile still establishes diplomatic Known.
bool ObserverSeesForeignUnit_(const Faction& rObserver, const Faction& rOther)
{
    const FactionVisibleMap& rVisible = rObserver.GetVisibleMap();
    for (const Unit& rUnit : rOther.GetUnitManager().Units())
    {
        if (rVisible.IsVisible(rUnit.GetTile()))
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
            MeetIfNeeded_(m_rLedger, observerId, pOther->GetFactionId());
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
        if (m_rLedger.AreKnown(pObserver->GetFactionId(), subjectFactionId))
        {
            continue;
        }
        if (pObserver->GetVisibleMap().IsVisible(rTile))
        {
            MeetIfNeeded_(m_rLedger, pObserver->GetFactionId(), subjectFactionId);
        }
    }
}

} // namespace ac
