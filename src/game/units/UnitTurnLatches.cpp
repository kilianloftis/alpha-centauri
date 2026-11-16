#include "game/units/UnitTurnLatches.h"

#include <cassert>

namespace ac
{

static_assert(static_cast<unsigned>(UnitTurnEvent_t::Count) <= 8,
              "UnitTurnEvent_t needs a wider EventSet_t");

UnitTurnLatches::EventSet_t UnitTurnLatches::Bit_(UnitTurnEvent_t event)
{
    assert(static_cast<unsigned>(event) < static_cast<unsigned>(UnitTurnEvent_t::Count));
    return static_cast<EventSet_t>(1u << static_cast<unsigned>(event));
}

bool UnitTurnLatches::Contains_(EventSet_t set, UnitTurnEvent_t event)
{
    return (set & Bit_(event)) != 0;
}

void UnitTurnLatches::Record(UnitTurnEvent_t event)
{
    m_thisTurn = static_cast<EventSet_t>(m_thisTurn | Bit_(event));
}

bool UnitTurnLatches::HasThisTurn(UnitTurnEvent_t event) const
{
    return Contains_(m_thisTurn, event);
}

bool UnitTurnLatches::HasLastTurn(UnitTurnEvent_t event) const
{
    return Contains_(m_lastTurn, event);
}

void UnitTurnLatches::Advance()
{
    m_lastTurn = m_thisTurn;
    m_thisTurn = 0;
}

} // namespace ac
