#include "game/visibility/FactionVisibleMap.h"

#include "game/faction/Faction.h"
#include "game/visibility/FactionExploredMap.h"
#include "game/visibility/FactionTileMemory.h"
#include "game/units/UnitManager.h"
#include "game/base/BaseManager.h"
#include "game/map/ImprovementConfigParser.h"
#include "game/map/MapUtils.h"
#include "game/map/TerritoryMap.h"
#include "game/map/Tile.h"
#include "game/map/WorldMap.h"
#include "game/units/Unit.h"
#include "game/effects/ActiveEffect.h"
#include "game/effects/EffectConfig.h"
#include "game/effects/EffectEnums.h"

#include <stdexcept>

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

    ForEachTileInChebyshevRadius(rOrigin, rWorldMap, radius,
        [this, &rExplored](const Tile* pTile, int /*distance*/)
        {
            Mark_(*pTile);
            rExplored.Mark(*pTile);
        });
}

void FactionVisibleMap::RecordVisible_(const WorldMap& rWorldMap, FactionTileMemory& rMemory) const
{
    for (const auto& pOwnedTile : rWorldMap.GetTiles())
    {
        if (pOwnedTile && IsVisible(*pOwnedTile))
        {
            rMemory.Record(*pOwnedTile);
        }
    }
}

void FactionVisibleMap::RevealImprovementSight_(FactionId_t factionId, const WorldMap& rWorldMap,
                                                FactionExploredMap& rExplored)
{
    const TerritoryMap& rTerritory = rWorldMap.GetTerritory();
    for (const auto& pOwnedTile : rWorldMap.GetTiles())
    {
        const Tile* pTile = pOwnedTile.get();
        if (!pTile)
        {
            continue;
        }
        for (const ImprovementConfig_t* pImprovement : pTile->GetImprovements())
        {
            if (!pImprovement || pImprovement->visionRadius <= 0)
            {
                continue;
            }
            if (pImprovement->ownedByTerritory && rTerritory.GetOwner(*pTile) != factionId)
            {
                continue;
            }
            RevealAround_(*pTile, pImprovement->visionRadius, rWorldMap, rExplored);
        }
    }
}

void FactionVisibleMap::RebuildFromSources(const Faction& rFaction, const WorldMap& rWorldMap,
                                           FactionExploredMap& rExplored,
                                           FactionTileMemory& rMemory)
{
    if (!IsSized())
    {
        throw std::runtime_error("FactionVisibleMap::RebuildFromSources: map is unsized");
    }

    RecordVisible_(rWorldMap, rMemory);
    ClearAll();

    for (const Unit& rUnit : rFaction.GetUnitManager().Units())
    {
        RevealAround_(rUnit.GetTile(), ResolveStat(rUnit, StatId_t::Vision), rWorldMap, rExplored);
    }

    for (const BaseManager& rBase : rFaction.Bases())
    {
        RevealAround_(rBase.GetTile(), k_BaseVisionRadius, rWorldMap, rExplored);
    }

    RevealImprovementSight_(rFaction.GetFactionId(), rWorldMap, rExplored);
    RecordVisible_(rWorldMap, rMemory);
}

} // namespace ac
