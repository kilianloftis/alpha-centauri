#include "game/base/BaseMutations.h"

#include "game/GameState.h"
#include "game/faction/Faction.h"
#include "game/map/WorldMap.h"
#include "game/units/Unit.h"
#include "game/units/UnitMutations.h"

#include "game/base/BaseManager.h"
#include "game/base/FoundBaseRules.h"
#include "game/production/ProductionManager.h"

namespace ac
{

BaseManager& FoundBase(GameState& rGameState, Unit& rFounder)
{
    Faction& rFaction = rFounder.GetFaction();
    Tile& rTile = rGameState.GetWorldMap().MutableTile(rFounder.GetTile());
    BaseManager* pBase = rFaction.CreateBase(rGameState.AllocateBaseId(),
                                             rFaction.SuggestBaseName(), &rTile,
                                             rGameState.GetTileEffects(), std::nullopt);

    // Before the founder is spent: its StartingMinerals still resolve on the pod.
    ApplyStartingMinerals(*pBase, &rFounder);
    ExpendIfSingleUse(rFounder);
    return *pBase;
}

void ApplyStartingMinerals(BaseManager& rBase, const Unit* pFoundingUnit)
{
    // Founding only; retooling stays free while the turn original is null.
    rBase.GetProduction().SetMineralStockpile(
        ResolveStartingMinerals(rBase, pFoundingUnit));
}

} // namespace ac
