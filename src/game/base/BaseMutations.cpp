#include "game/base/BaseMutations.h"

#include "game/GameState.h"
#include "game/faction/Faction.h"
#include "game/map/WorldMap.h"
#include "game/units/Unit.h"

#include "game/base/BaseManager.h"
#include "game/effects/TriggeredEffectDispatch.h"

namespace ac
{

BaseManager& FoundBase(GameState& rGameState, Unit& rFounder)
{
    Faction& rFaction = rFounder.GetFaction();
    Tile& rTile = rGameState.GetWorldMap().MutableTile(rFounder.GetTile());
    BaseManager* pBase = rFaction.CreateBase(rGameState.AllocateBaseId(),
                                             rFaction.SuggestBaseName(), &rTile,
                                             rGameState.GetTileEffects(), std::nullopt);

    TriggeredEffectContext_t context(rGameState, *pBase);
    context.pUnit = &rFounder;
    ApplyTriggeredEffects(rFounder.GetDesign().CollectOnFoundEffects(), context);
    return *pBase;
}

} // namespace ac
