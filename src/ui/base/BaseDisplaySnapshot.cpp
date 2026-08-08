#include "ui/base/BaseDisplaySnapshot.h"

#include "game/Faction.h"
#include "game/effects/TileEffectsContext.h"
#include "game/faction/base/BaseManager.h"
#include "game/faction/base/HomeBaseIndex.h"
#include "game/faction/base/population/PopulationManager.h"
#include "game/faction/base/production/ProductionManager.h"
#include "game/faction/base/resources/WorkerAssignmentManager.h"
#include "game/map/WorkedTileIndex.h"
#include "game/map/WorldMap.h"

namespace ac
{

BaseDisplayKey_t ReadBaseDisplayKey(const BaseManager& rBase)
{
    BaseDisplayKey_t key;
    key.effectsVersion = rBase.GetFaction().GetEffectsVersion();
    key.workedTileRevision = rBase.GetTileEffects().GetWorldMap().GetWorkedTiles().GetRevision();
    key.populationRevision = rBase.GetPopulation().GetRevision();
    key.homeUnitRevision = rBase.GetHomeUnits().GetRevision();
    key.pCurrentProduction = rBase.GetProduction().GetCurrentProduction();
    return key;
}

BaseDisplaySnapshot_t BuildBaseDisplaySnapshot(const BaseManager& rBase)
{
    BaseDisplaySnapshot_t snapshot;
    snapshot.key = ReadBaseDisplayKey(rBase);

    snapshot.nutrientProduction = rBase.GetNutrientProduction();
    snapshot.nutrientsRequired = rBase.GetNutrientsRequired();
    snapshot.mineralProduction = rBase.GetMineralProduction();

    const IConstructable* pProduction = rBase.GetProduction().GetCurrentProduction();
    snapshot.bHasProduction = pProduction != nullptr;
    if (pProduction)
    {
        snapshot.productionName = pProduction->GetName();
        snapshot.mineralCost = rBase.GetMineralCost();
    }

    const WorkerAssignmentManager& rAssignments = rBase.GetWorkerAssignments();
    for (const Tile* pTile : rAssignments.GetWorkableTiles())
    {
        if (!pTile)
        {
            continue;
        }

        TileDisplay_t entry;
        // Both predicates: "worked by this base" decides which yield to show (GetWorkedTileYield
        // only resolves against this base's pops, so asking the world-wide question painted a
        // neighbour's tile as worked and then displayed 0 0 0), and "worked by anyone" separates
        // a free tile from one this base cannot take.
        if (rAssignments.IsTileWorkedByThisBase(pTile))
        {
            entry.workState = TileWorkState_t::WorkedByThisBase;
            entry.yield = rBase.GetWorkedTileYield(*pTile);
        }
        else
        {
            entry.workState = rAssignments.IsTileAssigned(pTile) ? TileWorkState_t::WorkedByOther
                                                                : TileWorkState_t::Free;
            entry.yield = rBase.GetPreviewTileYield(*pTile);
        }
        snapshot.tiles.emplace(pTile, entry);
    }

    return snapshot;
}

} // namespace ac
