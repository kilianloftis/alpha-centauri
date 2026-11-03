#include "game/base/BaseRules.h"

#include "game/CurrentGameState.h"
#include "game/GameState.h"
#include "game/base/BaseManager.h"
#include "game/economy/WorkerAssignmentManager.h"
#include "game/faction/Faction.h"
#include "game/map/RegionCache.h"
#include "game/map/WorldMap.h"

#include <algorithm>
#include <vector>

namespace ac
{

int HeadquartersDistance(const Faction& rFaction, const Tile& rTile)
{
    const BaseManager* pHq = rFaction.GetHeadquarters();
    if (!pHq)
    {
        return rTile.MapRules().missingHeadquartersDistance;
    }
    const int mapWidth = CurrentGameState().GetWorldMap().GetWidth();
    // The binary's vector_dist, which TabletopDiagonalDistance equals on every offset.
    return TabletopDiagonalDistance(rTile, pHq->GetTile(), mapWidth);
}

int RegionBaseCount(const Faction& rFaction, int region)
{
    const RegionCache& rRegions = CurrentGameState().GetWorldMap().GetRegions();
    const auto inRegion = [&](const BaseManager& rBase)
    { return rRegions.RegionOf(rBase.GetTile()) == region; };
    const auto count = std::ranges::count_if(rFaction.Bases(), inRegion);
    return static_cast<int>(count);
}

bool MayWorkTile(const WorkerAssignmentManager& rAssignments, const Tile& rTile)
{
    const std::vector<const Tile*>& rArea = rAssignments.GetWorkableTiles();
    const bool bInArea = std::find(rArea.begin(), rArea.end(), &rTile) != rArea.end();
    return bInArea && !rAssignments.IsTileAssigned(&rTile);
}

} // namespace ac
