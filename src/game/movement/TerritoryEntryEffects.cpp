#include "game/movement/TerritoryEntryEffects.h"

#include "game/faction/Faction.h"
#include "game/GameState.h"
#include "game/diplomacy/DiplomaticTransitionEffects.h"
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
