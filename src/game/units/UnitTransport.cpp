#include "game/units/UnitTransport.h"

#include "game/map/UnitPositionIndex.h"
#include "game/movement/TransportRules.h"
#include "game/units/Unit.h"

#include <algorithm>
#include <stdexcept>

namespace ac
{

UnitTransport::UnitTransport(Unit& rOwner, UnitPositionIndex& rPositions)
    : m_rOwner(rOwner)
    , m_rPositions(rPositions)
{
}

void UnitTransport::EmbarkInto(Unit& rCarrier)
{
    if (m_pCarrier == &rCarrier)
    {
        return;
    }
    if (&rCarrier == &m_rOwner)
    {
        throw std::invalid_argument("Unit::EmbarkInto: a unit cannot carry itself");
    }
    if (&rCarrier.GetTile() != &m_rOwner.GetTile())
    {
        throw std::invalid_argument("Unit::EmbarkInto: carrier and passenger are not on the "
                                    "same tile");
    }
    if (!CanCarryPassenger(rCarrier, m_rOwner))
    {
        throw std::invalid_argument("Unit::EmbarkInto: carrier cannot take this passenger "
                                    "(capacity, domain, faction, or the carrier is itself cargo)");
    }
    if (m_pCarrier)
    {
        Disembark();
    }
    m_pCarrier = &rCarrier;
    rCarrier.Transport().m_cargo.push_back(&m_rOwner);
    m_rPositions.NoteEmbarked_(m_rOwner);
}

void UnitTransport::Disembark()
{
    if (!m_pCarrier)
    {
        return;
    }
    std::erase(m_pCarrier->Transport().m_cargo, &m_rOwner);
    m_pCarrier = nullptr;
    m_rPositions.NoteDisembarked_(m_rOwner);
}

void UnitTransport::ClearLinks()
{
    if (m_pCarrier)
    {
        std::erase(m_pCarrier->Transport().m_cargo, &m_rOwner);
        m_pCarrier = nullptr;
        // Still listed as cargo in the index until Unregister_ / RemoveFromTile_.
    }
    for (Unit* pPassenger : m_cargo)
    {
        pPassenger->Transport().m_pCarrier = nullptr;
        // Orphaned passenger becomes an independent occupant on the same tile.
        m_rPositions.NoteDisembarked_(*pPassenger);
    }
    m_cargo.clear();
}

} // namespace ac
