#include "game/units/TerritoryEntryEffects.h"

#include "game/Faction.h"
#include "game/GameState.h"
#include "game/faction/DiplomaticTransitionEffects.h"
#include "game/units/Unit.h"
#include "game/units/UnitOrderExecutor.h"

namespace ac
{

void ResolveTerritoryEntry(GameState& rGameState, Unit& rUnit, FactionId_t territoryOwner,
                           bool bBreakAgreement)
{
    if (!bBreakAgreement)
    {
        rUnit.ClearOrder();
        return;
    }
    DeclareVendetta(rGameState, rUnit.GetFaction().GetFactionId(), territoryOwner);
    rGameState.GetUnitOrderExecutor().Execute(rUnit);
}

} // namespace ac
