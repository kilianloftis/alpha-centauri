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

SurfaceFlipConsequences_t SurfaceFlipConsequences(const Tile& rTile, bool bNowWater,
                                                  const BaseManager* pBase)
{
    SurfaceFlipConsequences_t consequences;
    for (const ImprovementConfig_t* pImprovement : rTile.GetImprovements())
    {
        if (!pImprovement->domain.has_value()
            || DomainMatchesSurface(*pImprovement->domain, bNowWater))
        {
            continue;
        }
        consequences.removedImprovements.push_back(pImprovement->id);
    }
    const bool bBaseUntouched =
        pBase && MayBaseOccupySurface(bNowWater, pBase->MayOccupyWater());
    if (bBaseUntouched)
    {
        return consequences;
    }
    if (pBase)
    {
        consequences.bRazeBase = true;
    }
    consequences.strandedUnits = StrandedUnits_(rTile);
    return consequences;
}

} // namespace ac
