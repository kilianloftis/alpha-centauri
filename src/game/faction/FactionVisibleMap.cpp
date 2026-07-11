#include "game/faction/FactionVisibleMap.h"

#include "game/Faction.h"
#include "game/faction/FactionExploredMap.h"
#include "game/faction/UnitManager.h"
#include "game/faction/base/BaseManager.h"
#include "game/map/MapUtils.h"
#include "game/map/Tile.h"
#include "game/map/WorldMap.h"
#include "game/units/Unit.h"

namespace ac
{

namespace
{

// A base sees a Chebyshev radius-2 square (own tile plus two rings).
constexpr int k_BaseVisionRadius = 2;

} // namespace

void FactionVisibleMap::RevealAround_(const Tile& rOrigin, int radius, const WorldMap& rWorldMap,
                                      FactionExploredMap& rExplored)
{
    if (radius < 0)
    {
        return;
    }

    ForEachTileInChebyshevRadius(rOrigin, rWorldMap, radius, /*includeOrigin=*/true,
        [this, &rExplored](const Tile* pTile, int /*distance*/)
        {
            Mark(*pTile);
            rExplored.Mark(*pTile);
        });
}

void FactionVisibleMap::RebuildFromSources(const Faction& rFaction, const WorldMap& rWorldMap,
                                           FactionExploredMap& rExplored)
{
    if (!IsSized())
    {
        return;
    }

    ClearAll();

    for (const Unit& rUnit : rFaction.GetUnitManager().Units())
    {
        RevealAround_(rUnit.GetTile(), rUnit.GetVision(), rWorldMap, rExplored);
    }

    for (const BaseManager& rBase : rFaction.Bases())
    {
        const Tile* pBaseTile = rWorldMap.GetTile(rBase.GetX(), rBase.GetY());
        if (pBaseTile)
        {
            RevealAround_(*pBaseTile, k_BaseVisionRadius, rWorldMap, rExplored);
        }
    }
}

} // namespace ac
