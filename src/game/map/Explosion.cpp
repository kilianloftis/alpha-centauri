#include "game/map/Explosion.h"

#include "game/Faction.h"
#include "game/GameState.h"
#include "game/effects/TileEffectsContext.h"
#include "game/faction/UnitManager.h"
#include "game/faction/base/BaseManager.h"
#include "game/map/ElevationChange.h"
#include "game/map/ImprovementConfigParser.h"
#include "game/map/MapUtils.h"
#include "game/map/Tile.h"
#include "game/map/WorldMap.h"
#include "game/units/Unit.h"

#include <string>
#include <vector>

namespace ac
{
namespace
{

bool DestroyOtherUnits_(WorldMap& rWorldMap, Tile& rTile, const Unit* pSpareUnit)
{
    bool bDestroyed = false;
    for (;;)
    {
        Unit* pVictim = nullptr;
        for (Unit* pUnit : rWorldMap.GetAllUnitsOnTile(rTile))
        {
            if (pUnit && pUnit != pSpareUnit)
            {
                pVictim = pUnit;
                break;
            }
        }
        if (!pVictim)
        {
            return bDestroyed;
        }
        pVictim->GetFaction().GetUnitManager().DestroyUnit(*pVictim);
        bDestroyed = true;
    }
}

bool RazeBaseOnTile_(GameState& rGameState, Tile& rTile)
{
    BaseManager* pBase = rGameState.FindBaseAt(rTile.GetX(), rTile.GetY());
    if (!pBase || pBase->IsRazed())
    {
        return false;
    }
    pBase->GetFaction().RazeBase(*pBase);
    return true;
}

bool RemoveImprovements_(TileEffectsContext& rTileEffects, Tile& rTile)
{
    std::vector<std::string> ids;
    for (const ImprovementConfig_t* pImprovement : rTile.GetImprovements())
    {
        if (pImprovement)
        {
            ids.push_back(pImprovement->id);
        }
    }
    for (const std::string& rId : ids)
    {
        rTileEffects.RemoveOccupantWithEffects(rTile, rId);
    }
    return !ids.empty();
}

} // namespace

ExplosionResult_t ApplyExplosion(Tile& rOrigin, WorldMap& rWorldMap, int radius,
                                 std::mt19937& rRng, GameState& rGameState,
                                 const Unit* pSpareUnit)
{
    ExplosionResult_t result;
    if (radius <= 0)
    {
        return result;
    }
    result.radius = radius;

    std::vector<Tile*> disk;
    ForEachTileInChebyshevRadius(rOrigin, rWorldMap, radius, /*includeOrigin=*/true,
        [&](Tile* pTile, int /*distance*/)
        {
            if (!pTile)
            {
                return;
            }
            disk.push_back(pTile);
            if (DestroyOtherUnits_(rWorldMap, *pTile, pSpareUnit))
            {
                result.bChanged = true;
            }
            if (RazeBaseOnTile_(rGameState, *pTile))
            {
                result.bChanged = true;
            }
            if (RemoveImprovements_(rGameState.GetTileEffects(), *pTile))
            {
                result.bChanged = true;
            }
        });
    result.tiles = static_cast<int>(disk.size());

    if (LowerTilesOneLevel(disk, rWorldMap, rRng, rOrigin.MapRules(),
                           &rGameState.GetTileEffects(), &rGameState))
    {
        result.bChanged = true;
    }
    return result;
}

} // namespace ac
