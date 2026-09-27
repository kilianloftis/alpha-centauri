#include "game/faction/DiplomacyStatusEffects.h"

#include "game/Faction.h"
#include "game/GameDataContext.h"
#include "game/GameState.h"
#include "game/faction/DiplomacyLedger.h"
#include "game/units/EvacuateTerritoryEffects.h"

#include <stdexcept>

namespace ac
{

void ApplyVendetta(GameState& rGameState, FactionId_t a, FactionId_t b)
{
    if (a == b)
    {
        throw std::invalid_argument("ApplyVendetta: a faction cannot declare Vendetta on itself");
    }

    DiplomacyLedger& rLedger = rGameState.GetDiplomacyLedger();
    const bool bWasPact = rLedger.HasPact(a, b);
    rLedger.SetStatus(a, b, DiplomaticStatus_t::Vendetta);
    rLedger.SetKnown(a, b);

    if (!bWasPact)
    {
        return;
    }

    Faction* pA = rGameState.FindFaction(a);
    Faction* pB = rGameState.FindFaction(b);
    if (!pA || !pB)
    {
        return;
    }

    const InteractionGridsConfig_t& rGrids = pA->GetDataContext().interactionGrids;
    EvacuateUnitsFromTerritory(*pA, b, rGameState.GetWorldMap(), rGrids);
    EvacuateUnitsFromTerritory(*pB, a, rGameState.GetWorldMap(), rGrids);
}

} // namespace ac
