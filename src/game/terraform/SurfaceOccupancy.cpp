#include "game/terraform/SurfaceOccupancy.h"

#include "game/faction/Faction.h"
#include "game/base/BaseManager.h"
#include "game/effects/TileEffectsContext.h"
#include "game/map/ImprovementConfigParser.h"
#include "game/map/Tile.h"
#include "game/terraform/TerraformRules.h"
#include "game/CurrentGameState.h"
#include "game/GameState.h"
#include "game/PlayerInteractionQueue.h"
#include "game/units/UnitManager.h"

#include <string>
#include <vector>

namespace ac
{

namespace
{

void RemoveImprovements_(TileEffectsContext& rTileEffects, Tile& rTile,
                         const std::vector<std::string>& rIds)
{
    for (const std::string& rId : rIds)
    {
        rTileEffects.RemoveOccupantWithEffects(rTile, rId);
    }
}

void NoticeSubmergence_(GameState& rGameState, const BaseManager& rBase)
{
    const Faction& rPlayer = rGameState.GetPlayerFaction();
    const bool bOwn = &rBase.GetFaction() == &rPlayer;
    if (!bOwn && !rPlayer.GetVisibleMap().IsVisible(rBase.GetTile()))
    {
        return;
    }
    const std::string whose = bOwn ? "" : rBase.GetFaction().GetDefinition().identity.name + " ";
    const std::string body =
        "The " + whose + "base " + rBase.GetName() + " has been lost beneath the rising sea.";
    EnqueueForPlayer(rGameState, NoticeInteraction_t{std::nullopt, "Base Submerged", body,
                                                     rBase.GetTile().GetCoord()});
}

void DestroyUnits_(const std::vector<Unit*>& rUnits)
{
    for (Unit* pUnit : rUnits)
    {
        pUnit->GetFaction().GetUnitManager().DestroyUnit(*pUnit);
    }
}

void ReconcileTile_(GameState& rGameState, const SurfaceFlip_t& rFlip)
{
    Tile& rTile = *rFlip.pTile;
    BaseManager* pBase = rGameState.FindBaseAt(rTile);
    const SurfaceFlipConsequences_t consequences =
        SurfaceFlipConsequences(rTile, rFlip.bNowWater, pBase);
    bool bBaseStands = pBase != nullptr;
    if (consequences.bRazeBase)
    {
        NoticeSubmergence_(rGameState, *pBase);
        pBase->GetFaction().RazeBase(*pBase);
        bBaseStands = false;
    }
    RemoveImprovements_(rGameState.GetTileEffects(), rTile, consequences.removedImprovements);
    if (!bBaseStands)
    {
        DestroyUnits_(consequences.strandedUnits);
    }
}

} // namespace

void ReconcileSurfaceFlips(GameState& rGameState, const std::vector<SurfaceFlip_t>& rFlips)
{
    RequireCurrentGameState(rGameState);
    for (const SurfaceFlip_t& rFlip : rFlips)
    {
        ReconcileTile_(rGameState, rFlip);
    }
}

} // namespace ac
