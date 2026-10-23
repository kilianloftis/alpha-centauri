#include "game/units/SupplyCrawlMutations.h"

#include "game/base/BaseManager.h"
#include "game/effects/TileEffectsContext.h"
#include "game/map/WorkedTileIndex.h"
#include "game/map/WorldMap.h"
#include "game/units/SupplyCrawlRules.h"
#include "game/units/Unit.h"

#include <utility>

namespace ac
{

bool TryStartSupplyCrawl(Unit& rUnit, StatId_t resource)
{
    if (!CanStartSupplyCrawl(rUnit, resource))
    {
        return false;
    }
    rUnit.ClearOrder();

    // Claim the current tile directly (same WorkedTileIndex as workers; any free tile).
    WorkedTileIndex& rWorkedTiles =
        rUnit.GetHomeBase()->GetTileEffects().GetWorldMap().GetWorkedTiles();
    WorkedTileClaim claim = rWorkedTiles.TryClaim(rUnit.GetTile(), /*bUserAssigned*/ true,
                                                  [&rUnit]() { rUnit.ClearOrder(); });
    if (!claim.GetTile())
    {
        return false;
    }
    rUnit.AdoptSupplyCrawl(std::move(claim), resource);
    return true;
}

} // namespace ac
