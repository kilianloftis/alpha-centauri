#include "game/terraform/SurfaceFlipRules.h"

#include "game/CurrentGameState.h"
#include "game/GameState.h"
#include "game/base/BaseManager.h"
#include "game/base/BaseRules.h"
#include "game/map/ImprovementConfigParser.h"
#include "game/map/OccupantCoexistence.h"
#include "game/map/WorldMap.h"
#include "game/movement/MovementRules.h"
#include "game/units/Unit.h"

#include <vector>

namespace ac
{

namespace
{

std::vector<std::string> RemovedImprovements_(const Tile& rTile, bool bNowWater)
{
    std::vector<std::string> removed;
    for (const ImprovementConfig_t* pImprovement : rTile.GetImprovements())
    {
        if (!pImprovement->domain.has_value()
            || DomainMatchesSurface(*pImprovement->domain, bNowWater))
        {
            continue;
        }
        removed.push_back(pImprovement->id);
    }
    return removed;
}

std::vector<Unit*> StrandedUnits_(const Tile& rTile)
{
    std::vector<Unit*> units;
    for (Unit* pUnit : CurrentGameState().GetWorldMap().GetAllUnitsOnTile(rTile))
    {
        if (pUnit && !CanEnterTileTerrain(*pUnit, rTile))
        {
            units.push_back(pUnit);
        }
    }
    return units;
}

} // namespace

SurfaceFlipConsequences_t SurfaceFlipConsequences(const Tile& rTile, bool bNowWater)
{
    SurfaceFlipConsequences_t consequences;
    consequences.removedImprovements = RemovedImprovements_(rTile, bNowWater);
    consequences.strandedUnits = StrandedUnits_(rTile);
    return consequences;
}

SurfaceFlipConsequences_t SurfaceFlipConsequences(const Tile& rTile, bool bNowWater,
                                                  const BaseManager& rBase)
{
    SurfaceFlipConsequences_t consequences;
    consequences.removedImprovements = RemovedImprovements_(rTile, bNowWater);
    if (MayBaseOccupySurface(bNowWater, rBase.MayOccupyWater()))
    {
        return consequences;
    }
    consequences.bRazeBase = true;
    consequences.strandedUnits = StrandedUnits_(rTile);
    return consequences;
}

} // namespace ac
