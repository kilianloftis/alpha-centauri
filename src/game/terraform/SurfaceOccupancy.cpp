#include "game/terraform/SurfaceOccupancy.h"

#include "game/faction/Faction.h"
#include "game/base/BaseManager.h"
#include "game/effects/TileEffectsContext.h"
#include "game/map/ImprovementConfigParser.h"
#include "game/map/Tile.h"
#include "game/terraform/TerraformRules.h"
#include "game/CurrentGameState.h"
#include "game/GameState.h"

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

void ReconcileTile_(GameState& rGameState, const SurfaceFlip_t& rFlip)
{
    Tile& rTile = *rFlip.pTile;
    BaseManager* pBase = rGameState.FindBaseAt(rTile.GetX(), rTile.GetY());
    const SurfaceFlipConsequences_t consequences =
        SurfaceFlipConsequences(rTile, rFlip.bNowWater, pBase);
    const std::vector<std::string>& rRemoved = consequences.removedImprovements;
    if (consequences.bRazesBase)
    {
        pBase->GetFaction().RazeBase(*pBase);
    }
    RemoveImprovements_(rGameState.GetTileEffects(), rTile, rRemoved);
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
