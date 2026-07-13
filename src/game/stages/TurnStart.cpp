#include "game/stages/TurnStart.h"
#include "game/Faction.h"
#include "game/GameState.h"
#include "game/TurnStageRegistrar.h"
#include "game/faction/UnitManager.h"
#include "game/units/MovementConstants.h"
#include "game/units/Unit.h"

namespace ac
{

namespace { TurnStageRegistrar<TurnStart> g_registrar("TurnStart"); }

TurnStart::TurnStart(std::shared_ptr<HookContext> pHookContext)
    : GlobalTurnStage(pHookContext)
{
}

void TurnStart::ExecuteImpl(GameState& rGameState)
{
    for (Faction& rFaction : rGameState.Factions())
    {
        for (Unit& rUnit : rFaction.GetUnitManager().Units())
        {
            rUnit.SetMoveFragmentsRemaining(
                rUnit.GetMovementPoints() * MovementConstants_t::k_moveFragmentsPerPoint);
        }
    }
}

} // namespace ac
