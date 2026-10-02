#include "game/units/TerritoryEntryEffects.h"

#include "game/Faction.h"
#include "game/GameState.h"
#include "game/faction/DiplomaticTransitionEffects.h"
#include "game/units/Unit.h"
#include "game/units/UnitOrderExecutor.h"

namespace ac
{

void BreakAgreementAndContinue(GameState& rGameState, Unit& rUnit, FactionId_t territoryOwner)
{
    DeclareVendetta(rGameState, rUnit.GetFaction().GetFactionId(), territoryOwner);
    rGameState.GetUnitOrderExecutor().Execute(rUnit);
}

} // namespace ac
