#include "game/stages/PlayerActions.h"
#include "game/GameState.h"
#include "game/Faction.h"
#include "game/faction/UnitManager.h"
#include "game/units/Unit.h"
#include "game/units/UnitOrder.h"
#include "game/units/UnitOrderExecutor.h"
#include "game/map/ImprovementRegistry.h"
#include "game/map/WorldMap.h"
#include "game/TurnStageRegistrar.h"
#include <variant>

namespace ac
{

namespace { TurnStageRegistrar<PlayerActions> g_registrar("PlayerActions"); }

PlayerActions::PlayerActions(std::shared_ptr<HookContext> pHookContext)
    : PerFactionTurnStage(pHookContext)
{
}

void PlayerActions::ExecuteImpl(GameState& rGameState, Faction& rFaction)
{
    WorldMap& rWorldMap = rGameState.GetWorldMap();
    UnitOrderExecutor& rExecutor = rGameState.GetUnitOrderExecutor();
    const ImprovementRegistry& rImprovements = rGameState.GetTileEffects().GetImprovements();

    for (Unit& rUnit : rFaction.GetUnitManager().Units())
    {
        if (!rUnit.GetOrder().has_value())
        {
            continue;
        }

        // Move orders advance one step at a time until moves run out or progress stops.
        // Other orders run a single Execute (e.g. hold-for-turns countdown).
        if (!std::holds_alternative<MoveOrder_t>(*rUnit.GetOrder()))
        {
            rExecutor.Execute(rUnit, rWorldMap, rImprovements);
            continue;
        }

        while (rUnit.GetOrder().has_value()
               && std::holds_alternative<MoveOrder_t>(*rUnit.GetOrder())
               && rUnit.GetMovesRemaining() > 0)
        {
            const Tile* pTileBefore = &rUnit.GetTile();
            const int movesBefore = rUnit.GetMovesRemaining();
            rExecutor.Execute(rUnit, rWorldMap, rImprovements);
            if (&rUnit.GetTile() == pTileBefore && rUnit.GetMovesRemaining() == movesBefore)
            {
                break; // blocked (ZOC, terrain, no next step, …)
            }
        }
    }
}

} // namespace ac
