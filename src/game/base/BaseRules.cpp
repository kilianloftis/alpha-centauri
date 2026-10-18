#include "game/base/BaseRules.h"

#include "game/economy/WorkerAssignmentManager.h"
#include "game/faction/Faction.h"

#include <algorithm>
#include <vector>

namespace ac
{

bool GrantsHeadquarters(const Faction& rFaction)
{
    return rFaction.GetHeadquarters() == nullptr;
}

bool MayWorkTile(const WorkerAssignmentManager& rAssignments, const Tile& rTile)
{
    const std::vector<const Tile*>& rArea = rAssignments.GetWorkableTiles();
    const bool bInArea = std::find(rArea.begin(), rArea.end(), &rTile) != rArea.end();
    return bInArea && !rAssignments.IsTileAssigned(&rTile);
}

} // namespace ac
